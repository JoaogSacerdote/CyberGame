/* Gerado por tools/extract_collisions.py — NAO EDITE A MAO */
#include "collision_data.h"

/* 31 obstaculos extraidos de PAREDES_E_OBJETOS_02 (tile-based 16px) */
const collision_rect_t collision_empresa_obstaculos[] = {
    { .x =   0, .y =   0, .w = 480, .h =  64, .kind = OBSTACULO },
    { .x =   0, .y =  64, .w =  64, .h =  16, .kind = OBSTACULO },
    { .x = 128, .y =  64, .w =  32, .h =  16, .kind = OBSTACULO },
    { .x = 224, .y =  64, .w = 144, .h =  16, .kind = OBSTACULO },
    { .x = 416, .y =  64, .w =  64, .h =  32, .kind = OBSTACULO },
    { .x =   0, .y =  80, .w =  32, .h =  48, .kind = OBSTACULO },
    { .x =  64, .y =  80, .w = 208, .h =  96, .kind = OBSTACULO },
    { .x = 304, .y =  80, .w =  48, .h =  16, .kind = OBSTACULO },
    { .x = 304, .y =  96, .w =  16, .h =  32, .kind = OBSTACULO },
    { .x = 464, .y =  96, .w =  16, .h =  32, .kind = OBSTACULO },
    { .x = 304, .y = 128, .w =  64, .h =  32, .kind = OBSTACULO },
    { .x = 400, .y = 128, .w =  80, .h =  32, .kind = OBSTACULO },
    { .x = 432, .y = 160, .w =  48, .h =  32, .kind = OBSTACULO },
    { .x =  96, .y = 176, .w =  48, .h =  16, .kind = OBSTACULO },
    { .x = 208, .y = 176, .w =  32, .h =  32, .kind = OBSTACULO },
    { .x =   0, .y = 192, .w =  32, .h =  80, .kind = OBSTACULO },
    { .x = 112, .y = 192, .w =  32, .h =  16, .kind = OBSTACULO },
    { .x = 304, .y = 192, .w = 176, .h =  32, .kind = OBSTACULO },
    { .x = 304, .y = 224, .w =  48, .h =  16, .kind = OBSTACULO },
    { .x = 384, .y = 224, .w =  48, .h =  32, .kind = OBSTACULO },
    { .x = 464, .y = 224, .w =  16, .h =  16, .kind = OBSTACULO },
    { .x = 320, .y = 240, .w =  16, .h =  16, .kind = OBSTACULO },
    { .x = 448, .y = 240, .w =  32, .h =  16, .kind = OBSTACULO },
    { .x = 384, .y = 256, .w =  96, .h =  32, .kind = OBSTACULO },
    { .x =   0, .y = 272, .w =  96, .h =  48, .kind = OBSTACULO },
    { .x = 144, .y = 272, .w =  48, .h =  48, .kind = OBSTACULO },
    { .x = 304, .y = 272, .w =  32, .h =  16, .kind = OBSTACULO },
    { .x = 304, .y = 288, .w =  48, .h =  16, .kind = OBSTACULO },
    { .x = 384, .y = 288, .w =  48, .h =  16, .kind = OBSTACULO },
    { .x = 464, .y = 288, .w =  16, .h =  32, .kind = OBSTACULO },
    { .x = 272, .y = 304, .w =  80, .h =  16, .kind = OBSTACULO },
};
const size_t collision_empresa_obstaculos_count = sizeof(collision_empresa_obstaculos) / sizeof(collision_empresa_obstaculos[0]);

/* Areas de gatilho (extraidas dos arquivos _NULL) */
const collision_rect_t collision_empresa_gatilhos[] = {
    { .x = 323, .y = 110, .w =  31, .h =  21, .kind = AREA_INTERACAO_NPC_TI_BAIXO }, /* NPC TI olhando baixo (AREA_DE_INTERACAO_BAIXO_DO_NPC_TI_NULL) */
    { .x = 357, .y =  88, .w =  35, .h =  32, .kind = AREA_INTERACAO_NPC_TI_DIREITA }, /* NPC TI olhando direita (AREA_DE_INTERACAO_DIREITA_DO_NPC_TI_NULL) */
    { .x = 354, .y = 225, .w =  39, .h =  61, .kind = AREA_TAREFA_VERDE }, /* Tarefa verde (AREA_DE_INTERACAO_PARA_TAREFA_VERDE_NULL) */
    { .x =   0, .y = 128, .w =  15, .h =  64, .kind = AREA_PORTA_RECEPCAO }, /* Saida pra Recepcao (AREA_PORTA_RECEPCAO_NULL) */
    { .x = 144, .y =  66, .w =  32, .h =  31, .kind = AREA_SPAWN }, /* Spawn do player (SPAWN_NULL) */
};
const size_t collision_empresa_gatilhos_count = sizeof(collision_empresa_gatilhos) / sizeof(collision_empresa_gatilhos[0]);