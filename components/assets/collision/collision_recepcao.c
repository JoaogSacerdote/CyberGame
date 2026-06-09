/* Derivado de assets/layout/recepcao.json + INTERACOES.txt
 * Obstaculos: rect = (pivot.x + coll_offset_x, pivot.y + coll_offset_y, coll_w, coll_h)
 * Gatilhos:   pivot bottom-center → top_left_x = pivot_x - w/2, top_left_y = pivot_y - h */
#include "collision_data.h"

/* 15 obstaculos derivados das entidades solidas de recepcao.json */
const collision_rect_t collision_recepcao_obstaculos[] = {
    /* ---- paredes perimetrais ---- */
    { .x =  15, .y =   0, .w = 448, .h =  65, .kind = OBSTACULO }, /* parede_superior         */
    { .x =   0, .y =   0, .w =  16, .h = 319, .kind = OBSTACULO }, /* parede_esquerda         */
    { .x = 463, .y =   0, .w =  17, .h = 149, .kind = OBSTACULO }, /* parede_direita_superior */
    { .x = 463, .y = 219, .w =  17, .h = 100, .kind = OBSTACULO }, /* parede_direita_inferior */
    /* ---- mobiliario ---- */
    { .x =  15, .y = 242, .w = 112, .h =  77, .kind = OBSTACULO }, /* mesa_l_recepcao_01      */
    { .x = 111, .y = 299, .w =  32, .h =  20, .kind = OBSTACULO }, /* cadeira_esquerda_01     */
    { .x = 383, .y = 212, .w =  32, .h =  21, .kind = OBSTACULO }, /* banco_p_01_a            */
    { .x = 335, .y = 212, .w =  32, .h =  21, .kind = OBSTACULO }, /* banco_p_01_b            */
    { .x = 336, .y = 231, .w =  80, .h =  40, .kind = OBSTACULO }, /* mesa_centro_01          */
    { .x = 335, .y = 287, .w =  80, .h =  32, .kind = OBSTACULO }, /* sofa_costa_03           */
    { .x = 432, .y = 297, .w =  20, .h =  12, .kind = OBSTACULO }, /* planta_vaso_01          */
    { .x =  19, .y =  58, .w =  22, .h =  21, .kind = OBSTACULO }, /* planta_vaso_02          */
    { .x =  49, .y =  57, .w =  60, .h =  22, .kind = OBSTACULO }, /* estante_01              */
    { .x = 345, .y =  61, .w =  24, .h =  18, .kind = OBSTACULO }, /* lixeira_01              */
    { .x = 374, .y =  49, .w =  26, .h =  30, .kind = OBSTACULO }, /* bebedouro_01            */
};
const size_t collision_recepcao_obstaculos_count = sizeof(collision_recepcao_obstaculos) / sizeof(collision_recepcao_obstaculos[0]);

/* Gatilhos — fonte: INTERACOES.txt, pivot bottom-center → top-left */
const collision_rect_t collision_recepcao_gatilhos[] = {
    /* INTERACAO_PARA_IR_DA_RECEPCAO_PARA_ESCROITORIO: 10x54 pivot(474,204) → tl(469,150) */
    { .x = 469, .y = 150, .w =  10, .h =  54, .kind = AREA_PORTA_EMPRESA },
    /* INTERACAO_RECEPCIONISTA: 144x112 pivot(88,319) → tl(16,207) */
    { .x =  16, .y = 207, .w = 144, .h = 112, .kind = AREA_INTERACAO_NPC },
};
const size_t collision_recepcao_gatilhos_count = sizeof(collision_recepcao_gatilhos) / sizeof(collision_recepcao_gatilhos[0]);
