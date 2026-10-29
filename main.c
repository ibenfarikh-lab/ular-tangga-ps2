#include <tamtypes.h>
#include <sifrpc.h>
#include <loadfile.h>
#include <stdio.h>
#include <stdlib.h>
#include <libpad.h>
#include <gsKit.h>
#include <dmaKit.h>

static char padBuf[256] __attribute__((aligned(64)));

typedef struct {
    int posisi_real;      
    int posisi_tampil;    
    int sisa_langkah;     
    int target_tujuan;    
    int status_aksi;      
} PionGame;

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

void get_xy_dari_posisi(int posisi, int *x, int *y) {
    if (posisi < 1) posisi = 1;
    if (posisi > 100) posisi = 100;
    int index = posisi - 1; 
    int baris = index / 10;
    int kolom = index % 10;
    if (baris % 2 != 0) kolom = 9 - kolom;
    *x = 40 + (kolom * 40); 
    *y = 400 - (baris * 40); 
}

void draw_dadu(GSGLOBAL *gsGlobal, int angka_dadu, u64 color_dot) {
    int dx = 480, dy = 160; 
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
    SifInitRpc(0);
    SifLoadFileInit();
    
    printf("[LOG ULAR TANGGA]: Program EE berhasil dipicu.\n");

    GSGLOBAL *gsGlobal = gsKit_init_global();
    gsGlobal->Mode = GS_MODE_NTSC; 
    gsGlobal->Width = 640;
    gsGlobal->Height = 448;
    gsGlobal->Interlace = GS_NONINTERLACED; // Diubah ke non-interlaced agar stabil di HP
    gsGlobal->Field = GS_FIELD;          
    gsGlobal->ZBuffering = GS_SETTING_OFF;     
    gsGlobal->PrimAlphaEnable = GS_SETTING_OFF; 
    
    // OPTIMASI UTAMA UNTUK ANDROID: Turunkan ke PSM_CT16 agar tidak blank di GPU HP
    gsGlobal->PSM = GS_PSM_CT16;
    gsGlobal->PSMZ = GS_PSMZ_16S;
    
    dmaKit_init(D_CTRL_RELE_OFF, D_CTRL_MFD_OFF, D_CTRL_STS_UNSPEC, D_CTRL_STD_OFF, D_CTRL_RCYC_8, 1 << DMA_CHANNEL_GIF);
    dmaKit_chan_init(DMA_CHANNEL_GIF);

    gsKit_init_screen(gsGlobal);
    gsKit_mode_switch(gsGlobal, GS_PERSISTENT);
    
    // Pembersihan VRAM ganda
    gsKit_vram_clear(gsGlobal); 
    gsKit_clear(gsGlobal, GS_SETREG_RGBAQ(25, 35, 50, 0x00, 0));
    gsKit_queue_exec(gsGlobal);
    gsKit_sync_flip(gsGlobal);

    printf("[LOG ULAR TANGGA]: gsKit Screen Berhasil Menyala (16-bit Mode).\n");

    SifLoadModule("rom0:XSIO2MAN", 0, NULL);
    SifLoadModule("rom0:XPADMAN", 0, NULL);
    padInit(0);
    padPortOpen(0, 0, padBuf);

    struct padButtonStatus buttons;
    u32 paddata, old_pad = 0, new_pad;
    
    PionGame p1 = {1, 1, 0, 1, 0};
    PionGame p2 = {1, 1, 0, 1, 0};
    
    int giliran = 1; 
    int angka_dadu_terakhir = 0;
    int pemenang = 0; 
    int pesan_status_tipe = 0; 
    
    unsigned int frame_counter = 0;
    unsigned int frame_langkah_timer = 0;
    int seed_diatur = 0;

    u64 color_bg = GS_SETREG_RGBAQ(25, 35, 50, 0x00, 0);       
    u64 color_petak1 = GS_SETREG_RGBAQ(60, 100, 160, 0x00, 0);
    u64 color_petak2 = GS_SETREG_RGBAQ(40, 70, 120, 0x00, 0);
    u64 color_p1 = GS_SETREG_RGBAQ(255, 60, 60, 0x00, 0);      
    u64 color_p2 = GS_SETREG_RGBAQ(60, 200, 255, 0x00, 0);     
    u64 color_dot = GS_SETREG_RGBAQ(240, 240, 240, 0x00, 0);   
    u64 color_awal = GS_SETREG_RGBAQ(50, 200, 100, 0x00, 0); 
    u64 color_akhir = GS_SETREG_RGBAQ(255, 200, 50, 0x00, 0); 
    u64 color_panel = GS_SETREG_RGBAQ(40, 55, 80, 0x00, 0);

    printf("[LOG ULAR TANGGA]: Masuk game loop.\n");

    while (1) {
        frame_counter++;
        
        PionGame *p_skg = (giliran == 1) ? &p1 : &p2;

        if (pemenang == 0 && p_skg->status_aksi == 0 && (padGetState(0, 0) == 6)) {
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
                    p_skg->sisa_langkah = angka_dadu_terakhir;
                    p_skg->status_aksi = 1; 
                    pesan_status_tipe = 0;
                }
            }
        }

        if (p_skg->status_aksi == 1) {
            frame_langkah_timer++;
            if (frame_langkah_timer >= 15) {
                frame_langkah_timer = 0;
                if (p_skg->sisa_langkah > 0) {
                    p_skg->posisi_tampil++;
                    p_skg->sisa_langkah--;
                    if (p_skg->posisi_tampil > 100) p_skg->posisi_tampil = 100;
                } else {
                    p_skg->posisi_real = p_skg->posisi_tampil;
                    int posisi_setelah_ular = cekUlarTangga(p_skg->posisi_real);
                    
                    if (p_skg->posisi_real == 100) {
                        pemenang = giliran;
                        p_skg->status_aksi = 0;
                    } else if (posisi_setelah_ular != p_skg->posisi_real) {
                        p_skg->target_tujuan = posisi_setelah_ular;
                        pesan_status_tipe = (posisi_setelah_ular > p_skg->posisi_real) ? 1 : 2;
                        p_skg->status_aksi = 2; 
                        frame_langkah_timer = 0;
                    } else {
                        p_skg->status_aksi = 0;
                        giliran = (giliran == 1) ? 2 : 1;
                    }
                }
            }
        } 
        else if (p_skg->status_aksi == 2) {
            frame_langkah_timer++;
            if (frame_langkah_timer >= 45) { 
                frame_langkah_timer = 0;
                p_skg->posisi_real = p_skg->target_tujuan;
                p_skg->posisi_tampil = p_skg->target_tujuan;
                
                if (p_skg->posisi_real == 100) {
                    pemenang = giliran;
                } else {
                    giliran = (giliran == 1) ? 2 : 1;
                }
                p_skg->status_aksi = 0;
                pesan_status_tipe = 0;
            }
        }

        gsKit_vsync_wait();

        if (pemenang != 0) {
            u64 warna_menang = (pemenang == 1) ? color_p1 : color_p2;
            u64 flash_bg = (frame_counter % 20 < 10) ? warna_menang : color_bg;
            gsKit_clear(gsGlobal, flash_bg);
        } else {
            gsKit_clear(gsGlobal, color_bg);
        }

        for (int i = 1; i <= 100; i++) {
            int px, py;
            get_xy_dari_posisi(i, &px, &py);
            u64 warna_petak = (i % 2 == 0) ? color_petak1 : color_petak2;
            if (i == 1) warna_petak = color_awal;
            if (i == 100) warna_petak = color_akhir;
            gsKit_prim_sprite(gsGlobal, px, py, px + 38, py + 38, 2, warna_petak);
        }

        int p1_x, p1_y, p2_x, p2_y;
        get_xy_dari_posisi(p1.posisi_tampil, &p1_x, &p1_y);
        get_xy_dari_posisi(p2.posisi_tampil, &p2_x, &p2_y);

        if (p1.posisi_tampil == p2.posisi_tampil) {
            gsKit_prim_sprite(gsGlobal, p1_x + 2, p1_y + 10, p1_x + 18, p1_y + 30, 1, color_p1); 
            gsKit_prim_sprite(gsGlobal, p2_x + 20, p2_y + 10, p2_x + 36, p2_y + 30, 1, color_p2); 
        } else {
            gsKit_prim_sprite(gsGlobal, p1_x + 8, p1_y + 8, p1_x + 30, p1_y + 30, 1, color_p1); 
            gsKit_prim_sprite(gsGlobal, p2_x + 8, p2_y + 8, p2_x + 30, p2_y + 30, 1, color_p2); 
        }

        if (pemenang == 0) {
            gsKit_prim_sprite(gsGlobal, 460, 40, 600, 280, 2, color_panel);

            u64 warna_giliran = (giliran == 1) ? color_p1 : color_p2;
            gsKit_prim_sprite(gsGlobal, 480, 70, 560, 95, 1, warna_giliran); 
            
            if (angka_dadu_terakhir > 0) {
                draw_dadu(gsGlobal, angka_dadu_terakhir, color_dot);
            }

            gsKit_prim_sprite(gsGlobal, 460, 310, 600, 380, 2, color_panel);
            if (pesan_status_tipe == 1) {
                gsKit_prim_sprite(gsGlobal, 475, 330, 585, 360, 1, color_awal);
            } else if (pesan_status_tipe == 2) {
                gsKit_prim_sprite(gsGlobal, 475, 330, 585, 360, 1, color_p1);
            }
        }

        gsKit_queue_exec(gsGlobal);
        gsKit_sync_flip(gsGlobal);
    }
    return 0;
}
