/* TES TEKSTUR v2 - Ular Tangga PS2
 * Dibangun ulang di atas profil init REPO (perbaikan user 29 Okt malam,
 * commit 1d5645ef): Mode = gsKit_check_rom() (PAL 512 / NTSC 448),
 * dmaKit_init dengan 1 << DMA_CHANNEL_GIF, PrimAlphaEnable OFF,
 * mode_switch GS_PERSISTENT, modul stik dimuat SETELAH layar nyala.
 * Versi v1 (profil workspace lama: NTSC paksa + DMA 1<<0 + alpha ON)
 * terbukti BLANK HITAM di PCSX2 - profil lama itulah yang dibuang di sini.
 * Tujuan tunggal: papan ilustrasi 512x512 tampil layar penuh.
 * Sinyal: HIJAU TERANG tanpa papan = loop jalan, tekstur belum tampil;
 * MERAH = alokasi VRAM gagal. Keluar: Reset. */
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

int main(int argc, char *argv[]) {
    /* 1. RPC sederhana (pola repo) */
    SifInitRpc(0);
    SifLoadFileInit();
    printf("[LOG TES TEKSTUR]: Program EE berhasil dipicu.\n");

    /* 2. gsKit global + mode dari ROM (PAL/NTSC adaptif, pola repo) */
    GSGLOBAL *gsGlobal = gsKit_init_global();
    gsGlobal->Mode = gsKit_check_rom();
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

    /* 3. DMA jalur GIF (perbaikan utama repo) */
    dmaKit_init(D_CTRL_RELE_OFF, D_CTRL_MFD_OFF, D_CTRL_STS_UNSPEC, D_CTRL_STD_OFF, D_CTRL_RCYC_8, 1 << DMA_CHANNEL_GIF);
    dmaKit_chan_init(DMA_CHANNEL_GIF);

    gsKit_init_screen(gsGlobal);
    gsKit_mode_switch(gsGlobal, GS_PERSISTENT);
    gsKit_vram_clear(gsGlobal);
    printf("[LOG TES TEKSTUR]: gsKit Screen Berhasil Menyala.\n");

    /* 4. Modul stik SETELAH layar (urutan repo) */
    SifLoadModule("rom0:XSIO2MAN", 0, NULL);
    SifLoadModule("rom0:XPADMAN", 0, NULL);
    padInit(0);
    padPortOpen(0, 0, padBuf);

    /* 5. Tekstur papan: alokasi VRAM + unggah (pola loader gsToolkit) */
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

    u64 color_bg  = GS_SETREG_RGBAQ(0, 255, 0, 0x00, 0);
    u64 color_err = GS_SETREG_RGBAQ(255, 0, 0, 0x00, 0);
    u64 tex_color = GS_SETREG_RGBAQ(0x80, 0x80, 0x80, 0x00, 0x00);

    if (boardTex.Vram == GSKIT_ALLOC_ERROR) {
        while (1) {
            gsKit_vsync_wait();
            gsKit_clear(gsGlobal, color_err);
            gsKit_queue_exec(gsGlobal);
            gsKit_sync_flip(gsGlobal);
        }
    }
    gsKit_texture_upload(gsGlobal, &boardTex);
    printf("[LOG TES TEKSTUR]: Tekstur papan terunggah, masuk loop.\n");

    /* 6. Loop gambar (irama repo: vsync -> clear -> gambar -> exec -> flip) */
    while (1) {
        gsKit_vsync_wait();
        gsKit_clear(gsGlobal, color_bg);
        gsKit_prim_sprite_texture(gsGlobal, &boardTex,
                                  0, 0, 0, 0,
                                  gsGlobal->Width, gsGlobal->Height, 512, 512,
                                  1, tex_color);
        gsKit_queue_exec(gsGlobal);
        gsKit_sync_flip(gsGlobal);
    }
    return 0;
}
