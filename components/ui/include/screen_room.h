#pragma once

/* Helpers compartilhados pelas telas de gameplay (recepcao, empresa, e
 * salas futuras). Concentram a logica de:
 *   - colisao do player com obstaculos e gatilhos da sala;
 *   - velocidade proporcional a deflexao do joystick;
 *   - animacao walk do sprite do player.
 *
 * Antes desta refatoracao, as duas telas tinham ~60 linhas duplicadas de
 * helpers locais — o que ia escalar mal com salas futuras. */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "collision_data.h"
#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Bounding box de colisao do player dentro do sprite frame.
 * O sprite do player e 32x48 (PLAYER_FRAME_W x PLAYER_FRAME_H), mas o "pe" que realmente
 * encosta no chao e menor — 16x12 deslocado em (off_x, off_y) dentro do frame. */
typedef struct {
    int16_t off_x;
    int16_t off_y;
    int16_t w;
    int16_t h;
} room_player_box_t;

/* Dados da sala usados pelos helpers de colisao. As telas preenchem este
 * struct no build, apontando para as tabelas de collision_data.h. */
typedef struct {
    const collision_rect_t *obstaculos;
    size_t                  obstaculos_count;
    const collision_rect_t *gatilhos;
    size_t                  gatilhos_count;
    int16_t                 screen_w;        /* tipicamente 480 */
    int16_t                 screen_h;        /* tipicamente 320 */
} room_collision_t;

/* Estado da animacao walk do player. Resetar no build da tela. */
typedef struct {
    int8_t   dir;          /* 0=DOWN, 1=LEFT, 2=RIGHT, 3=UP (linha do sheet) */
    uint8_t  walk_idx;     /* indice em WALK_SEQ interno */
    uint32_t walk_ms;      /* acumulador desde a ultima troca de frame */
} room_player_anim_t;

/* === Geometria === */

/* AABB overlap (true se os dois retangulos se cruzam). */
bool rects_overlap(int ax, int ay, int aw, int ah,
                   int bx, int by, int bw, int bh);

/* === Movimento === */

/* Velocidade em px/tick em funcao da magnitude |eixo| do joystick (0..100).
 * Curva linear entre PSTEP_MIN e PSTEP_MAX, com deadzone interna. */
int room_speed_from_mag(int mag);

/* === Colisao com a sala === */

/* True se a hitbox do player em (px,py) colide com algum obstaculo da sala
 * OU sai dos limites da tela. */
bool room_collides_at(const room_collision_t *rc,
                       const room_player_box_t *box,
                       int px, int py);

/* Devolve o gatilho sob a hitbox do player, ou NULL se nenhum.
 * Se houver multiplos gatilhos sobrepostos, devolve o primeiro encontrado. */
const collision_rect_t *room_gatilho_at(const room_collision_t *rc,
                                          const room_player_box_t *box,
                                          int px, int py);

/* Busca o primeiro gatilho da sala com este kind (ex.: localizar a porta
 * para calcular spawn). Devolve NULL se nao houver. */
const collision_rect_t *room_find_gatilho(const room_collision_t *rc,
                                            collision_kind_t kind);

/* True se kind e qualquer tipo de porta. */
bool room_is_porta(collision_kind_t k);

/* === Animacao walk === */

/* Avanca o estado da animacao em dt_ms e aplica o frame correto no objeto
 * de imagem do player (lv_image_set_offset_x/y).
 *
 * - dx, dy: direcao do movimento neste tick (-1, 0, +1 em cada eixo).
 *           Se ambos 0, fica em idle (walk_idx=1).
 * - frame_w, frame_h: tamanho de UM frame do sprite-sheet (tipicamente 32x48).
 *
 * O caller mantem o estado dx/dy/dir; este helper so cuida do walk_idx/ms. */
void room_anim_step(room_player_anim_t *a, lv_obj_t *player_img,
                    int dx, int dy, uint32_t dt_ms,
                    int frame_w, int frame_h);

/* Atualiza s_dir do anim baseado em jx, jy (eixo dominante). Helper porque
 * a logica e identica nas duas telas. */
void room_anim_update_dir(room_player_anim_t *a, int jx, int jy);

#ifdef __cplusplus
}
#endif
