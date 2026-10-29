#include <tamtypes.h>
#include <sifrpc.h>
#include <loadfile.h>
#include <stdio.h>
#include <stdlib.h>
#include <libpad.h>
#include <gsKit.h>
#include <dmaKit.h>

#include "board_data.h"

static char padBuf[256] __attribute__((aligned(64)));

// 16 Entri Ular Tangga (Sudah dirapikan strukturnya agar compiler tidak protes)
int cekUlarTangga(int posisi) {
    if (posisi == 4)  return 14;   
    if (posisi == 9)  return 31;
    if (posisi == 20) return 38;  
    if (posisi == 28) return 84;
    if (posisi == 40) return 59;  
    if (posisi == 51) return 67;
    if (posisi == 63) return 81;  
    if (posisi == 71) return 91;
    if (posisi == 17) return 7;   
    if (posisi == 54) return 34;
    if (posisi == 62) return 19;  
    if (posisi == 64) return 60;
    if (posisi == 87) return 24;  
    if (posisi == 93) return 73;
    if (posisi == 95) return 75;  
    if (posisi == 99) return 78;
    return posisi;
}

// Konversi posisi 1-100 ke koordinat X,Y layar
void get_xy_dari_posisi(int posisi, int *x, int *y) {
    int index = posisi - 1; 
    int baris = index / 10;
    int kolom = index % 10;
    if (baris % 2 != 0) kolom = 9 - kolom;
    *x = 32 + (kolom * 89) / 2;
    *y = 32 + ((9 - baris) * 89) / 2;
}

// Fungsi menggambar dadu visual
void draw_dadu(GSGLOBAL *gsGlobal, int angka_dadu, u64 color_dot) {
    int dx = 480, dy = 200; 
    gsKit_prim_sprite(gsGlobal, dx, dy, dx + 80, dy + 80, 2, GS_SETREG_RGBAQ(220, 220, 220, 0x80, 0));

    if (angka_dadu == 1 || angka_dadu == 3 || angka_dadu == 5)
        gsKit_prim_sprite(gsGlobal, dx+35, dy+35, dx+45, dy+45, 1, color_dot);
    if (angka_dadu >= 2) {
        gsKit_prim_sprite(gsGlobal, dx+15, dy+15, dx+25, dy+25, 1, color_dot);
        gsKit_prim_sprite(gsGlobal, dx+55, dy+55, dx+65, dy+65, 1, color_dot);
    }
    if (angka_dadu >= 4) {
        gsKit_prim_sprite(gsGlobal, dx+55, dy+15, dx+65, dy+25, 1, color_dot);
        gsKit_prim_sprite(gsGlobal, dx+15, dy+55, dx+25, dy+65, 1, color_dot);
    }
    if (angka_dadu == 6) {
        gsKit_prim_sprite(gsGlobal, dx+15, dy+35, dx+25, dy+45, 1, color_dot);
        gsKit_prim_sprite(gsGlobal, dx+55, dy+35, dx+65, dy+45, 1, color_dot);
    }
}

