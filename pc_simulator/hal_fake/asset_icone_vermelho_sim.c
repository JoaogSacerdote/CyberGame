/* Stub do simulador: asset_icone_vermelho usa EMBED_FILES do ESP-IDF (binário
 * em flash), mecanismo que não existe no MinGW. Aqui o ícone é um quadrado
 * vermelho 16x16 gerado em runtime — suficiente pra ver o blink de alerta
 * dos servidores durante os testes de gameplay. */
#include "asset_icone_vermelho.h"

#define ICONE_VM_W       16
#define ICONE_VM_H       16
#define ICONE_VM_STRIDE  (ICONE_VM_W * 2)
#define ICONE_VM_PIXELS  (ICONE_VM_W * ICONE_VM_H)

/* RGB565A8: plano RGB565 (2 B/px, little-endian) + plano A8 contíguo. */
static uint8_t        s_buf[ICONE_VM_PIXELS * 2 + ICONE_VM_PIXELS];
static lv_image_dsc_t s_dsc;
static bool           s_inited = false;

const lv_image_dsc_t *asset_icone_vermelho_get_dsc(void)
{
    if (!s_inited) {
        for (int i = 0; i < ICONE_VM_PIXELS; i++) {
            s_buf[i * 2]     = 0x00;   /* RGB565 0xF800 (vermelho), LE */
            s_buf[i * 2 + 1] = 0xF8;
            s_buf[ICONE_VM_PIXELS * 2 + i] = 0xFF;   /* alpha opaco */
        }
        s_dsc.header.magic  = LV_IMAGE_HEADER_MAGIC;
        s_dsc.header.cf     = LV_COLOR_FORMAT_RGB565A8;
        s_dsc.header.flags  = 0;
        s_dsc.header.w      = ICONE_VM_W;
        s_dsc.header.h      = ICONE_VM_H;
        s_dsc.header.stride = ICONE_VM_STRIDE;
        s_dsc.data_size     = sizeof(s_buf);
        s_dsc.data          = s_buf;
        s_inited = true;
    }
    return &s_dsc;
}
