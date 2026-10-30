#include <tamtypes.h>
#include <sifrpc.h>
#include <loadfile.h>
#include <stdio.h>
#include <stdlib.h>
#include <libpad.h>
#include <libmc.h>
#include <kernel.h>
#include <iopcontrol.h>
#include <gsKit.h>
#include <dmaKit.h>
#include <math.h>
#include <string.h>

#include "board_data.h"
#include "judul_data.h"
#include "set_data.h"
#include "aturan_data.h"
#include "dice_data.h"

static char padBuf[256] __attribute__((aligned(64)));
static char padBuf2[256] __attribute__((aligned(64)));

// Mesin status layar
#define LAYAR_JUDUL  0
#define LAYAR_SET    1
#define LAYAR_GAME   2
#define LAYAR_ATURAN 3

// Fase di dalam layar game
#define F_TUNGGU      0   // menunggu pemain menekan X untuk melempar
#define F_DADU        1   // animasi dadu berputar (layar dadu)
#define F_DADU_HASIL  2   // dadu berhenti di hasil (layar dadu)
#define F_JALAN       3   // bidak berjalan petak demi petak
#define F_JALUR       4   // bidak meluncur mengikuti ular/tangga
#define F_PESAN       5   // pesan hasil tampil di papan
#define F_PEN_TUNGGU  6   // lempar penentu: menunggu X (layar dadu)
#define F_PEN_PESAN   7   // lempar penentu: pesan seri (layar dadu)

// Baris menu judul (koord layar 640x448, sesuai kanvas judul)
static const int MENU_Y[3] = {332, 369, 406};
// Baris layar pengaturan baru (kanvas set.png): GETAR / KECEPATAN / KEMBALI
static const int SET_Y[3] = {194, 238, 282};
// Baris layar aturan main (kanvas aturan.png)
static const int ATURAN_Y[7] = {148, 180, 212, 244, 276, 308, 340};
// Baris menu jeda & pemenang (digambar kode di atas papan)
static const int JEDA_Y[4] = {176, 212, 248, 284};
static const int MENANG_Y[3] = {282, 310, 338};

// Indeks aturan
#define AWAL_P1 0
#define AWAL_P2 1
#define AWAL_PENENTU 2
#define AWAL_ACAK 3
#define MODEL_BERBAGI 0
#define MODEL_DUA 1
#define FINIS_TEPAT 0
#define FINIS_LEWAT 1
#define FINIS_PANTUL 2

// Tabel rute (struct Jump + daftar_ular_tangga) tinggal di board_data.h.
int cekUlarTangga(int posisi) {
    int jumlah_entri = sizeof(daftar_ular_tangga) / sizeof(daftar_ular_tangga[0]);
    for (int i = 0; i < jumlah_entri; i++) {
        if (daftar_ular_tangga[i].asal == posisi) {
            return daftar_ular_tangga[i].tujuan;
        }
    }
    return posisi;
}

// Hasil satu lemparan dihitung murni dari posisi + aturan aktif.
// keluaran: *landing = petak tempat bidak mendarat sebelum ular/tangga
//            *final   = petak akhir sesudah ular/tangga
// nilai balik: 0 = tidak bergerak, 1 = bergerak, 2 = menang
static int hitung_hasil(int pos, int roll, int mulai6, int finis,
                        int *landing, int *final) {
    if (pos == 0 && mulai6 && roll < 6) return 0;
    int raw = pos + roll;
    if (raw > 100) {
        if (finis == FINIS_TEPAT) return 0;
        if (finis == FINIS_LEWAT) { *landing = 100; *final = 100; return 2; }
        *landing = 200 - raw; // FINIS_PANTUL: memantul balik dari 100
        *final = cekUlarTangga(*landing);
        return (*final == 100) ? 2 : 1;
    }
    if (raw == 100) { *landing = 100; *final = 100; return 2; }
    *landing = raw;
    *final = cekUlarTangga(raw);
    return (*final == 100) ? 2 : 1;
}

// Konversi posisi 1-100 ke koordinat layar (papan: asal 152,64, rentang 335)
void get_xy_dari_posisi(int posisi, int *x, int *y) {
    int index = posisi - 1;
    int baris = index / 10;
    int kolom = index % 10;
    if (baris % 2 != 0) kolom = 9 - kolom;
    *x = 152 + (kolom * 335) / 10;
    *y = 64 + ((9 - baris) * 335) / 10;
}

// Titik dadu kecil di papan (di atas dadu polos kanvas, kotak 64x64 di 36,200)
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

