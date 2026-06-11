#pragma once
/* Gerado por tools/extract_collisions.py — NAO EDITE A MAO */
#include <stdint.h>
#include <stddef.h>

typedef enum {
    OBSTACULO = 0,
    AREA_INTERACAO_NPC,
    AREA_INTERACAO_NPC_TI_DIREITA,
    AREA_INTERACAO_NPC_TI_BAIXO,
    AREA_PORTA_EMPRESA,
    AREA_PORTA_RECEPCAO,
    AREA_TAREFA_VERDE,
    AREA_TAREFA_AMARELA,
    AREA_SERVIDOR,    /* servidor A (esquerda) */
    AREA_SERVIDOR_B,  /* servidor B (direita)  */
    AREA_SPAWN,
    AREA_GENERICA,
} collision_kind_t;

typedef struct {
    int16_t x;
    int16_t y;
    int16_t w;
    int16_t h;
    collision_kind_t kind;
} collision_rect_t;

/* Por sala */
extern const collision_rect_t collision_recepcao_obstaculos[];
extern const size_t           collision_recepcao_obstaculos_count;
extern const collision_rect_t collision_recepcao_gatilhos[];
extern const size_t           collision_recepcao_gatilhos_count;

extern const collision_rect_t collision_empresa_obstaculos[];
extern const size_t           collision_empresa_obstaculos_count;
extern const collision_rect_t collision_empresa_gatilhos[];
extern const size_t           collision_empresa_gatilhos_count;