int main(int argc, char *argv[]) {
    // 1. Inisialisasi RPC Paling Sederhana
    SifInitRpc(0);
    SifLoadFileInit();
    
    // Kirim log awal untuk memastikan program mulai dieksekusi oleh EE
    printf("[LOG ULAR TANGGA]: Program EE berhasil dipicu.\n");

    // 2. Alokasikan struktur dasar gsKit Global
    GSGLOBAL *gsGlobal = gsKit_init_global();
    
    // PERBAIKAN UTAMA: Menggunakan gsKit_check_rom() yang valid di SDK
    gsGlobal->Mode = gsKit_check_rom();
    
    // Atur resolusi adaptif berdasarkan mode yang terdeteksi
    if (gsGlobal->Mode == GS_MODE_PAL) {
        gsGlobal->Height = 512;
    } else {
        gsGlobal->Height = 448;
    }
    gsGlobal->Width = 640;
    gsGlobal->Interlace = GS_INTERLACED; 
    gsGlobal->Field = GS_FIELD;          
    gsGlobal->ZBuffering = GS_SETTING_OFF;     
    gsGlobal->PrimAlphaEnable = GS_SETTING_OFF; 
    gsGlobal->PSM = GS_PSM_CT32;
    gsGlobal->PSMZ = GS_PSMZ_16S;
    
    // 3. Hidupkan Jalur Kerja DMA Grafis (GIF)
    dmaKit_init(D_CTRL_RELE_OFF, D_CTRL_MFD_OFF, D_CTRL_STS_UNSPEC, D_CTRL_STD_OFF, D_CTRL_RCYC_8, 1 << DMA_CHANNEL_GIF);
    dmaKit_chan_init(DMA_CHANNEL_GIF);

    // Hidupkan Output Layar
    gsKit_init_screen(gsGlobal);
    gsKit_mode_switch(gsGlobal, GS_PERSISTENT);
    gsKit_vram_clear(gsGlobal); 
    printf("[LOG ULAR TANGGA]: gsKit Screen Berhasil Menyala.\n");

    // 4. Pemuatan Modul Input Stik PS2
    SifLoadModule("rom0:XSIO2MAN", 0, NULL);
    SifLoadModule("rom0:XPADMAN", 0, NULL);
    padInit(0);
    padPortOpen(0, 0, padBuf);

    // Tekstur papan ilustrasi (terbukti tampil di PCSX2 29 Okt 23:28):
    // alokasi VRAM + unggah, pola loader gsToolkit seperti tes tekstur.
    GSTEXTURE boardTex;
    boardTex.Width = 512;
    boardTex.Height = 512;
    boardTex.PSM = GS_PSM_CT32;
    boardTex.Filter = GS_FILTER_LINEAR;
    boardTex.Mem = (u32 *)board_pixels;
    boardTex.Clut = NULL;
    boardTex.VramClut = 0;
    boardTex.Delayed = 0;
    boardTex.Vram = gsKit_vram_alloc(gsGlobal,
                        gsKit_texture_size(512, 512, GS_PSM_CT32),
                        GSKIT_ALLOC_USERBUFFER);
    if (boardTex.Vram == GSKIT_ALLOC_ERROR) {
        while (1) {
            gsKit_vsync_wait();
            gsKit_clear(gsGlobal, GS_SETREG_RGBAQ(255, 0, 0, 0x00, 0));
            gsKit_queue_exec(gsGlobal);
            gsKit_sync_flip(gsGlobal);
        }
    }
    gsKit_texture_upload(gsGlobal, &boardTex);
    printf("[LOG ULAR TANGGA]: Tekstur papan terunggah.\n");

    // Variabel Game
    struct padButtonStatus buttons;
    u32 paddata, old_pad = 0, new_pad;
    
    int p1_pos = 1, p2_pos = 1;
    int giliran = 1; 
    int angka_dadu_terakhir = 0;
    int pemenang = 0; 
    
    unsigned int frame_counter = 0;
    int seed_diatur = 0;

    // Background = hijau padang lukisan tekstur (contoh piksel board512.png)
    u64 color_bg = GS_SETREG_RGBAQ(52, 131, 70, 0x00, 0);
    u64 color_p1 = GS_SETREG_RGBAQ(255, 50, 50, 0x00, 0);   
    u64 color_p2 = GS_SETREG_RGBAQ(50, 200, 255, 0x00, 0);  
    u64 color_dot = GS_SETREG_RGBAQ(10, 10, 10, 0x00, 0);   

    printf("[LOG ULAR TANGGA]: Memasuki putaran game loop utama.\n");

    // Main Game Loop
    while (1) {
        frame_counter++;
        
        // Baca Input Stik jika status port siap (State 6 = Ready)
        if (pemenang == 0 && (padGetState(0, 0) == 6)) {
            int ret = padRead(0, 0, &buttons);
            if (ret != 0) {
                paddata = 0xffff ^ buttons.btns;
                new_pad = paddata & ~old_pad;
                old_pad = paddata;

                int tombol_ditekan = 0;
                if ((giliran == 1) && (new_pad & PAD_CROSS)) tombol_ditekan = 1;
                else if ((giliran == 2) && (new_pad & PAD_CIRCLE)) tombol_ditekan = 1;

                if (tombol_ditekan) {
                    if (!seed_diatur) { srand(frame_counter); seed_diatur = 1; }
                    
                    angka_dadu_terakhir = (rand() % 6) + 1;
                    int *posisi_skg = (giliran == 1) ? &p1_pos : &p2_pos;
                    
                    *posisi_skg += angka_dadu_terakhir;
                    
                    if (*posisi_skg >= 100) {
                        *posisi_skg = 100;
                        pemenang = giliran;
                    } else {
                        *posisi_skg = cekUlarTangga(*posisi_skg);
                        giliran = (giliran == 1) ? 2 : 1; 
                    }
                }
            }
        }

        // Tunggu refresh layar TV (V-Sync)
        gsKit_vsync_wait();

        // Render Background game
        if (pemenang != 0) {
            u64 warna_menang = (pemenang == 1) ? color_p1 : color_p2;
            u64 flash_bg = (frame_counter % 20 < 10) ? warna_menang : color_bg;
            gsKit_clear(gsGlobal, flash_bg);
        } else {
            gsKit_clear(gsGlobal, color_bg);
        }

        // Gambar papan ilustrasi (tekstur) - bidak & panel digambar sesudah ini
        gsKit_prim_sprite_texture(gsGlobal, &boardTex,
                                  0, 0, 0, 0,
                                  512, 512, 512, 512,
                                  3, GS_SETREG_RGBAQ(0x80, 0x80, 0x80, 0x00, 0));

        // Gambar Pion Pemain
        int p1_x, p1_y, p2_x, p2_y;
        get_xy_dari_posisi(p1_pos, &p1_x, &p1_y);
        get_xy_dari_posisi(p2_pos, &p2_x, &p2_y);

        if (p1_pos == p2_pos) {
            gsKit_prim_sprite(gsGlobal, p1_x + 2, p1_y + 10, p1_x + 18, p1_y + 30, 1, color_p1); 
            gsKit_prim_sprite(gsGlobal, p2_x + 20, p2_y + 10, p2_x + 36, p2_y + 30, 1, color_p2); 
        } else {
            gsKit_prim_sprite(gsGlobal, p1_x + 8, p1_y + 8, p1_x + 30, p1_y + 30, 1, color_p1); 
            gsKit_prim_sprite(gsGlobal, p2_x + 8, p2_y + 8, p2_x + 30, p2_y + 30, 1, color_p2); 
        }

        // Gambar Panel Dadu & Giliran
        if (pemenang == 0) {
            u64 warna_giliran = (giliran == 1) ? color_p1 : color_p2;
            gsKit_prim_sprite(gsGlobal, 480, 80, 560, 100, 2, warna_giliran); 
            if (angka_dadu_terakhir > 0) draw_dadu(gsGlobal, angka_dadu_terakhir, color_dot);
        }

        // Eksekusi antrean render dan tukar buffer gambar
        gsKit_queue_exec(gsGlobal);
        gsKit_sync_flip(gsGlobal);
    }
    return 0;
}
