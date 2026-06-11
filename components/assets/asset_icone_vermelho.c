#include "asset_icone_vermelho.h"

/* 16x16 px, RGB565A8: 512 bytes RGB + 256 bytes alpha = 768 bytes total.
 * Gerado de ATT_CAIO/VERMELHO.png (redimensionado 32x32 -> 16x16).
 * Embarcado em flash via EMBED_FILES no CMakeLists.txt do componente assets. */
extern const uint8_t icone_vermelho_rgb565a8_bin_start[]
    asm("_binary_icone_vermelho_rgb565a8_bin_start");
extern const uint8_t icone_vermelho_rgb565a8_bin_end[]
    asm("_binary_icone_vermelho_rgb565a8_bin_end");

#define ICONE_VM_W       16
#define ICONE_VM_H       16
#define ICONE_VM_STRIDE  (ICONE_VM_W * 2)

static lv_image_dsc_t s_dsc;
static bool           s_inited = false;

const lv_image_dsc_t *asset_icone_vermelho_get_dsc(void)
{
    if (!s_inited) {
        s_dsc.header.magic  = LV_IMAGE_HEADER_MAGIC;
        s_dsc.header.cf     = LV_COLOR_FORMAT_RGB565A8;
        s_dsc.header.flags  = 0;
        s_dsc.header.w      = ICONE_VM_W;
        s_dsc.header.h      = ICONE_VM_H;
        s_dsc.header.stride = ICONE_VM_STRIDE;
        s_dsc.data_size     = (size_t)(icone_vermelho_rgb565a8_bin_end -
                                       icone_vermelho_rgb565a8_bin_start);
        s_dsc.data          = icone_vermelho_rgb565a8_bin_start;
        s_inited = true;
    }
    return &s_dsc;
}
