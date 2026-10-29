#include <tamtypes.h>
#include <sifrpc.h>
#include <loadfile.h>
#include <stdio.h>
#include <stdlib.h>
#include <libpad.h>
#include <gsKit.h>
#include <dmaKit.h>
#include <math.h>
#include <string.h>

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
    int dx = 536, dy = 196; 
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


// Menggambar elips penuh memakai triangle fan gsKit
void draw_elips(GSGLOBAL *gsGlobal, float cx, float cy, float rx, float ry, int z, u64 color) {
    float fan[2 * 26];
    fan[0] = cx; fan[1] = cy;
    for (int i = 0; i <= 24; i++) {
        float a = 6.2831853f * (float)i / 24.0f;
        fan[2 + 2*i] = cx + rx * cosf(a);
        fan[3 + 2*i] = cy + ry * sinf(a);
    }
    gsKit_prim_triangle_fan(gsGlobal, fan, 26, z, color);
}

// Elips gradasi (gouraud): tengah terang -> tepi gelap, kesan glossy membulat
void draw_grad(GSGLOBAL *gsGlobal, float cx, float cy, float rx, float ry, int z, u64 cTengah, u64 cTepi) {
    for (int i = 0; i < 20; i++) {
        float a1 = 6.2831853f * (float)i / 20.0f;
        float a2 = 6.2831853f * (float)(i + 1) / 20.0f;
        gsKit_prim_triangle_gouraud_3d(gsGlobal,
            cx, cy, z,
            cx + rx * cosf(a1), cy + ry * sinf(a1), z,
            cx + rx * cosf(a2), cy + ry * sinf(a2), z,
            cTengah, cTepi, cTepi);
    }
}

// Bidak pion glossy: bayangan + alas + badan + kepala gradasi gouraud ber-ring gelap
void draw_bidak(GSGLOBAL *gsGlobal, float cx, float cy, float s, u64 cTerang, u64 cDasar, u64 cGelap) {
    u64 gelap = GS_SETREG_RGBAQ(40, 26, 18, 0x00, 0);
    u64 putih = GS_SETREG_RGBAQ(255, 255, 255, 0x00, 0);
    u64 bayang = GS_SETREG_RGBAQ(58, 46, 28, 0x00, 0);
    draw_elips(gsGlobal, cx + 2.0f*s, cy + 13.0f*s, 12.0f*s, 4.5f*s, 1, bayang);
    draw_elips(gsGlobal, cx, cy + 10.0f*s, 12.5f*s, 6.0f*s, 1, gelap);
    draw_grad(gsGlobal, cx, cy + 10.0f*s, 11.0f*s, 5.2f*s, 1, cTerang, cGelap);
    draw_elips(gsGlobal, cx, cy + 3.0f*s, 10.0f*s, 10.0f*s, 1, gelap);
    draw_grad(gsGlobal, cx, cy + 3.0f*s, 8.8f*s, 8.8f*s, 1, cTerang, cDasar);
    draw_elips(gsGlobal, cx, cy - 8.0f*s, 7.0f*s, 7.0f*s, 1, gelap);
    draw_grad(gsGlobal, cx, cy - 8.0f*s, 5.8f*s, 5.8f*s, 1, cTerang, cDasar);
    draw_elips(gsGlobal, cx - 2.0f*s, cy - 10.0f*s, 1.8f*s, 1.8f*s, 1, putih);
}

