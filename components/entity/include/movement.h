#pragma once

#include <stdbool.h>
#include "entity.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Tenta mover entity por (dx, dy). Faz **separacao X/Y** para colisao:
 * 1. aplica dx; se colide com solido, desfaz dx
 * 2. aplica dy; se colide com solido, desfaz dy
 *
 * Sem essa separacao, bater diagonal em parede trava o movimento
 * inteiro. Com ela, o player desliza ao longo da parede.
 *
 * Atualiza lv_obj position automaticamente via entity_apply_lv_pos
 * (chamar SOB lv_lock — esta funcao NAO toma o lock sozinha).
 * Marca y_sort dirty se moveu em qualquer eixo.
 *
 * Retorna true se moveu em pelo menos um eixo. */
bool entity_try_move(entity_t *e, int16_t dx, int16_t dy);

/* Testa se a collision_box de self sobrepoe a de qualquer outra entity
 * viva com ENTITY_FLAG_SOLID. Linear no pool — O(N).
 * Self eh excluida do teste (sem self-collision). */
bool entity_collides_solid(const entity_t *self);

/* Callback para entity_check_triggers. Chamada uma vez por trigger
 * sobreposto. */
typedef void (*entity_trigger_cb_t)(entity_t *trigger, void *user_ctx);

/* Itera entities com ENTITY_FLAG_TRIGGER cuja collision_box sobrepoe
 * a de 'mover'. Chama cb para cada uma. Util para "player entrou em
 * zona X". Sem state — caller filtra repeticoes se quiser. */
void entity_check_triggers(entity_t *mover,
                           entity_trigger_cb_t cb,
                           void *user_ctx);

#ifdef __cplusplus
}
#endif
