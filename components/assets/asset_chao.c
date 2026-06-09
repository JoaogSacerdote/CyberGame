#include "asset_chao.h"

/* Pixels RGB565 puros embutidos em flash via EMBED_FILES no CMakeLists.txt.
 * Gerado de CHAO.png (464x254, crop=False) por tools/asset_codec.py. */
extern const uint8_t chao_rgb565_bin_start[] asm("_binary_chao_rgb565_bin_start");
extern const uint8_t chao_rgb565_bin_end[]   asm("_binary_chao_rgb565_bin_end");

#define CHAO_W      464
#define CHAO_H      254
#define CHAO_STRIDE (CHAO_W * 2)  /* bytes por linha RGB565 */

static lv_image_dsc_t s_dsc;
static bool           s_inited = false;

const lv_image_dsc_t *asset_chao_get_dsc(void)
{
    if (!s_inited) {
        s_dsc.header.magic  = LV_IMAGE_HEADER_MAGIC;
        s_dsc.header.cf     = LV_COLOR_FORMAT_RGB565;
        s_dsc.header.flags  = 0;
        s_dsc.header.w      = CHAO_W;
        s_dsc.header.h      = CHAO_H;
        s_dsc.header.stride = CHAO_STRIDE;
        s_dsc.data_size     = (size_t)(chao_rgb565_bin_end - chao_rgb565_bin_start);
        s_dsc.data          = chao_rgb565_bin_start;
        s_inited = true;
    }
    return &s_dsc;
}
