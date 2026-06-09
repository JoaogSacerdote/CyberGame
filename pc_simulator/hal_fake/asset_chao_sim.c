/* Stub do simulador: asset_chao usa EMBED_FILES do ESP-IDF (binário em flash),
 * mecanismo que não existe no MinGW. O chão não é renderizado no simulador. */
#include "asset_chao.h"

const lv_image_dsc_t *asset_chao_get_dsc(void)
{
    return NULL;
}
