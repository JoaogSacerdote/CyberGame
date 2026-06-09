/* Derivado de assets/layout/empresa.json + INTERACOES.txt
 * Obstaculos: rect = (pivot.x + coll_offset_x, pivot.y + coll_offset_y, coll_w, coll_h)
 * Gatilhos:   rect = (pos_x, pos_y, largura, altura) conforme INTERACOES.txt */
#include "collision_data.h"

/* 35 obstaculos derivados das entidades solidas de empresa.json */
const collision_rect_t collision_empresa_obstaculos[] = {
    /* ---- paredes perimetrais ---- */
    { .x =  31, .y =   0, .w = 432, .h =  65, .kind = OBSTACULO }, /* parede_superior_01     */
    { .x =   0, .y =   0, .w =  31, .h =  74, .kind = OBSTACULO }, /* parede_esquerda_sup    */
    { .x =   0, .y = 191, .w =  31, .h = 128, .kind = OBSTACULO }, /* parede_esquerda_inf    */
    { .x = 464, .y =   0, .w =  17, .h = 320, .kind = OBSTACULO }, /* parede_direita         */
    { .x =   0, .y = 113, .w =  32, .h =  14, .kind = OBSTACULO }, /* parede_rec_07          */
    /* ---- divisoria interna (parede_rec_*) ---- */
    { .x = 305, .y = 115, .w =  12, .h =  12, .kind = OBSTACULO }, /* parede_rec_04_a        */
    { .x = 306, .y =  63, .w =  12, .h =  96, .kind = OBSTACULO }, /* parede_rec_02          */
    { .x = 308, .y = 150, .w =  12, .h =   9, .kind = OBSTACULO }, /* parede_rec_03          */
    { .x = 320, .y = 139, .w =  16, .h =  20, .kind = OBSTACULO }, /* parede_rec_06_a        */
    { .x = 336, .y = 139, .w =  32, .h =  20, .kind = OBSTACULO }, /* parede_rec_05_c        */
    { .x = 400, .y = 139, .w =  32, .h =  20, .kind = OBSTACULO }, /* parede_rec_05_b        */
    { .x = 432, .y = 139, .w =  32, .h =  20, .kind = OBSTACULO }, /* parede_rec_05_a        */
    { .x = 308, .y = 211, .w =  12, .h =  12, .kind = OBSTACULO }, /* parede_rec_04_b        */
    { .x = 320, .y = 203, .w =  16, .h =  20, .kind = OBSTACULO }, /* parede_rec_06_b        */
    { .x = 336, .y = 203, .w =  32, .h =  20, .kind = OBSTACULO }, /* parede_rec_05_g        */
    { .x = 368, .y = 203, .w =  32, .h =  20, .kind = OBSTACULO }, /* parede_rec_05_f        */
    { .x = 400, .y = 203, .w =  32, .h =  20, .kind = OBSTACULO }, /* parede_rec_05_e        */
    { .x = 432, .y = 203, .w =  32, .h =  20, .kind = OBSTACULO }, /* parede_rec_05_d        */
    { .x = 306, .y = 223, .w =  12, .h =  48, .kind = OBSTACULO }, /* parede_rec_01          */
    { .x = 308, .y = 307, .w =  12, .h =  12, .kind = OBSTACULO }, /* parede_rec_04_c        */
    /* ---- servidores ---- */
    { .x = 320, .y =  29, .w =  32, .h =  66, .kind = OBSTACULO }, /* servidor_01_b          */
    { .x = 431, .y =  29, .w =  32, .h =  66, .kind = OBSTACULO }, /* servidor_01_a          */
    /* ---- mobiliario ---- */
    { .x =  64, .y = 115, .w = 208, .h =  60, .kind = OBSTACULO }, /* mesa_l_escritorio_01   */
    { .x = 391, .y = 245, .w =  32, .h =  58, .kind = OBSTACULO }, /* mesa_l_escritorio_02   */
    { .x = 143, .y = 287, .w =  48, .h =  32, .kind = OBSTACULO }, /* impressora_01          */
    { .x =  47, .y = 279, .w =  48, .h =  40, .kind = OBSTACULO }, /* cafeteira_01           */
    { .x = 280, .y = 298, .w =  22, .h =  21, .kind = OBSTACULO }, /* planta_vaso_02_a       */
    { .x = 436, .y = 167, .w =  22, .h =  21, .kind = OBSTACULO }, /* planta_vaso_02_b       */
    { .x =  36, .y =  58, .w =  22, .h =  21, .kind = OBSTACULO }, /* planta_vaso_02_c       */
    { .x = 218, .y =  67, .w =  32, .h =  12, .kind = OBSTACULO }, /* planta_vaso_03         */
    { .x = 252, .y =  49, .w =  26, .h =  30, .kind = OBSTACULO }, /* bebedouro_01           */
    { .x = 431, .y = 271, .w =  32, .h =  16, .kind = OBSTACULO }, /* cadeira_esquerda_02_a  */
    { .x = 117, .y =  79, .w =  32, .h =  16, .kind = OBSTACULO }, /* cadeira_esquerda_02_b  */
    { .x = 217, .y = 187, .w =  32, .h =  20, .kind = OBSTACULO }, /* cadeira_costa_01_a     */
    { .x = 111, .y = 187, .w =  32, .h =  20, .kind = OBSTACULO }, /* cadeira_costa_01_b     */
};
const size_t collision_empresa_obstaculos_count = sizeof(collision_empresa_obstaculos) / sizeof(collision_empresa_obstaculos[0]);

/* Gatilhos — fonte: INTERACOES.txt, formato pivot bottom-center → top-left
 * top_left_x = pivot_x - w/2,  top_left_y = pivot_y - h */
const collision_rect_t collision_empresa_gatilhos[] = {
    /* RETORNO_RECEPCAO:           12x64  pivot(6,191)   → tl(0,127)  */
    { .x =   0, .y = 127, .w =  12, .h =  64, .kind = AREA_PORTA_RECEPCAO },
    /* INTERAÇCAO_COMPUTADOR_SALA: 16x58  pivot(383,298) → tl(375,240) */
    { .x = 375, .y = 240, .w =  16, .h =  58, .kind = AREA_TAREFA_VERDE },
    /* SERVIDOR_ESQUERDA:          32x16  pivot(336,111) → tl(320,95)  */
    { .x = 320, .y =  95, .w =  32, .h =  16, .kind = AREA_SERVIDOR },
    /* SERVIDOR_DIREITA:           32x16  pivot(447,111) → tl(431,95)  */
    { .x = 431, .y =  95, .w =  32, .h =  16, .kind = AREA_SERVIDOR },
    /* INTERACAO_NPC_SERVIDOR:     22x26  pivot(363,127) → tl(352,101) */
    { .x = 352, .y = 101, .w =  22, .h =  26, .kind = AREA_INTERACAO_NPC },
};
const size_t collision_empresa_gatilhos_count = sizeof(collision_empresa_gatilhos) / sizeof(collision_empresa_gatilhos[0]);
