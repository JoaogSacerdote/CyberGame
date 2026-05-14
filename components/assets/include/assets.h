#pragma once

/* API publica do componente assets. Ponto unico de entrada — outros
 * componentes incluem APENAS este header.
 *
 * Os arquivos abaixo sao GERADOS por tools/convert_assets.py:
 *   assets_images.h: declaracoes extern dos lv_image_dsc_t
 *   assets_meta.h:   offsets (off_x, off_y, w, h) cropados de cada imagem
 *
 * Quando regerar os PNGs em assets/sprites/, rodar:
 *   python tools/convert_assets.py
 *
 * Para usar uma imagem cropada na posicao original, posicionar com
 * lv_obj_set_pos(img, META.off_x, META.off_y) — o cropping deslocou a
 * imagem ao bounding box dos pixels visiveis, off_x/off_y restauram.
 */

#include "assets_images.h"
#include "assets_meta.h"