// Huruf bitmap 5x7 (bit 4..0 = kiri..kanan)
static const unsigned char HURUF_SPASI[7] = {0,0,0,0,0,0,0};
static const unsigned char HURUF_A[7] = {0x0E,0x11,0x11,0x1F,0x11,0x11,0x11};
static const unsigned char HURUF_E[7] = {0x1F,0x10,0x10,0x1E,0x10,0x10,0x1F};
static const unsigned char HURUF_G[7] = {0x0E,0x11,0x10,0x17,0x11,0x11,0x0F};
static const unsigned char HURUF_I[7] = {0x1F,0x04,0x04,0x04,0x04,0x04,0x1F};
static const unsigned char HURUF_K[7] = {0x11,0x12,0x14,0x18,0x14,0x12,0x11};
static const unsigned char HURUF_L[7] = {0x10,0x10,0x10,0x10,0x10,0x10,0x1F};
static const unsigned char HURUF_M[7] = {0x11,0x1B,0x15,0x15,0x11,0x11,0x11};
static const unsigned char HURUF_N[7] = {0x11,0x19,0x15,0x13,0x11,0x11,0x11};
static const unsigned char HURUF_O[7] = {0x0E,0x11,0x11,0x11,0x11,0x11,0x0E};
static const unsigned char HURUF_P[7] = {0x1E,0x11,0x11,0x1E,0x10,0x10,0x10};
static const unsigned char HURUF_R[7] = {0x1E,0x11,0x11,0x1E,0x14,0x12,0x11};
static const unsigned char HURUF_T[7] = {0x1F,0x04,0x04,0x04,0x04,0x04,0x04};
static const unsigned char HURUF_U[7] = {0x11,0x11,0x11,0x11,0x11,0x11,0x0E};
static const unsigned char HURUF_X[7] = {0x11,0x11,0x0A,0x04,0x0A,0x11,0x11};
static const unsigned char HURUF_1[7] = {0x04,0x0C,0x04,0x04,0x04,0x04,0x0E};
static const unsigned char HURUF_2[7] = {0x0E,0x11,0x01,0x06,0x08,0x10,0x1F};
static const unsigned char HURUF_TANDA[7] = {0x04,0x04,0x04,0x04,0x04,0x00,0x04};
static const unsigned char HURUF_J[7] = {0x07,0x02,0x02,0x02,0x02,0x12,0x0C};
static const unsigned char HURUF_S[7] = {0x0F,0x10,0x10,0x0E,0x01,0x01,0x1E};
static const unsigned char HURUF_TANYA[7] = {0x0E,0x11,0x01,0x06,0x04,0x00,0x04};
static const unsigned char HURUF_0[7] = {0x0E,0x11,0x13,0x15,0x19,0x11,0x0E};
static const unsigned char HURUF_3[7] = {0x0E,0x11,0x01,0x06,0x01,0x11,0x0E};
static const unsigned char HURUF_4[7] = {0x06,0x0A,0x12,0x1F,0x02,0x02,0x02};
static const unsigned char HURUF_5[7] = {0x1F,0x10,0x1E,0x01,0x01,0x11,0x0E};
static const unsigned char HURUF_6[7] = {0x06,0x08,0x10,0x1E,0x11,0x11,0x0E};
static const unsigned char HURUF_7[7] = {0x1F,0x01,0x02,0x04,0x08,0x08,0x08};
static const unsigned char HURUF_8[7] = {0x0E,0x11,0x11,0x0E,0x11,0x11,0x0E};
static const unsigned char HURUF_9[7] = {0x0E,0x11,0x11,0x0F,0x01,0x02,0x0C};

static const unsigned char *glyph_for(char c) {
    switch (c) {
        case 'A': return HURUF_A; case 'E': return HURUF_E;
        case 'G': return HURUF_G; case 'I': return HURUF_I;
        case 'K': return HURUF_K; case 'L': return HURUF_L;
        case 'M': return HURUF_M; case 'N': return HURUF_N;
        case 'O': return HURUF_O; case 'P': return HURUF_P;
        case 'R': return HURUF_R; case 'T': return HURUF_T;
        case 'U': return HURUF_U; case 'X': return HURUF_X;
        case '1': return HURUF_1; case '2': return HURUF_2;
        case '!': return HURUF_TANDA;
        case 'J': return HURUF_J; case 'S': return HURUF_S;
        case '?': return HURUF_TANYA;
        case '0': return HURUF_0; case '3': return HURUF_3;
        case '4': return HURUF_4; case '5': return HURUF_5;
        case '6': return HURUF_6; case '7': return HURUF_7;
        case '8': return HURUF_8; case '9': return HURUF_9;
        default:  return HURUF_SPASI;
    }
}

