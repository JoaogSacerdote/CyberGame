/* Gerado por tools/extract_collisions.py — NAO EDITE A MAO */
#include "collision_data.h"

/* 23 obstaculos extraidos de PAREDES_E_OBJETOS_02 (tile-based 16px) */
const collision_rect_t collision_recepcao_obstaculos[] = {
    { .x =   0, .y =   0, .w = 480, .h =  64, .kind = OBSTACULO },
    { .x =   0, .y =  64, .w = 112, .h =  16, .kind = OBSTACULO },
    { .x = 336, .y =  64, .w =  64, .h =  16, .kind = OBSTACULO },
    { .x = 464, .y =  64, .w =  16, .h =  48, .kind = OBSTACULO },
    { .x =   0, .y =  80, .w =  16, .h = 144, .kind = OBSTACULO },
    { .x = 448, .y = 112, .w =  32, .h =  48, .kind = OBSTACULO },
    { .x = 464, .y = 176, .w =  16, .h =  16, .kind = OBSTACULO },
    { .x = 336, .y = 192, .w =  32, .h =  32, .kind = OBSTACULO },
    { .x = 384, .y = 192, .w =  32, .h =  32, .kind = OBSTACULO },
    { .x = 448, .y = 192, .w =  32, .h =  32, .kind = OBSTACULO },
    { .x =   0, .y = 224, .w = 128, .h =  16, .kind = OBSTACULO },
    { .x = 336, .y = 224, .w =  80, .h =  16, .kind = OBSTACULO },
    { .x = 464, .y = 224, .w =  16, .h =  64, .kind = OBSTACULO },
    { .x =   0, .y = 240, .w = 144, .h =  32, .kind = OBSTACULO },
    { .x = 320, .y = 240, .w =  96, .h =  16, .kind = OBSTACULO },
    { .x = 336, .y = 256, .w =  80, .h =  16, .kind = OBSTACULO },
    { .x =   0, .y = 272, .w =  64, .h =  48, .kind = OBSTACULO },
    { .x = 112, .y = 272, .w =  16, .h =  16, .kind = OBSTACULO },
    { .x = 320, .y = 272, .w =  96, .h =  48, .kind = OBSTACULO },
    { .x =  96, .y = 288, .w =  32, .h =  16, .kind = OBSTACULO },
    { .x = 432, .y = 288, .w =  48, .h =  16, .kind = OBSTACULO },
    { .x = 432, .y = 304, .w =  16, .h =  16, .kind = OBSTACULO },
    { .x = 464, .y = 304, .w =  16, .h =  16, .kind = OBSTACULO },
};
const size_t collision_recepcao_obstaculos_count = sizeof(collision_recepcao_obstaculos) / sizeof(collision_recepcao_obstaculos[0]);

/* Areas de gatilho (extraidas dos arquivos _NULL) */
const collision_rect_t collision_recepcao_gatilhos[] = {
    { .x = 135, .y = 254, .w =  40, .h =  51, .kind = AREA_PORTA_EMPRESA }, /* Saida pra Empresa (AREA_PARA_ACESSAR_ESCRITORIO_NULL) */
    { .x = 137, .y = 261, .w =  41, .h =  53, .kind = AREA_INTERACAO_NPC }, /* Recepcionista (AREA_QUE_LIBERA_INTERACAO_COM_RECEPCIONISTA_NULL) */
};
const size_t collision_recepcao_gatilhos_count = sizeof(collision_recepcao_gatilhos) / sizeof(collision_recepcao_gatilhos[0]);