// Wajah dadu besar versi primitif (cadangan bila tekstur wajah gagal dimuat)
void draw_wajah_primitif(GSGLOBAL *gsGlobal, int angka, int cx, int cy, int ukuran, u64 putih, u64 dot) {
    int s = ukuran, x1 = cx - s/2, y1 = cy - s/2;
    gsKit_prim_sprite(gsGlobal, x1, y1, x1 + s, y1 + s, 2, putih);
    int u = s / 4, r = s / 11; // jarak titik dari tepi & ukuran titik
    int px[3] = {x1 + u, cx, x1 + s - u};
    int py[3] = {y1 + u, cy, y1 + s - u};
    #define TITIK(a, b) gsKit_prim_sprite(gsGlobal, px[a]-r, py[b]-r, px[a]+r, py[b]+r, 2, dot)
    if (angka == 1 || angka == 3 || angka == 5) TITIK(1, 1);
    if (angka >= 2) { TITIK(0, 0); TITIK(2, 2); }
    if (angka >= 4) { TITIK(2, 0); TITIK(0, 2); }
    if (angka == 6) { TITIK(0, 1); TITIK(2, 1); }
    #undef TITIK
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

// Panel pekat untuk menu jeda & pemenang
void draw_panel(GSGLOBAL *gsGlobal, int x1, int y1, int x2, int y2, u64 isi, u64 tepi) {
    gsKit_prim_sprite(gsGlobal, x1, y1, x2, y2, 2, isi);
    draw_bingkai(gsGlobal, x1, y1, x2, y2, tepi);
}

// Huruf bitmap 5x7 (bit 4..0 = kiri..kanan)
static const unsigned char HURUF_SPASI[7] = {0,0,0,0,0,0,0};
static const unsigned char HURUF_A[7] = {0x0E,0x11,0x11,0x1F,0x11,0x11,0x11};
static const unsigned char HURUF_B[7] = {0x1E,0x11,0x11,0x1E,0x11,0x11,0x1E};
static const unsigned char HURUF_C[7] = {0x0E,0x11,0x01,0x01,0x01,0x11,0x0E};
static const unsigned char HURUF_D[7] = {0x1C,0x12,0x11,0x11,0x11,0x12,0x1C};
static const unsigned char HURUF_E[7] = {0x1F,0x10,0x10,0x1E,0x10,0x10,0x1F};
static const unsigned char HURUF_F[7] = {0x1F,0x10,0x10,0x1E,0x10,0x10,0x10};
static const unsigned char HURUF_G[7] = {0x0E,0x11,0x10,0x17,0x11,0x11,0x0F};
static const unsigned char HURUF_H[7] = {0x11,0x11,0x11,0x1F,0x11,0x11,0x11};
static const unsigned char HURUF_I[7] = {0x1F,0x04,0x04,0x04,0x04,0x04,0x1F};
static const unsigned char HURUF_J[7] = {0x07,0x02,0x02,0x02,0x02,0x12,0x0C};
static const unsigned char HURUF_K[7] = {0x11,0x12,0x14,0x18,0x14,0x12,0x11};
static const unsigned char HURUF_L[7] = {0x10,0x10,0x10,0x10,0x10,0x10,0x1F};
static const unsigned char HURUF_M[7] = {0x11,0x1B,0x15,0x15,0x11,0x11,0x11};
static const unsigned char HURUF_N[7] = {0x11,0x19,0x15,0x13,0x11,0x11,0x11};
static const unsigned char HURUF_O[7] = {0x0E,0x11,0x11,0x11,0x11,0x11,0x0E};
static const unsigned char HURUF_P[7] = {0x1E,0x11,0x11,0x1E,0x10,0x10,0x10};
static const unsigned char HURUF_Q[7] = {0x0E,0x11,0x11,0x11,0x15,0x12,0x0D};
static const unsigned char HURUF_R[7] = {0x1E,0x11,0x11,0x1E,0x14,0x12,0x11};
static const unsigned char HURUF_S[7] = {0x0F,0x10,0x10,0x0E,0x01,0x01,0x1E};
static const unsigned char HURUF_T[7] = {0x1F,0x04,0x04,0x04,0x04,0x04,0x04};
static const unsigned char HURUF_U[7] = {0x11,0x11,0x11,0x11,0x11,0x11,0x0E};
static const unsigned char HURUF_V[7] = {0x11,0x11,0x11,0x11,0x11,0x0A,0x04};
static const unsigned char HURUF_W[7] = {0x11,0x11,0x11,0x15,0x15,0x15,0x0A};
static const unsigned char HURUF_X[7] = {0x11,0x11,0x0A,0x04,0x0A,0x11,0x11};
static const unsigned char HURUF_Y[7] = {0x11,0x11,0x0A,0x04,0x04,0x04,0x04};
static const unsigned char HURUF_Z[7] = {0x1F,0x01,0x02,0x04,0x08,0x10,0x1F};
static const unsigned char HURUF_0[7] = {0x0E,0x11,0x13,0x15,0x19,0x11,0x0E};
static const unsigned char HURUF_1[7] = {0x04,0x0C,0x04,0x04,0x04,0x04,0x0E};
static const unsigned char HURUF_2[7] = {0x0E,0x11,0x01,0x06,0x08,0x10,0x1F};
static const unsigned char HURUF_3[7] = {0x0E,0x11,0x01,0x06,0x01,0x11,0x0E};
static const unsigned char HURUF_4[7] = {0x06,0x0A,0x12,0x1F,0x02,0x02,0x02};
static const unsigned char HURUF_5[7] = {0x1F,0x10,0x1E,0x01,0x01,0x11,0x0E};
static const unsigned char HURUF_6[7] = {0x06,0x08,0x10,0x1E,0x11,0x11,0x0E};
static const unsigned char HURUF_7[7] = {0x1F,0x01,0x02,0x04,0x08,0x08,0x08};
static const unsigned char HURUF_8[7] = {0x0E,0x11,0x11,0x0E,0x11,0x11,0x0E};
static const unsigned char HURUF_9[7] = {0x0E,0x11,0x11,0x0F,0x01,0x02,0x0C};
static const unsigned char HURUF_TANDA[7] = {0x04,0x04,0x04,0x04,0x04,0x00,0x04};
static const unsigned char HURUF_TANYA[7] = {0x0E,0x11,0x01,0x06,0x04,0x00,0x04};
static const unsigned char HURUF_MINUS[7] = {0x00,0x00,0x00,0x1F,0x00,0x00,0x00};
static const unsigned char HURUF_TITIK2[7] = {0x00,0x0C,0x0C,0x00,0x0C,0x0C,0x00};
static const unsigned char HURUF_TITIK[7] = {0x00,0x00,0x00,0x00,0x00,0x0C,0x0C};
static const unsigned char HURUF_MIRING[7] = {0x01,0x02,0x02,0x04,0x08,0x08,0x10};

static const unsigned char *glyph_for(char c) {
    switch (c) {
        case 'A': return HURUF_A; case 'B': return HURUF_B;
        case 'C': return HURUF_C; case 'D': return HURUF_D;
        case 'E': return HURUF_E; case 'F': return HURUF_F;
        case 'G': return HURUF_G; case 'H': return HURUF_H;
        case 'I': return HURUF_I; case 'J': return HURUF_J;
        case 'K': return HURUF_K; case 'L': return HURUF_L;
        case 'M': return HURUF_M; case 'N': return HURUF_N;
        case 'O': return HURUF_O; case 'P': return HURUF_P;
        case 'Q': return HURUF_Q; case 'R': return HURUF_R;
        case 'S': return HURUF_S; case 'T': return HURUF_T;
        case 'U': return HURUF_U; case 'V': return HURUF_V;
        case 'W': return HURUF_W; case 'X': return HURUF_X;
        case 'Y': return HURUF_Y; case 'Z': return HURUF_Z;
        case '0': return HURUF_0; case '1': return HURUF_1;
        case '2': return HURUF_2; case '3': return HURUF_3;
        case '4': return HURUF_4; case '5': return HURUF_5;
        case '6': return HURUF_6; case '7': return HURUF_7;
        case '8': return HURUF_8; case '9': return HURUF_9;
        case '!': return HURUF_TANDA; case '?': return HURUF_TANYA;
        case '-': return HURUF_MINUS; case ':': return HURUF_TITIK2;
        case '.': return HURUF_TITIK; case '/': return HURUF_MIRING;
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

static int lebar_teks(const char *teks, int skala) {
    int n = (int)strlen(teks);
    return (n > 0) ? n * 6 * skala - skala : 0;
}

static void draw_text_tengah(GSGLOBAL *gsGlobal, int cx, int y, int skala, u64 warna, const char *teks) {
    draw_text(gsGlobal, cx - lebar_teks(teks, skala) / 2, y, skala, warna, teks);
}

// Satu slot tekstur VRAM layar penuh dipakai bergantian oleh semua layar
static GSTEXTURE texLayar;
static void pakai_tekstur(GSGLOBAL *gsGlobal, const unsigned char *piksel, int w, int h) {
    texLayar.Width = w;
    texLayar.Height = h;
    texLayar.Mem = (u32 *)piksel;
    gsKit_texture_upload(gsGlobal, &texLayar);
}

// Pindah layar di tengah putaran: tunggu GS menuntaskan antrean gambar
// frame sebelumnya (gsKit_finish) sebelum mengunggah tekstur baru.
// Tanpa tunggu ini, unggahan bertabrakan dengan antrean gambar layar
// game yang berat dan konsol membeku (kasus terbukti: Start di tengah
// permainan hang, sedangkan kembali dari Pengaturan aman).
static void ganti_tekstur(GSGLOBAL *gsGlobal, const unsigned char *piksel, int w, int h) {
    gsKit_finish();
    pakai_tekstur(gsGlobal, piksel, w, h);
}

// Tekstur wajah dadu (dimuat sekali, tinggal tetap di VRAM)
static GSTEXTURE texWajah[6];
static int wajah_ok = 0;
static const unsigned char *WAJAH_PIX[6] = {
    dice_face1_pixels, dice_face2_pixels, dice_face3_pixels,
    dice_face4_pixels, dice_face5_pixels, dice_face6_pixels
};

// ===== Pengaturan & aturan (tersimpan ke memory card) =====
static int set_getar = 0;        // 0 MATI, 1 HIDUP
static int set_kecepatan = 0;    // 0 NORMAL, 1 CEPAT
static int aturan_awal = AWAL_P1;
static int aturan_model = MODEL_BERBAGI;
static int aturan_bonus6 = 0;    // 0 MATI, 1 HIDUP
static int aturan_finis = FINIS_TEPAT;
static int aturan_mulai6 = 0;    // 0 MATI, 1 HIDUP
static int rekor_menang[3] = {0, 0, 0};   // indeks pemain 1..2

// ===== Simpan/muat memory card (libmc asinkron, tidak memblokir game) =====
#define MC_O_RDONLY 0x0001
#define MC_O_WRONLY 0x0002
#define MC_O_CREAT  0x0200
#define MC_O_TRUNC  0x0400
#define SAVE_MAGIC  0x55544731
typedef struct {
    int magic, getar, kecepatan, awal, model, bonus, finis, mulai6, rekor1, rekor2;
} SaveData;
static SaveData save_buf;
static int mc_op = 0;            // 0 diam, 1 muat, 2 simpan
static int mc_step = 0;
static int mc_fd = -1;
static int mc_tunggu = 0;
static int mc_minta_simpan = 0;
static int mc_minta_muat = 1;
static int mc_type = 0, mc_free = 0, mc_format = 0;

static void mc_gagal(void) {
    mc_op = 0; mc_step = 0; mc_fd = -1;
    mc_minta_simpan = 0; mc_minta_muat = 0;
}

static void mc_poll(unsigned int frame_counter) {
    int cmd = 0, res = 0;
    if (mc_op == 0) {
        if (mc_minta_muat) {
            mc_op = 1; mc_step = 0; mc_tunggu = 0;
            mcGetInfo(0, 0, &mc_type, &mc_free, &mc_format);
        } else if (mc_minta_simpan) {
            mc_op = 2; mc_step = 0; mc_tunggu = 0;
            mcGetInfo(0, 0, &mc_type, &mc_free, &mc_format);
        }
        return;
    }
    if (mcSync(1, &cmd, &res) == 0) {           // masih berjalan
        if (++mc_tunggu > 900) mc_gagal();      // macet > 15 detik: batalkan
        return;
    }
    mc_tunggu = 0;
    if (mc_step == 0) {                          // GetInfo selesai
        if (res < -1) { mc_gagal(); return; }    // kartu belum format / galat
        if (mc_op == 1) mcOpen(0, 0, "ULARTANGGA/SAVE.BIN", MC_O_RDONLY);
        else mcMkDir(0, 0, "ULARTANGGA");
        mc_step = 1;
    } else if (mc_op == 1) {                     // muat
        if (mc_step == 1) {
            if (res < 0) { mc_gagal(); return; } // belum ada berkas simpanan
            mc_fd = res;
            mcRead(mc_fd, &save_buf, sizeof(SaveData));
            mc_step = 2;
        } else if (mc_step == 2) {
            if (res == (int)sizeof(SaveData) && save_buf.magic == SAVE_MAGIC) {
                if (frame_counter < 600) {       // terapkan penuh hanya di awal
                    set_getar = save_buf.getar;
                    set_kecepatan = save_buf.kecepatan;
                    aturan_awal = save_buf.awal;
                    aturan_model = save_buf.model;
                    aturan_bonus6 = save_buf.bonus;
                    aturan_finis = save_buf.finis;
                    aturan_mulai6 = save_buf.mulai6;
                }
                rekor_menang[1] = save_buf.rekor1;
                rekor_menang[2] = save_buf.rekor2;
            }
            mcClose(mc_fd);
            mc_step = 3;
        } else {
            mc_op = 0; mc_step = 0; mc_fd = -1; mc_minta_muat = 0;
        }
    } else {                                     // simpan
        if (mc_step == 1) {                      // MkDir selesai (hasil diabaikan)
            save_buf.magic = SAVE_MAGIC;
            save_buf.getar = set_getar;
            save_buf.kecepatan = set_kecepatan;
            save_buf.awal = aturan_awal;
            save_buf.model = aturan_model;
            save_buf.bonus = aturan_bonus6;
            save_buf.finis = aturan_finis;
            save_buf.mulai6 = aturan_mulai6;
            save_buf.rekor1 = rekor_menang[1];
            save_buf.rekor2 = rekor_menang[2];
            mcOpen(0, 0, "ULARTANGGA/SAVE.BIN", MC_O_CREAT | MC_O_WRONLY | MC_O_TRUNC);
            mc_step = 2;
        } else if (mc_step == 2) {
            if (res < 0) { mc_gagal(); return; }
            mc_fd = res;
            mcWrite(mc_fd, &save_buf, sizeof(SaveData));
            mc_step = 3;
        } else if (mc_step == 3) {
            mcClose(mc_fd);
            mc_step = 4;
        } else {
            mc_op = 0; mc_step = 0; mc_fd = -1; mc_minta_simpan = 0;
        }
    }
}

// ===== Stik kedua & getar =====
static u32 np1_g = 0, np2_g = 0;   // tepi tombol frame ini (untuk fungsi bantu)
static int pad2_ada_g = 0;

static int edges_pemain(int p) {
    if (aturan_model == MODEL_BERBAGI || p == 1 || !pad2_ada_g) return np1_g;
    return np2_g;
}
static int port_pemain(int p) {
    if (aturan_model == MODEL_DUA && p == 2 && pad2_ada_g) return 1;
    return 0;
}

static const char ACT_ALIGN[6] = {0, 1, (char)0xFF, (char)0xFF, (char)0xFF, (char)0xFF};
static int getar_port = 0;
static int getar_sisa = 0;
static void getar_mulai(int port, int kecil, int besar, int durasi) {
    if (!set_getar) return;
    char act[6] = {(char)(kecil ? 1 : 0), (char)besar, 0, 0, 0, 0};
    padSetActDirect(port, 0, act);
    getar_port = port;
    getar_sisa = durasi;
}
static void getar_untuk(int pemain, int kecil, int besar, int durasi) {
    getar_mulai(port_pemain(pemain), kecil, besar, durasi);
}
static void getar_poll(void) {
    if (getar_sisa > 0 && --getar_sisa == 0) {
        char act[6] = {0, 0, 0, 0, 0, 0};
        padSetActDirect(getar_port, 0, act);
    }
}

// ===== Keadaan pertandingan =====
static int pos[3] = {0, 0, 0}, vis[3] = {0, 0, 0};
static int stat_lemparan[3] = {0, 0, 0};
static int stat_tangga[3] = {0, 0, 0};
static int stat_ular[3] = {0, 0, 0};
static int giliran = 1, pemenang = 0, fase = F_TUNGGU;
static int roll_skg = 0, wajah_skg = 1;
static int dadu_timer = 0, hasil_timer = 0;
static int landing_skg = 0, final_skg = 0, hasil_kode = 0;
static int enam_beruntun = 0, posisi_awal_rantai = 0;
static int mode_lempar = 0, pen_giliran = 1, pen1 = 0, pen2 = 0;
static char pesan_hasil[64] = "";
static int seed_diatur = 0;

static int fase_di_layar_dadu(int f) {
    return f == F_DADU || f == F_DADU_HASIL || f == F_PEN_TUNGGU || f == F_PEN_PESAN;
}

// Penutup giliran sesudah bidak tiba (menang ditangani terpisah)
static void finalisasi(void) {
    int g = giliran;
    pos[g] = final_skg;
    vis[g] = final_skg;
    if (final_skg > landing_skg) stat_tangga[g]++;
    else if (final_skg < landing_skg) stat_ular[g]++;
    if (hasil_kode == 2) {
        pemenang = g;
        rekor_menang[g]++;
        mc_minta_simpan = 1;
        getar_untuk(g, 1, 255, 45);
        return;
    }
    if (final_skg > landing_skg) sprintf(pesan_hasil, "TANGGA! NAIK KE %d", final_skg);
    else if (final_skg < landing_skg) sprintf(pesan_hasil, "ULAR! TURUN KE %d", final_skg);
    else sprintf(pesan_hasil, "P%d MAJU %d", g, roll_skg);
    if (roll_skg == 6 && aturan_bonus6) {
        enam_beruntun++;
        strcat(pesan_hasil, " - LEMPAR LAGI");
    } else {
        enam_beruntun = 0;
        giliran = 3 - g;
    }
    hasil_timer = 85;
    fase = F_PESAN;
}

// Penutup giliran bila bidak tidak bergerak (atau giliran dibatalkan)
static void akhiri_tanpa_gerak(int batal) {
    if (!batal) {
        if (roll_skg == 6 && aturan_bonus6) {
            enam_beruntun++;
            strcat(pesan_hasil, " - LEMPAR LAGI");
        } else {
            enam_beruntun = 0;
            giliran = 3 - giliran;
        }
    }
    hasil_timer = 100;
    fase = F_PESAN;
}

// Atur ulang pertandingan & tentukan pemain pertama menurut aturan
static void reset_pertandingan(GSGLOBAL *gsGlobal, unsigned int frame) {
    pos[1] = pos[2] = vis[1] = vis[2] = 0;
    stat_lemparan[1] = stat_lemparan[2] = 0;
    stat_tangga[1] = stat_tangga[2] = 0;
    stat_ular[1] = stat_ular[2] = 0;
    pemenang = 0;
    enam_beruntun = 0;
    roll_skg = 0;
    pesan_hasil[0] = 0;
    hasil_timer = 0;
    if (!seed_diatur) { srand(frame + 12345u); seed_diatur = 1; }
    if (aturan_awal == AWAL_PENENTU) {
        mode_lempar = 1;
        pen_giliran = 1;
        pen1 = pen2 = 0;
        ganti_tekstur(gsGlobal, dice_pixels, 640, 448);
        fase = F_PEN_TUNGGU;
    } else {
        mode_lempar = 0;
        if (aturan_awal == AWAL_P2) giliran = 2;
        else if (aturan_awal == AWAL_ACAK) giliran = 1 + (rand() % 2);
        else giliran = 1;
        ganti_tekstur(gsGlobal, board_pixels, 640, 448);
        fase = F_TUNGGU;
    }
}

int main(int argc, char *argv[]) {
    // Reset IOP lebih dulu: game diluncurkan dari wOPL/uLaunchELF yang sudah
    // memuat driver stik sendiri; tanpa reset, pemuatan modul stik kita
    // tabrakan dan stik mati total di konsol asli.
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
    padPortOpen(1, 0, padBuf2);
    padSetMainMode(1, 0, 1, 3);
    padSetActAlign(0, 0, ACT_ALIGN);
    padSetActAlign(1, 0, ACT_ALIGN);
    mcInit(MC_TYPE_MC);
    printf("[LOG ULAR TANGGA]: Stik 1+2 & memory card siap.\n");

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

    // Tekstur wajah dadu: dimuat sekali, tinggal tetap di VRAM
    wajah_ok = 1;
    for (int i = 0; i < 6; i++) {
        texWajah[i].Width = 128;
        texWajah[i].Height = 128;
        texWajah[i].PSM = GS_PSM_CT32;
        texWajah[i].Filter = GS_FILTER_LINEAR;
        texWajah[i].Clut = NULL;
        texWajah[i].VramClut = 0;
        texWajah[i].Delayed = 0;
        texWajah[i].Mem = (u32 *)WAJAH_PIX[i];
        texWajah[i].Vram = gsKit_vram_alloc(gsGlobal,
                            gsKit_texture_size(128, 128, GS_PSM_CT32),
                            GSKIT_ALLOC_USERBUFFER);
        if (texWajah[i].Vram == GSKIT_ALLOC_ERROR) { wajah_ok = 0; break; }
        gsKit_texture_upload(gsGlobal, &texWajah[i]);
    }
    printf("[LOG ULAR TANGGA]: Tekstur wajah dadu %s.\n", wajah_ok ? "termuat" : "GAGAL - pakai primitif");

    // Unggahan pertama: belum ada antrean gambar, pakai unggah langsung
    pakai_tekstur(gsGlobal, judul_pixels, 640, 448);
    printf("[LOG ULAR TANGGA]: Tekstur judul terunggah.\n");

    struct padButtonStatus b1, b2;
    u32 old1 = 0, old2 = 0;
    int pad2_pernah_ok = 0, pad2_gagal = 0;

    int layar = LAYAR_JUDUL;
    int menu_sel = 0;
    int set_sel = 0, set_dari = 0;      // set_dari: 0 judul, 1 jeda
    int aturan_sel = 0;
    int jeda_aktif = 0, jeda_sel = 0;
    int menang_sel = 0;
    int jalan_target = 0, jalan_arah = 1, jalan_lewat100 = 0, langkah_timer = 0;
    int jalur_timer = 0, jalur_dari_x = 0, jalur_dari_y = 0, jalur_ke_x = 0, jalur_ke_y = 0;

    unsigned int frame_counter = 0;

    u64 color_navy = GS_SETREG_RGBAQ(10, 22, 48, 0x00, 0);
    u64 color_panel = GS_SETREG_RGBAQ(8, 18, 44, 0x00, 0);
    u64 color_biru = GS_SETREG_RGBAQ(64, 156, 255, 0x00, 0);
    u64 color_p1 = GS_SETREG_RGBAQ(255, 50, 50, 0x00, 0);
    u64 color_p2 = GS_SETREG_RGBAQ(50, 200, 255, 0x00, 0);
    u64 color_dot = GS_SETREG_RGBAQ(10, 10, 10, 0x00, 0);
    u64 color_putih = GS_SETREG_RGBAQ(245, 247, 252, 0x00, 0);
    u64 color_p1_terang = GS_SETREG_RGBAQ(255, 130, 110, 0x00, 0);
    u64 color_p1_gelap = GS_SETREG_RGBAQ(170, 26, 22, 0x00, 0);
    u64 color_p2_terang = GS_SETREG_RGBAQ(140, 210, 255, 0x00, 0);
    u64 color_p2_gelap = GS_SETREG_RGBAQ(24, 88, 180, 0x00, 0);
    u64 color_teks = GS_SETREG_RGBAQ(235, 242, 255, 0x00, 0);
    u64 color_kuning = GS_SETREG_RGBAQ(255, 205, 48, 0x00, 0);
    u64 color_emas = GS_SETREG_RGBAQ(255, 205, 80, 0x00, 0);

    printf("[LOG ULAR TANGGA]: Memasuki putaran utama.\n");

    while (1) {
        frame_counter++;
        mc_poll(frame_counter);
        getar_poll();

        // ---- Baca kedua stik ----
        np1_g = 0;
        if (padGetState(0, 0) == 6) {
            int ret = padRead(0, 0, &b1);
            if (ret != 0) {
                u32 paddata = 0xffff ^ b1.btns;
                np1_g = paddata & ~old1;
                old1 = paddata;
            }
        }
        np2_g = 0;
        {
            int p2ok = 0;
            if (padGetState(1, 0) == 6) {
                int ret = padRead(1, 0, &b2);
                if (ret != 0) {
                    u32 paddata = 0xffff ^ b2.btns;
                    np2_g = paddata & ~old2;
                    old2 = paddata;
                    p2ok = 1;
                }
            }
            if (p2ok) { pad2_pernah_ok = 1; pad2_gagal = 0; }
            else if (pad2_gagal < 100000) pad2_gagal++;
        }
        pad2_ada_g = pad2_pernah_ok && pad2_gagal < 120;
        u32 menu_pad = np1_g | np2_g;

        // ---- Logika per layar ----
        if (layar == LAYAR_JUDUL) {
            if (menu_pad & PAD_DOWN) menu_sel = (menu_sel + 1) % 3;
            if (menu_pad & PAD_UP)   menu_sel = (menu_sel + 2) % 3;
            if (menu_pad & PAD_CROSS) {
                if (menu_sel == 0) {
                    aturan_sel = 0;
                    ganti_tekstur(gsGlobal, aturan_pixels, 640, 448);
                    layar = LAYAR_ATURAN;
                } else if (menu_sel == 1) {
                    set_sel = 0; set_dari = 0;
                    ganti_tekstur(gsGlobal, set_pixels, 640, 448);
                    layar = LAYAR_SET;
                } else {
                    printf("[LOG ULAR TANGGA]: Keluar ke BOOT.ELF.\n");
                    LoadExecPS2("mc0:/BOOT/BOOT.ELF", 0, NULL);
                }
            }
        } else if (layar == LAYAR_SET) {
            if (menu_pad & PAD_DOWN) set_sel = (set_sel + 1) % 3;
            if (menu_pad & PAD_UP)   set_sel = (set_sel + 2) % 3;
            int kembali = 0;
            if (menu_pad & PAD_CIRCLE) kembali = 1;
            if ((menu_pad & (PAD_LEFT | PAD_RIGHT)) && set_sel < 2) {
                if (set_sel == 0) set_getar = !set_getar;
                else set_kecepatan = !set_kecepatan;
            }
            if (menu_pad & PAD_CROSS) {
                if (set_sel == 0) set_getar = !set_getar;
                else if (set_sel == 1) set_kecepatan = !set_kecepatan;
                else kembali = 1;
            }
            if (kembali) {
                mc_minta_simpan = 1;
                if (set_dari == 1) {
                    ganti_tekstur(gsGlobal, fase_di_layar_dadu(fase) ? dice_pixels : board_pixels, 640, 448);
                    layar = LAYAR_GAME;
                } else {
                    ganti_tekstur(gsGlobal, judul_pixels, 640, 448);
                    layar = LAYAR_JUDUL;
                }
            }
        } else if (layar == LAYAR_ATURAN) {
            if (menu_pad & PAD_DOWN) aturan_sel = (aturan_sel + 1) % 7;
            if (menu_pad & PAD_UP)   aturan_sel = (aturan_sel + 6) % 7;
            int dir = 0;
            if (menu_pad & PAD_RIGHT) dir = 1;
            if (menu_pad & PAD_LEFT)  dir = -1;
            if (dir != 0 && aturan_sel < 5) {
                switch (aturan_sel) {
                    case 0: aturan_awal = (aturan_awal + dir + 4) % 4; break;
                    case 1: aturan_model = (aturan_model + dir + 2) % 2; break;
                    case 2: aturan_bonus6 = (aturan_bonus6 + dir + 2) % 2; break;
                    case 3: aturan_finis = (aturan_finis + dir + 3) % 3; break;
                    case 4: aturan_mulai6 = (aturan_mulai6 + dir + 2) % 2; break;
                }
            }
            if (menu_pad & PAD_CROSS) {
                if (aturan_sel < 5) {
                    switch (aturan_sel) {
                        case 0: aturan_awal = (aturan_awal + 1) % 4; break;
                        case 1: aturan_model = (aturan_model + 1) % 2; break;
                        case 2: aturan_bonus6 = (aturan_bonus6 + 1) % 2; break;
                        case 3: aturan_finis = (aturan_finis + 1) % 3; break;
                        case 4: aturan_mulai6 = (aturan_mulai6 + 1) % 2; break;
                    }
                } else if (aturan_sel == 5) {
                    // Aturan Resmi: preset resmi Hasbro
                    aturan_awal = AWAL_P1;
                    aturan_bonus6 = 0;
                    aturan_finis = FINIS_TEPAT;
                    aturan_mulai6 = 0;
                } else {
                    mc_minta_simpan = 1;
                    reset_pertandingan(gsGlobal, frame_counter);
                    layar = LAYAR_GAME;
                }
            }
            if (menu_pad & PAD_CIRCLE) {
                mc_minta_simpan = 1;
                ganti_tekstur(gsGlobal, judul_pixels, 640, 448);
                layar = LAYAR_JUDUL;
            }
        } else { // LAYAR_GAME
            if (pemenang != 0) {
                if (menu_pad & PAD_DOWN) menang_sel = (menang_sel + 1) % 3;
                if (menu_pad & PAD_UP)   menang_sel = (menang_sel + 2) % 3;
                if (menu_pad & PAD_CROSS) {
                    if (menang_sel == 0) {
                        menang_sel = 0;
                        reset_pertandingan(gsGlobal, frame_counter);
                    } else if (menang_sel == 1) {
                        pemenang = 0;
                        aturan_sel = 0;
                        ganti_tekstur(gsGlobal, aturan_pixels, 640, 448);
                        layar = LAYAR_ATURAN;
                    } else {
                        pemenang = 0;
                        ganti_tekstur(gsGlobal, judul_pixels, 640, 448);
                        layar = LAYAR_JUDUL;
                    }
                }
            } else if (jeda_aktif) {
                if (menu_pad & PAD_DOWN) jeda_sel = (jeda_sel + 1) % 4;
                if (menu_pad & PAD_UP)   jeda_sel = (jeda_sel + 3) % 4;
                if ((menu_pad & PAD_CIRCLE) || (menu_pad & PAD_START)) jeda_aktif = 0;
                else if (menu_pad & PAD_CROSS) {
                    if (jeda_sel == 0) {
                        jeda_aktif = 0;
                    } else if (jeda_sel == 1) {
                        jeda_aktif = 0;
                        set_sel = 0; set_dari = 1;
                        ganti_tekstur(gsGlobal, set_pixels, 640, 448);
                        layar = LAYAR_SET;
                    } else if (jeda_sel == 2) {
                        jeda_aktif = 0;
                        reset_pertandingan(gsGlobal, frame_counter);
                    } else {
                        jeda_aktif = 0;
                        ganti_tekstur(gsGlobal, judul_pixels, 640, 448);
                        layar = LAYAR_JUDUL;
                    }
                }
            } else {
                if ((np1_g | np2_g) & PAD_START) {
                    jeda_aktif = 1;
                    jeda_sel = 0;
                } else if (fase == F_TUNGGU || fase == F_PESAN) {
                    if (hasil_timer > 0) hasil_timer--;
                    if (fase == F_PESAN && hasil_timer == 0) {
                        fase = F_TUNGGU;
                    } else if (fase == F_TUNGGU && hasil_timer == 0 && (edges_pemain(giliran) & PAD_CROSS)) {
                        if (!seed_diatur) { srand(frame_counter + 999u); seed_diatur = 1; }
                        roll_skg = (rand() % 6) + 1;
                        stat_lemparan[giliran]++;
                        if (enam_beruntun == 0) posisi_awal_rantai = pos[giliran];
                        wajah_skg = 1 + (rand() % 6);
                        dadu_timer = 0;
                        ganti_tekstur(gsGlobal, dice_pixels, 640, 448);
                        fase = F_DADU;
                    }
                } else if (fase == F_PEN_TUNGGU) {
                    if (edges_pemain(pen_giliran) & PAD_CROSS) {
                        roll_skg = (rand() % 6) + 1;
                        wajah_skg = 1 + (rand() % 6);
                        dadu_timer = 0;
                        fase = F_DADU;
                    }
                } else if (fase == F_DADU) {
                    dadu_timer++;
                    int jeda_wajah = (dadu_timer < 60) ? 5 : (dadu_timer < 120) ? 8 : 14;
                    if (dadu_timer % jeda_wajah == 0) {
                        int w;
                        do { w = 1 + (rand() % 6); } while (w == wajah_skg);
                        wajah_skg = w;
                    }
                    int pemain_lempar = (mode_lempar == 1) ? pen_giliran : giliran;
                    if ((edges_pemain(pemain_lempar) & PAD_CROSS) || dadu_timer >= 180) {
                        hasil_timer = 0;
                        fase = F_DADU_HASIL;
                    }
                } else if (fase == F_DADU_HASIL) {
                    hasil_timer++;
                    if (hasil_timer >= 70) {
                        if (mode_lempar == 1) {
                            // Selesaikan lempar penentu
                            if (pen_giliran == 1) {
                                pen1 = roll_skg;
                                pen_giliran = 2;
                                fase = F_PEN_TUNGGU;
                            } else {
                                pen2 = roll_skg;
                                if (pen1 == pen2) {
                                    hasil_timer = 0;
                                    fase = F_PEN_PESAN;
                                } else {
                                    giliran = (pen1 > pen2) ? 1 : 2;
                                    mode_lempar = 0;
                                    ganti_tekstur(gsGlobal, board_pixels, 640, 448);
                                    sprintf(pesan_hasil, "PEMAIN %d JALAN DULUAN", giliran);
                                    hasil_timer = 110;
                                    fase = F_TUNGGU;
                                }
                            }
                        } else {
                            ganti_tekstur(gsGlobal, board_pixels, 640, 448);
                            hasil_kode = hitung_hasil(pos[giliran], roll_skg,
                                                       aturan_mulai6, aturan_finis,
                                                       &landing_skg, &final_skg);
                            if (hasil_kode != 2 && aturan_bonus6 && roll_skg == 6 && enam_beruntun >= 2) {
                                // Penyeimbang terdokumentasi: 3x angka 6 beruntun
                                // membatalkan seluruh giliran ini.
                                pos[giliran] = posisi_awal_rantai;
                                vis[giliran] = posisi_awal_rantai;
                                enam_beruntun = 0;
                                giliran = 3 - giliran;
                                sprintf(pesan_hasil, "3X 6 BERTURUT - GILIRAN BATAL");
                                akhiri_tanpa_gerak(1);
                            } else if (hasil_kode == 0) {
                                if (pos[giliran] == 0 && aturan_mulai6 && roll_skg < 6)
                                    sprintf(pesan_hasil, "HARUS DAPAT 6 DULU");
                                else
                                    sprintf(pesan_hasil, "LEWAT 100 - TETAP DI TEMPAT");
                                akhiri_tanpa_gerak(0);
                            } else {
                                jalan_target = landing_skg;
                                jalan_lewat100 = (pos[giliran] + roll_skg > 100 &&
                                                  aturan_finis == FINIS_PANTUL);
                                jalan_arah = 1;
                                langkah_timer = 0;
                                fase = F_JALAN;
                            }
                        }
                    }
                } else if (fase == F_PEN_PESAN) {
                    hasil_timer++;
                    if (hasil_timer >= 90) {
                        pen_giliran = 1;
                        fase = F_PEN_TUNGGU;
                    }
                } else if (fase == F_JALAN) {
                    langkah_timer++;
                    int jeda_langkah = set_kecepatan ? 5 : 10;
                    if (langkah_timer >= jeda_langkah) {
                        langkah_timer = 0;
                        vis[giliran] += jalan_arah;
                        if (jalan_lewat100 && vis[giliran] == 100) jalan_arah = -1;
                        if (vis[giliran] == jalan_target) {
                            if (final_skg != landing_skg) {
                                int x1, y1, x2, y2;
                                get_xy_dari_posisi(landing_skg, &x1, &y1);
                                get_xy_dari_posisi(final_skg, &x2, &y2);
                                jalur_dari_x = x1 + 17; jalur_dari_y = y1 + 17;
                                jalur_ke_x = x2 + 17;   jalur_ke_y = y2 + 17;
                                jalur_timer = 0;
                                if (final_skg > landing_skg) getar_untuk(giliran, 0, 60, 12);
                                else getar_untuk(giliran, 1, 200, 25);
                                fase = F_JALUR;
                            } else {
                                finalisasi();
                            }
                        }
                    }
                } else if (fase == F_JALUR) {
                    jalur_timer++;
                    int durasi = set_kecepatan ? 24 : 42;
                    if (jalur_timer >= durasi) {
                        vis[giliran] = final_skg;
                        finalisasi();
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
            static const char *SET_LABEL[3] = {"GETAR STIK", "KECEPATAN", "KEMBALI"};
            for (int i = 0; i < 3; i++) {
                draw_text(gsGlobal, 140, SET_Y[i] - 7, 2, color_teks, SET_LABEL[i]);
                if (i == 0) draw_text(gsGlobal, 420, SET_Y[i] - 7, 2, color_kuning, set_getar ? "HIDUP" : "MATI");
                if (i == 1) draw_text(gsGlobal, 420, SET_Y[i] - 7, 2, color_kuning, set_kecepatan ? "CEPAT" : "NORMAL");
            }
            int cy = SET_Y[set_sel];
            u64 warna_bingkai = ((frame_counter / 20) % 2 == 0)
                ? GS_SETREG_RGBAQ(90, 170, 255, 0x00, 0)
                : GS_SETREG_RGBAQ(170, 225, 255, 0x00, 0);
            draw_bingkai(gsGlobal, 126, cy - 17, 514, cy + 17, warna_bingkai);
        } else if (layar == LAYAR_ATURAN) {
            gsKit_clear(gsGlobal, color_navy);
            gsKit_prim_sprite_texture(gsGlobal, &texLayar,
                                      0, 0, 0, 0,
                                      640, 448, 640, 448,
                                      3, GS_SETREG_RGBAQ(0x80, 0x80, 0x80, 0x00, 0));
            static const char *AT_LABEL[7] = {
                "PEMAIN PERTAMA", "MODEL STIK", "BONUS ANGKA 6",
                "CARA FINIS", "HARUS 6 MULAI", "ATURAN RESMI", "MULAI"
            };
            static const char *AT_AWAL[4] = {"P1", "P2", "PENENTU", "ACAK"};
            static const char *AT_FINIS[3] = {"TEPAT", "LEWAT", "PANTUL"};
            const char *nilai[7];
            nilai[0] = AT_AWAL[aturan_awal];
            nilai[1] = (aturan_model == MODEL_DUA) ? "DUA STIK" : "BERBAGI";
            nilai[2] = aturan_bonus6 ? "HIDUP" : "MATI";
            nilai[3] = AT_FINIS[aturan_finis];
            nilai[4] = aturan_mulai6 ? "HIDUP" : "MATI";
            nilai[5] = "TEKAN X";
            nilai[6] = "";
            for (int i = 0; i < 7; i++) {
                draw_text(gsGlobal, 104, ATURAN_Y[i] - 7, 2, color_teks, AT_LABEL[i]);
                if (nilai[i][0])
                    draw_text(gsGlobal, 376, ATURAN_Y[i] - 7, 2,
                              (i == 6) ? color_teks : color_kuning, nilai[i]);
            }
            int cy = ATURAN_Y[aturan_sel];
            u64 warna_bingkai = ((frame_counter / 20) % 2 == 0)
                ? GS_SETREG_RGBAQ(90, 170, 255, 0x00, 0)
                : GS_SETREG_RGBAQ(170, 225, 255, 0x00, 0);
            draw_bingkai(gsGlobal, 84, cy - 15, 556, cy + 15, warna_bingkai);
        } else if (fase_di_layar_dadu(fase)) { // LAYAR_GAME di layar dadu
            gsKit_clear(gsGlobal, color_navy);
            gsKit_prim_sprite_texture(gsGlobal, &texLayar,
                                      0, 0, 0, 0,
                                      640, 448, 640, 448,
                                      3, GS_SETREG_RGBAQ(0x80, 0x80, 0x80, 0x00, 0));
            char skor[8];
            sprintf(skor, "%d", vis[1]);
            draw_text(gsGlobal, 205 - ((int)strlen(skor) * 18 - 3) / 2, 23, 3, color_teks, skor);
            sprintf(skor, "%d", vis[2]);
            draw_text(gsGlobal, 435 - ((int)strlen(skor) * 18 - 3) / 2, 23, 3, color_teks, skor);
            int aktif = (mode_lempar == 1) ? pen_giliran : giliran;
            if ((frame_counter / 20) % 2 == 0) {
                if (aktif == 1) draw_bingkai(gsGlobal, 10, 8, 280, 58, color_emas);
                else draw_bingkai(gsGlobal, 360, 8, 630, 58, color_emas);
            }
            // Dadu besar di tengah
            int wajah = (fase == F_DADU) ? wajah_skg : (fase == F_PEN_TUNGGU ? wajah_skg : roll_skg);
            int ukuran = 168, jx = 0, jy = 0;
            if (fase == F_DADU) {
                ukuran = 168 + (int)(10.0f * sinf((float)dadu_timer * 0.35f));
                jx = (int)((frame_counter * 37) % 13) - 6;
                jy = (int)((frame_counter * 53) % 11) - 5;
            } else if (fase == F_PEN_TUNGGU) {
                ukuran = 150;
            }
            if (wajah_ok) {
                gsKit_prim_sprite_texture(gsGlobal, &texWajah[wajah - 1],
                                          320 + jx - ukuran/2, 212 + jy - ukuran/2, 0, 0,
                                          320 + jx + ukuran/2, 212 + jy + ukuran/2, 128, 128,
                                          2, GS_SETREG_RGBAQ(0x80, 0x80, 0x80, 0x00, 0));
            } else {
                draw_wajah_primitif(gsGlobal, wajah, 320 + jx, 212 + jy, ukuran, color_putih, color_dot);
            }
            // Banner layar dadu
            char bt[48];
            if (fase == F_PEN_TUNGGU) {
                sprintf(bt, "PENENTU - PEMAIN %d TEKAN X", pen_giliran);
                draw_text_tengah(gsGlobal, 320, 409, 2, color_teks, bt);
            } else if (fase == F_PEN_PESAN) {
                draw_text_tengah(gsGlobal, 320, 409, 2, color_kuning, "SERI - LEMPAR LAGI");
            } else if (fase == F_DADU) {
                sprintf(bt, "PEMAIN %d MELEMPAR DADU...", aktif);
                draw_text_tengah(gsGlobal, 320, 409, 2, color_teks, bt);
            } else {
                sprintf(bt, "PEMAIN %d DAPAT %d!", aktif, roll_skg);
                draw_text_tengah(gsGlobal, 320, 409, 2, color_kuning, bt);
            }
            // Menu jeda juga bisa dibuka di layar dadu
            if (jeda_aktif) {
                static const char *JEDA_LABEL2[4] = {"LANJUT", "PENGATURAN", "ULANGI", "KELUAR"};
                draw_panel(gsGlobal, 170, 104, 470, 336, color_panel, color_biru);
                draw_text_tengah(gsGlobal, 320, 122, 3, color_teks, "JEDA");
                for (int i = 0; i < 4; i++) {
                    draw_text_tengah(gsGlobal, 320, JEDA_Y[i], 2,
                                     (i == jeda_sel) ? color_kuning : color_teks, JEDA_LABEL2[i]);
                }
                u64 wb = ((frame_counter / 20) % 2 == 0)
                    ? GS_SETREG_RGBAQ(90, 170, 255, 0x00, 0)
                    : GS_SETREG_RGBAQ(170, 225, 255, 0x00, 0);
                draw_bingkai(gsGlobal, 196, JEDA_Y[jeda_sel] - 6, 444, JEDA_Y[jeda_sel] + 20, wb);
            }
        } else { // LAYAR_GAME di papan
            gsKit_clear(gsGlobal, color_navy);
            gsKit_prim_sprite_texture(gsGlobal, &texLayar,
                                      0, 0, 0, 0,
                                      640, 448, 640, 448,
                                      3, GS_SETREG_RGBAQ(0x80, 0x80, 0x80, 0x00, 0));

            if (pemenang == 0 && fase == F_TUNGGU && (frame_counter / 20) % 2 == 0) {
                if (giliran == 1) draw_bingkai(gsGlobal, 10, 8, 280, 58, color_emas);
                else draw_bingkai(gsGlobal, 360, 8, 630, 58, color_emas);
            }

            char skor[8];
            sprintf(skor, "%d", vis[1]);
            draw_text(gsGlobal, 205 - ((int)strlen(skor) * 18 - 3) / 2, 23, 3, color_teks, skor);
            sprintf(skor, "%d", vis[2]);
            draw_text(gsGlobal, 435 - ((int)strlen(skor) * 18 - 3) / 2, 23, 3, color_teks, skor);

            // Bidak menurut posisi visual (animasi jalan/jalur tercermin di sini)
            for (int p = 1; p <= 2; p++) {
                if (vis[p] <= 0) continue;
                int lawan = 3 - p;
                float cx, cy;
                if (fase == F_JALUR && p == giliran) {
                    int durasi = set_kecepatan ? 24 : 42;
                    float t = (float)jalur_timer / (float)durasi;
                    if (t > 1.0f) t = 1.0f;
                    cx = jalur_dari_x + (jalur_ke_x - jalur_dari_x) * t;
                    cy = jalur_dari_y + (jalur_ke_y - jalur_dari_y) * t;
                    draw_bidak(gsGlobal, cx, cy, 0.72f,
                               (p == 1) ? color_p1_terang : color_p2_terang,
                               (p == 1) ? color_p1 : color_p2,
                               (p == 1) ? color_p1_gelap : color_p2_gelap);
                } else if (vis[p] == vis[lawan] && vis[lawan] > 0) {
                    int bx, by;
                    get_xy_dari_posisi(vis[p], &bx, &by);
                    draw_bidak(gsGlobal, bx + (p == 1 ? 10 : 24), by + 17, 0.55f,
                               (p == 1) ? color_p1_terang : color_p2_terang,
                               (p == 1) ? color_p1 : color_p2,
                               (p == 1) ? color_p1_gelap : color_p2_gelap);
                } else {
                    int bx, by;
                    get_xy_dari_posisi(vis[p], &bx, &by);
                    draw_bidak(gsGlobal, bx + 17, by + 17, 0.72f,
                               (p == 1) ? color_p1_terang : color_p2_terang,
                               (p == 1) ? color_p1 : color_p2,
                               (p == 1) ? color_p1_gelap : color_p2_gelap);
                }
            }

            if (roll_skg > 0) draw_dadu(gsGlobal, roll_skg, color_dot);

            // Banner bawah papan
            if (pemenang == 0 && !jeda_aktif) {
                if (fase == F_PESAN || (fase == F_TUNGGU && hasil_timer > 0)) {
                    draw_text_tengah(gsGlobal, 320, 409, 2, color_teks, pesan_hasil);
                } else if (fase == F_JALAN) {
                    char bf[24];
                    sprintf(bf, "P%d MAJU %d", giliran, roll_skg);
                    draw_text_tengah(gsGlobal, 320, 409, 2, color_teks, bf);
                } else if (fase == F_JALUR) {
                    draw_text_tengah(gsGlobal, 320, 409, 2, color_kuning,
                                     (final_skg > landing_skg) ? "TANGGA!" : "ULAR!");
                } else if (fase == F_TUNGGU) {
                    char kiri[32];
                    sprintf(kiri, "GILIRAN PEMAIN %d - TEKAN ", giliran);
                    const char *kanan = " UNTUK MELEMPAR DADU";
                    int wk = lebar_teks(kiri, 2);
                    int wn = lebar_teks(kanan, 2);
                    int sx = 320 - (wk + 26 + wn) / 2;
                    draw_text(gsGlobal, sx, 409, 2, color_teks, kiri);
                    draw_elips(gsGlobal, sx + wk + 13, 416, 11, 11, 2, GS_SETREG_RGBAQ(47, 127, 224, 0x00, 0));
                    draw_text(gsGlobal, sx + wk + 8, 409, 2, color_teks, "X");
                    draw_text(gsGlobal, sx + wk + 26, 409, 2, color_teks, kanan);
                }
            }

            // Menu jeda di atas papan
            if (jeda_aktif) {
                static const char *JEDA_LABEL[4] = {"LANJUT", "PENGATURAN", "ULANGI", "KELUAR"};
                draw_panel(gsGlobal, 170, 104, 470, 336, color_panel, color_biru);
                draw_text_tengah(gsGlobal, 320, 122, 3, color_teks, "JEDA");
                for (int i = 0; i < 4; i++) {
                    draw_text_tengah(gsGlobal, 320, JEDA_Y[i], 2,
                                     (i == jeda_sel) ? color_kuning : color_teks, JEDA_LABEL[i]);
                }
                u64 wb = ((frame_counter / 20) % 2 == 0)
                    ? GS_SETREG_RGBAQ(90, 170, 255, 0x00, 0)
                    : GS_SETREG_RGBAQ(170, 225, 255, 0x00, 0);
                draw_bingkai(gsGlobal, 196, JEDA_Y[jeda_sel] - 6, 444, JEDA_Y[jeda_sel] + 20, wb);
            }

            // Layar pemenang di atas papan
            if (pemenang != 0) {
                static const char *MENANG_LABEL[3] = {"MAIN LAGI", "UBAH ATURAN", "MENU UTAMA"};
                draw_panel(gsGlobal, 110, 84, 530, 368, color_panel, color_biru);
                char wt[24];
                sprintf(wt, "PEMAIN %d MENANG!", pemenang);
                draw_text_tengah(gsGlobal, 320, 102, 3, color_kuning, wt);
                char st[40];
                sprintf(st, "LEMPARAN: %d", stat_lemparan[pemenang]);
                draw_text(gsGlobal, 150, 158, 2, color_teks, st);
                sprintf(st, "TANGGA: %d", stat_tangga[pemenang]);
                draw_text(gsGlobal, 150, 184, 2, color_teks, st);
                sprintf(st, "ULAR: %d", stat_ular[pemenang]);
                draw_text(gsGlobal, 150, 210, 2, color_teks, st);
                sprintf(st, "POSISI: P1 %d - P2 %d", pos[1], pos[2]);
                draw_text(gsGlobal, 150, 236, 2, color_teks, st);
                for (int i = 0; i < 3; i++) {
                    draw_text_tengah(gsGlobal, 320, MENANG_Y[i], 2,
                                     (i == menang_sel) ? color_kuning : color_teks, MENANG_LABEL[i]);
                }
                u64 wb = ((frame_counter / 20) % 2 == 0)
                    ? GS_SETREG_RGBAQ(90, 170, 255, 0x00, 0)
                    : GS_SETREG_RGBAQ(170, 225, 255, 0x00, 0);
                draw_bingkai(gsGlobal, 196, MENANG_Y[menang_sel] - 6, 444, MENANG_Y[menang_sel] + 20, wb);
            }
        }

        gsKit_queue_exec(gsGlobal);
        gsKit_sync_flip(gsGlobal);
    }
    return 0;
}