// Menggambar teks huruf bitmap; tiap piksel huruf = satu sprite kecil
void draw_text(GSGLOBAL *gsGlobal, int x, int y, int skala, u64 warna, const char *teks) {
    int cx = x;
    for (const char *p = teks; *p; p++) {
        const unsigned char *g = glyph_for(*p);
        for (int r = 0; r < 7; r++)
            for (int c = 0; c < 5; c++)
                if (g[r] & (0x10 >> c))
                    gsKit_prim_sprite(gsGlobal, cx + c*skala, y + r*skala,
                                      cx + (c+1)*skala, y + (r+1)*skala, 1, warna);
        cx += 6 * skala;
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
    char pesan1[24] = "SIAP?";
    char pesan2[24] = "";
    
    unsigned int frame_counter = 0;
    int seed_diatur = 0;

    // Background = hijau padang lukisan tekstur (contoh piksel board512.png)
    u64 color_bg = GS_SETREG_RGBAQ(52, 131, 70, 0x00, 0);
    u64 color_p1 = GS_SETREG_RGBAQ(255, 50, 50, 0x00, 0);   
    u64 color_p2 = GS_SETREG_RGBAQ(50, 200, 255, 0x00, 0);  
    u64 color_dot = GS_SETREG_RGBAQ(10, 10, 10, 0x00, 0);
    u64 color_p1_terang = GS_SETREG_RGBAQ(255, 130, 110, 0x00, 0);
    u64 color_p1_gelap = GS_SETREG_RGBAQ(170, 26, 22, 0x00, 0);
    u64 color_p2_terang = GS_SETREG_RGBAQ(140, 210, 255, 0x00, 0);
    u64 color_p2_gelap = GS_SETREG_RGBAQ(24, 88, 180, 0x00, 0);   

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
                    
                    int tujuan = *posisi_skg + angka_dadu_terakhir;
                    if (tujuan >= 100) {
                        *posisi_skg = 100;
                        pemenang = giliran;
                        strcpy(pesan1, "MENANG!");
                        pesan2[0] = 0;
                    } else {
                        int hasil = cekUlarTangga(tujuan);
                        if (hasil > tujuan) {
                            strcpy(pesan1, "TANGGA!");
                            sprintf(pesan2, "NAIK %d", hasil);
                        } else if (hasil < tujuan) {
                            strcpy(pesan1, "ULAR!");
                            sprintf(pesan2, "TURUN %d", hasil);
                        } else {
                            sprintf(pesan1, "%s MAJU %d", (giliran == 1) ? "P1" : "P2", angka_dadu_terakhir);
                            pesan2[0] = 0;
                        }
                        *posisi_skg = hasil;
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

        // Gambar Bidak Pemain (bentuk pion)
        int p1_x, p1_y, p2_x, p2_y;
        get_xy_dari_posisi(p1_pos, &p1_x, &p1_y);
        get_xy_dari_posisi(p2_pos, &p2_x, &p2_y);

        if (p1_pos == p2_pos) {
            draw_bidak(gsGlobal, p1_x + 13, p1_y + 24, 0.72f, color_p1_terang, color_p1, color_p1_gelap);
            draw_bidak(gsGlobal, p2_x + 31, p2_y + 24, 0.72f, color_p2_terang, color_p2, color_p2_gelap);
        } else {
            draw_bidak(gsGlobal, p1_x + 22, p1_y + 22, 1.0f, color_p1_terang, color_p1, color_p1_gelap);
            draw_bidak(gsGlobal, p2_x + 22, p2_y + 22, 1.0f, color_p2_terang, color_p2, color_p2_gelap);
        }

        // Gambar Panel Keterangan di strip kanan (judul, skor, giliran, dadu, petunjuk, banner)
        {
            u64 color_teks = GS_SETREG_RGBAQ(255, 240, 200, 0x00, 0);
            char skor[16];
            draw_text(gsGlobal, 553, 10, 2, color_teks, "ULAR");
            draw_text(gsGlobal, 541, 28, 2, color_teks, "TANGGA");
            gsKit_prim_sprite(gsGlobal, 518, 50, 534, 66, 2, color_p1);
            draw_text(gsGlobal, 540, 51, 2, color_teks, "P1");
            sprintf(skor, "%d", p1_pos);
            draw_text(gsGlobal, 634 - ((int)strlen(skor) * 12 - 2), 51, 2, color_p1, skor);
            gsKit_prim_sprite(gsGlobal, 518, 72, 534, 88, 2, color_p2);
            draw_text(gsGlobal, 540, 73, 2, color_teks, "P2");
            sprintf(skor, "%d", p2_pos);
            draw_text(gsGlobal, 634 - ((int)strlen(skor) * 12 - 2), 73, 2, color_p2, skor);
            if (pemenang == 0) {
                u64 warna_giliran = (giliran == 1) ? color_p1 : color_p2;
                draw_text(gsGlobal, 535, 100, 2, color_teks, "GILIRAN");
                gsKit_prim_sprite(gsGlobal, 536, 122, 616, 142, 2, warna_giliran);
                draw_text(gsGlobal, 559, 150, 3, warna_giliran, (giliran == 1) ? "P1" : "P2");
                if (angka_dadu_terakhir > 0) draw_dadu(gsGlobal, angka_dadu_terakhir, color_dot);
                draw_text(gsGlobal, 535, 288, 2, color_teks, (giliran == 1) ? "TEKAN X" : "TEKAN O");
                if (pesan1[0]) draw_text(gsGlobal, 576 - ((int)strlen(pesan1) * 12 - 2) / 2, 330, 2, color_teks, pesan1);
                if (pesan2[0]) draw_text(gsGlobal, 576 - ((int)strlen(pesan2) * 12 - 2) / 2, 350, 2, color_teks, pesan2);
            } else {
                u64 warna_menang_teks = (pemenang == 1) ? color_p1 : color_p2;
                draw_text(gsGlobal, 559, 150, 3, warna_menang_teks, (pemenang == 1) ? "P1" : "P2");
                draw_text(gsGlobal, 535, 196, 2, color_teks, "MENANG!");
            }
        }

        // Eksekusi antrean render dan tukar buffer gambar
        gsKit_queue_exec(gsGlobal);
        gsKit_sync_flip(gsGlobal);
    }
    return 0;
}
