#include <tamtypes.h>
#include <sifrpc.h>
#include <loadfile.h>
#include <stdio.h>
#include <stdlib.h>
#include <libpad.h>
#include <kernel.h>
#include <iopcontrol.h>
#include <gsKit.h>
#include <dmaKit.h>
#include <math.h>
#include <string.h>

#include "board_data.h"
#include "judul_data.h"
#include "set_data.h"

static char padBuf[256] __attribute__((aligned(64)));

// Mesin status layar
#define LAYAR_JUDUL 0
#define LAYAR_SET   1
#define LAYAR_GAME  2

// Baris menu judul (koord layar 640x448, sesuai kanvas judul baru)
static const int MENU_Y[3] = {332, 369, 406};
// Baris menu pengaturan
static const int SET_Y[2] = {196, 262};

// Tabel rute (struct Jump + daftar_ular_tangga) kini tinggal di board_data.h.
// Fungsi pengecekan posisi menggunakan pencarian tabel (Lookup Table)
int cekUlarTangga(int posisi) {
    int jumlah_entri = sizeof(daftar_ular_tangga) / sizeof(daftar_ular_tangga[0]);

    for (int i = 0; i < jumlah_entri; i++) {
        if (daftar_ular_tangga[i].asal == posisi) {
            return daftar_ular_tangga[i].tujuan;
        }
    }

    // Jika tidak menginjak kepala ular atau tangga, kembalikan posisi normal
    return posisi;
}

// Konversi posisi 1-100 ke koordinat layar (papan kanvas baru:
// asal dalam 152,64, rentang 335 untuk 10 petak)
void get_xy_dari_posisi(int posisi, int *x, int *y) {
    int index = posisi - 1;
    int baris = index / 10;
    int kolom = index % 10;
    if (baris % 2 != 0) kolom = 9 - kolom;
    *x = 152 + (kolom * 335) / 10;
    *y = 64 + ((9 - baris) * 335) / 10;
}

// Titik dadu di atas dadu polos kanvas (kotak 64x64 di 36,200)
void draw_dadu(GSGLOBAL *gsGlobal, int angka_dadu, u64 color_dot) {
    int dx = 36, dy = 200;
    if (angka_dadu == 1 || angka_dadu == 3 || angka_dadu == 5)
        gsKit_prim_sprite(gsGlobal, dx+28, dy+28, dx+36, dy+36, 1, color_dot);
    if (angka_dadu >= 2) {
        gsKit_prim_sprite(gsGlobal, dx+12, dy+12, dx+20, dy+20, 1, color_dot);
        gsKit_prim_sprite(gsGlobal, dx+44, dy+44, dx+52, dy+52, 1, color_dot);
    }
    if (angka_dadu >= 4) {
        gsKit_prim_sprite(gsGlobal, dx+44, dy+12, dx+52, dy+20, 1, color_dot);
        gsKit_prim_sprite(gsGlobal, dx+12, dy+44, dx+20, dy+52, 1, color_dot);
    }
    if (angka_dadu == 6) {
        gsKit_prim_sprite(gsGlobal, dx+12, dy+28, dx+20, dy+36, 1, color_dot);
        gsKit_prim_sprite(gsGlobal, dx+44, dy+28, dx+52, dy+36, 1, color_dot);
    }
}

// Elips penuh memakai triangle fan gsKit
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

// Elips gradasi (gouraud): tengah terang -> tepi gelap
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

// Bidak pion glossy
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

// Bingkai pilihan menu (4 sprite garis, tebal 3)
void draw_bingkai(GSGLOBAL *gsGlobal, int x1, int y1, int x2, int y2, u64 warna) {
    gsKit_prim_sprite(gsGlobal, x1, y1, x2, y1 + 3, 2, warna);
    gsKit_prim_sprite(gsGlobal, x1, y2 - 3, x2, y2, 2, warna);
    gsKit_prim_sprite(gsGlobal, x1, y1, x1 + 3, y2, 2, warna);
    gsKit_prim_sprite(gsGlobal, x2 - 3, y1, x2, y2, 2, warna);
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
static const unsigned char HURUF_D[7] = {0x1C,0x12,0x11,0x11,0x11,0x12,0x1C};
static const unsigned char HURUF_MINUS[7] = {0x00,0x00,0x00,0x1F,0x00,0x00,0x00};

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
        case 'D': return HURUF_D; case '-': return HURUF_MINUS;
        default:  return HURUF_SPASI;
    }
}

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

// Satu slot tekstur VRAM dipakai bergantian oleh semua layar
static GSTEXTURE texLayar;
static void pakai_tekstur(GSGLOBAL *gsGlobal, const unsigned char *piksel, int w, int h) {
    texLayar.Width = w;
    texLayar.Height = h;
    texLayar.Mem = (u32 *)piksel;
    gsKit_texture_upload(gsGlobal, &texLayar);
}

int main(int argc, char *argv[]) {
    // Reset IOP lebih dulu: game diluncurkan dari wOPL/uLaunchELF yang sudah
    // memuat driver stik sendiri; tanpa reset, pemuatan modul stik kita
    // tabrakan dan stik mati total di konsol asli (di PCSX2 tidak terasa
    // karena IOP bersih). Pola yang sama dipakai uLaunchELF (Init_Reset_IOP=1).
    SifInitRpc(0);
    SifIopReset("", 0);
    while (!SifIopSync()) ;
    SifInitRpc(0);
    SifLoadFileInit();
    printf("[LOG ULAR TANGGA]: Program EE berhasil dipicu (IOP sudah direset).\n");

    GSGLOBAL *gsGlobal = gsKit_init_global();
    // Standar tampilan dikunci user: NTSC 640x448 (480i, 4:3)
    gsGlobal->Mode = GS_MODE_NTSC;
    gsGlobal->Width = 640;
    gsGlobal->Height = 448;
    gsGlobal->Interlace = GS_INTERLACED;
    gsGlobal->Field = GS_FIELD;
    gsGlobal->ZBuffering = GS_SETTING_OFF;
    gsGlobal->PrimAlphaEnable = GS_SETTING_OFF;
    gsGlobal->PSM = GS_PSM_CT32;
    gsGlobal->PSMZ = GS_PSMZ_16S;

    dmaKit_init(D_CTRL_RELE_OFF, D_CTRL_MFD_OFF, D_CTRL_STS_UNSPEC, D_CTRL_STD_OFF, D_CTRL_RCYC_8, 1 << DMA_CHANNEL_GIF);
    dmaKit_chan_init(DMA_CHANNEL_GIF);

    gsKit_init_screen(gsGlobal);
    gsKit_mode_switch(gsGlobal, GS_PERSISTENT);
    gsKit_vram_clear(gsGlobal);
    printf("[LOG ULAR TANGGA]: gsKit Screen Berhasil Menyala (NTSC 640x448).\n");

    SifLoadModule("rom0:XSIO2MAN", 0, NULL);
    SifLoadModule("rom0:XPADMAN", 0, NULL);
    padInit(0);
    padPortOpen(0, 0, padBuf);
    padSetMainMode(0, 0, 1, 3); // mode analog: lampu stik menyala, tombol terbaca

    // Alokasikan satu slot tekstur sebesar layar penuh; isinya diganti per layar
    texLayar.PSM = GS_PSM_CT32;
    texLayar.Filter = GS_FILTER_LINEAR;
    texLayar.Clut = NULL;
    texLayar.VramClut = 0;
    texLayar.Delayed = 0;
    texLayar.Vram = gsKit_vram_alloc(gsGlobal,
                        gsKit_texture_size(640, 448, GS_PSM_CT32),
                        GSKIT_ALLOC_USERBUFFER);
    if (texLayar.Vram == GSKIT_ALLOC_ERROR) {
        while (1) {
            gsKit_vsync_wait();
            gsKit_clear(gsGlobal, GS_SETREG_RGBAQ(255, 0, 0, 0x00, 0));
            gsKit_queue_exec(gsGlobal);
            gsKit_sync_flip(gsGlobal);
        }
    }
    pakai_tekstur(gsGlobal, judul_pixels, 640, 448);
    printf("[LOG ULAR TANGGA]: Tekstur judul terunggah.\n");

    struct padButtonStatus buttons;
    u32 paddata, old_pad = 0, new_pad;

    int layar = LAYAR_JUDUL;
    int menu_sel = 0;
    int set_sel = 0;
    int pemain_pertama = 1;

    int p1_pos = 0, p2_pos = 0;
    int giliran = 1;
    int angka_dadu_terakhir = 0;
    int pemenang = 0;
    char pesan_hasil[40] = "";
    int hasil_timer = 0;

    unsigned int frame_counter = 0;
    int seed_diatur = 0;

    u64 color_navy = GS_SETREG_RGBAQ(10, 22, 48, 0x00, 0);
    u64 color_gelap = GS_SETREG_RGBAQ(4, 10, 26, 0x00, 0);
    u64 color_p1 = GS_SETREG_RGBAQ(255, 50, 50, 0x00, 0);
    u64 color_p2 = GS_SETREG_RGBAQ(50, 200, 255, 0x00, 0);
    u64 color_dot = GS_SETREG_RGBAQ(10, 10, 10, 0x00, 0);
    u64 color_p1_terang = GS_SETREG_RGBAQ(255, 130, 110, 0x00, 0);
    u64 color_p1_gelap = GS_SETREG_RGBAQ(170, 26, 22, 0x00, 0);
    u64 color_p2_terang = GS_SETREG_RGBAQ(140, 210, 255, 0x00, 0);
    u64 color_p2_gelap = GS_SETREG_RGBAQ(24, 88, 180, 0x00, 0);
    u64 color_teks = GS_SETREG_RGBAQ(235, 242, 255, 0x00, 0);
    u64 color_kuning = GS_SETREG_RGBAQ(255, 205, 48, 0x00, 0);

    printf("[LOG ULAR TANGGA]: Memasuki putaran utama.\n");

    while (1) {
        frame_counter++;

        // ---- Baca stik ----
        new_pad = 0;
        if (padGetState(0, 0) == 6) {
            int ret = padRead(0, 0, &buttons);
            if (ret != 0) {
                paddata = 0xffff ^ buttons.btns;
                new_pad = paddata & ~old_pad;
                old_pad = paddata;
            }
        }

        // ---- Logika per layar ----
        if (layar == LAYAR_JUDUL) {
            if (new_pad & PAD_DOWN) menu_sel = (menu_sel + 1) % 3;
            if (new_pad & PAD_UP)   menu_sel = (menu_sel + 2) % 3;
            if (new_pad & PAD_CROSS) {
                if (menu_sel == 0) {
                    // Mulai Game: reset & masuk layar game
                    p1_pos = 0; p2_pos = 0;
                    giliran = pemain_pertama;
                    angka_dadu_terakhir = 0;
                    pemenang = 0;
                    pesan_hasil[0] = 0;
                    hasil_timer = 0;
                    pakai_tekstur(gsGlobal, board_pixels, 640, 448);
                    layar = LAYAR_GAME;
                } else if (menu_sel == 1) {
                    set_sel = 0;
                    pakai_tekstur(gsGlobal, (pemain_pertama == 1) ? set_p1_pixels : set_p2_pixels, 640, 448);
                    layar = LAYAR_SET;
                } else {
                    // Keluar: kembali ke pemuat utama (wOPL di MC)
                    printf("[LOG ULAR TANGGA]: Keluar ke BOOT.ELF.\n");
                    LoadExecPS2("mc0:/BOOT/BOOT.ELF", 0, NULL);
                }
            }
        } else if (layar == LAYAR_SET) {
            if (new_pad & PAD_DOWN) set_sel = (set_sel + 1) % 2;
            if (new_pad & PAD_UP)   set_sel = (set_sel + 1) % 2;
            if ((new_pad & (PAD_LEFT | PAD_RIGHT)) && set_sel == 0) {
                pemain_pertama = (pemain_pertama == 1) ? 2 : 1;
                pakai_tekstur(gsGlobal, (pemain_pertama == 1) ? set_p1_pixels : set_p2_pixels, 640, 448);
            }
            if (new_pad & PAD_CROSS) {
                if (set_sel == 0) {
                    pemain_pertama = (pemain_pertama == 1) ? 2 : 1;
                    pakai_tekstur(gsGlobal, (pemain_pertama == 1) ? set_p1_pixels : set_p2_pixels, 640, 448);
                } else {
                    pakai_tekstur(gsGlobal, judul_pixels, 640, 448);
                    layar = LAYAR_JUDUL;
                }
            }
            if (new_pad & PAD_TRIANGLE) {
                pakai_tekstur(gsGlobal, judul_pixels, 640, 448);
                layar = LAYAR_JUDUL;
            }
        } else { // LAYAR_GAME
            if (hasil_timer > 0) hasil_timer--;
            if (new_pad & PAD_START) {
                pemenang = 0;
                pakai_tekstur(gsGlobal, judul_pixels, 640, 448);
                layar = LAYAR_JUDUL;
            } else if (pemenang != 0) {
                if (new_pad & PAD_CROSS) {
                    pemenang = 0;
                    pakai_tekstur(gsGlobal, judul_pixels, 640, 448);
                    layar = LAYAR_JUDUL;
                }
            } else {
                // Tombol lempar dadu: X untuk kedua pemain (keputusan user)
                if (new_pad & PAD_CROSS) {
                    if (!seed_diatur) { srand(frame_counter); seed_diatur = 1; }
                    angka_dadu_terakhir = (rand() % 6) + 1;
                    int *posisi_skg = (giliran == 1) ? &p1_pos : &p2_pos;
                    int tujuan = *posisi_skg + angka_dadu_terakhir;
                    if (tujuan >= 100) {
                        *posisi_skg = 100;
                        pemenang = giliran;
                    } else {
                        int hasil = cekUlarTangga(tujuan);
                        if (hasil > tujuan) {
                            sprintf(pesan_hasil, "TANGGA! NAIK KE %d", hasil);
                        } else if (hasil < tujuan) {
                            sprintf(pesan_hasil, "ULAR! TURUN KE %d", hasil);
                        } else {
                            sprintf(pesan_hasil, "%s MAJU %d", (giliran == 1) ? "P1" : "P2", angka_dadu_terakhir);
                        }
                        *posisi_skg = hasil;
                        hasil_timer = 100;
                        giliran = (giliran == 1) ? 2 : 1;
                    }
                }
            }
        }

        gsKit_vsync_wait();

        // ---- Gambar per layar ----
        if (layar == LAYAR_JUDUL) {
            gsKit_clear(gsGlobal, color_navy);
            gsKit_prim_sprite_texture(gsGlobal, &texLayar,
                                      0, 0, 0, 0,
                                      640, 448, 640, 448,
                                      3, GS_SETREG_RGBAQ(0x80, 0x80, 0x80, 0x00, 0));
            int cy = MENU_Y[menu_sel];
            u64 warna_bingkai = ((frame_counter / 20) % 2 == 0)
                ? GS_SETREG_RGBAQ(90, 170, 255, 0x00, 0)
                : GS_SETREG_RGBAQ(170, 225, 255, 0x00, 0);
            draw_bingkai(gsGlobal, 183, cy - 17, 457, cy + 17, warna_bingkai);
            gsKit_prim_triangle(gsGlobal, 190, cy - 7, 190, cy + 7, 202, cy, 2, color_kuning);
        } else if (layar == LAYAR_SET) {
            gsKit_clear(gsGlobal, color_navy);
            gsKit_prim_sprite_texture(gsGlobal, &texLayar,
                                      0, 0, 0, 0,
                                      640, 448, 640, 448,
                                      3, GS_SETREG_RGBAQ(0x80, 0x80, 0x80, 0x00, 0));
            int cy = SET_Y[set_sel];
            u64 warna_bingkai = ((frame_counter / 20) % 2 == 0)
                ? GS_SETREG_RGBAQ(90, 170, 255, 0x00, 0)
                : GS_SETREG_RGBAQ(170, 225, 255, 0x00, 0);
            draw_bingkai(gsGlobal, 126, cy - 20, 514, cy + 20, warna_bingkai);
        } else { // LAYAR_GAME
            gsKit_clear(gsGlobal, color_navy);
            gsKit_prim_sprite_texture(gsGlobal, &texLayar,
                                      0, 0, 0, 0,
                                      640, 448, 640, 448,
                                      3, GS_SETREG_RGBAQ(0x80, 0x80, 0x80, 0x00, 0));

            // Penanda giliran: bingkai emas berkedip di lencana pemain aktif
            if (pemenang == 0 && (frame_counter / 20) % 2 == 0) {
                u64 emas = GS_SETREG_RGBAQ(255, 205, 80, 0x00, 0);
                if (giliran == 1) draw_bingkai(gsGlobal, 10, 8, 280, 58, emas);
                else draw_bingkai(gsGlobal, 360, 8, 630, 58, emas);
            }

            // Nilai posisi di lencana (tengah ruang kosong lencana)
            char skor[8];
            sprintf(skor, "%d", p1_pos);
            draw_text(gsGlobal, 205 - ((int)strlen(skor) * 18 - 3) / 2, 23, 3, color_teks, skor);
            sprintf(skor, "%d", p2_pos);
            draw_text(gsGlobal, 435 - ((int)strlen(skor) * 18 - 3) / 2, 23, 3, color_teks, skor);

            // Bidak: hanya bila sudah masuk papan (posisi > 0)
            if (p1_pos > 0 && p1_pos == p2_pos) {
                int bx, by;
                get_xy_dari_posisi(p1_pos, &bx, &by);
                draw_bidak(gsGlobal, bx + 10, by + 17, 0.55f, color_p1_terang, color_p1, color_p1_gelap);
                draw_bidak(gsGlobal, bx + 24, by + 17, 0.55f, color_p2_terang, color_p2, color_p2_gelap);
            } else {
                if (p1_pos > 0) {
                    int bx, by;
                    get_xy_dari_posisi(p1_pos, &bx, &by);
                    draw_bidak(gsGlobal, bx + 17, by + 17, 0.72f, color_p1_terang, color_p1, color_p1_gelap);
                }
                if (p2_pos > 0) {
                    int bx, by;
                    get_xy_dari_posisi(p2_pos, &bx, &by);
                    draw_bidak(gsGlobal, bx + 17, by + 17, 0.72f, color_p2_terang, color_p2, color_p2_gelap);
                }
            }

            // Titik dadu di atas dadu polos kanvas
            if (angka_dadu_terakhir > 0) draw_dadu(gsGlobal, angka_dadu_terakhir, color_dot);

            // Banner bawah: menang / hasil lemparan / giliran
            if (pemenang != 0) {
                char wt[24];
                sprintf(wt, "PEMAIN %d MENANG!", pemenang);
                draw_text(gsGlobal, 320 - ((int)strlen(wt) * 12 - 2) / 2, 409, 2, color_kuning, wt);
            } else if (hasil_timer > 0) {
                draw_text(gsGlobal, 320 - ((int)strlen(pesan_hasil) * 12 - 2) / 2, 409, 2, color_teks, pesan_hasil);
            } else {
                char kiri[32];
                sprintf(kiri, "GILIRAN PEMAIN %d - TEKAN ", giliran);
                const char *kanan = " UNTUK MELEMPAR DADU";
                int wk = (int)strlen(kiri) * 12 - 2;
                int wn = (int)strlen(kanan) * 12 - 2;
                int sx = 320 - (wk + 26 + wn) / 2;
                draw_text(gsGlobal, sx, 409, 2, color_teks, kiri);
                draw_elips(gsGlobal, sx + wk + 13, 416, 11, 11, 2, GS_SETREG_RGBAQ(47, 127, 224, 0x00, 0));
                draw_text(gsGlobal, sx + wk + 8, 409, 2, color_teks, "X");
                draw_text(gsGlobal, sx + wk + 26, 409, 2, color_teks, kanan);
            }
        }

        gsKit_queue_exec(gsGlobal);
        gsKit_sync_flip(gsGlobal);
    }
    return 0;
}
