#include "screen_room.h"
#include "game_config.h"

#include <stdlib.h>

/* Sequencia de colunas do sprite walk: 3 frames + idle de volta. */
static const int8_t WALK_SEQ[] = { 0, 1, 2, 1 };
#define WALK_SEQ_LEN    (sizeof(WALK_SEQ) / sizeof(WALK_SEQ[0]))

/* === Geometria === */

bool rects_overlap(int ax, int ay, int aw, int ah,
                   int bx, int by, int bw, int bh)
{
    return (ax < bx + bw) && (ax + aw > bx) &&
           (ay < by + bh) && (ay + ah > by);
}

/* === Movimento === */

int room_speed_from_mag(int mag)
{
    if (mag <= PLAYER_JOY_DEADZONE) return 0;
    int s = PLAYER_STEP_MIN_PX + (mag - PLAYER_JOY_DEADZONE) * (PLAYER_STEP_MAX_PX - PLAYER_STEP_MIN_PX) / (100 - PLAYER_JOY_DEADZONE);
    if (s < PLAYER_STEP_MIN_PX) s = PLAYER_STEP_MIN_PX;
    if (s > PLAYER_STEP_MAX_PX) s = PLAYER_STEP_MAX_PX;
    return s;
}

/* === Colisao === */

bool room_collides_at(const room_collision_t *rc,
                       const room_player_box_t *box,
                       int px, int py)
{
    const int cx = px + box->off_x;
    const int cy = py + box->off_y;
    for (size_t i = 0; i < rc->obstaculos_count; ++i) {
        const collision_rect_t *r = &rc->obstaculos[i];
        if (rects_overlap(cx, cy, box->w, box->h, r->x, r->y, r->w, r->h)) {
            return true;
        }
    }
    /* Mantem player dentro da tela. */
    if (cx < 0 || cy < 0 ||
        cx + box->w > rc->screen_w ||
        cy + box->h > rc->screen_h) {
        return true;
    }
    return false;
}

const collision_rect_t *room_gatilho_at(const room_collision_t *rc,
                                          const room_player_box_t *box,
                                          int px, int py)
{
    const int cx = px + box->off_x;
    const int cy = py + box->off_y;
    for (size_t i = 0; i < rc->gatilhos_count; ++i) {
        const collision_rect_t *r = &rc->gatilhos[i];
        if (rects_overlap(cx, cy, box->w, box->h, r->x, r->y, r->w, r->h)) {
            return r;
        }
    }
    return NULL;
}

const collision_rect_t *room_find_gatilho(const room_collision_t *rc,
                                            collision_kind_t kind)
{
    for (size_t i = 0; i < rc->gatilhos_count; ++i) {
        if (rc->gatilhos[i].kind == kind) {
            return &rc->gatilhos[i];
        }
    }
    return NULL;
}

bool room_is_porta(collision_kind_t k)
{
    return k == AREA_PORTA_EMPRESA || k == AREA_PORTA_RECEPCAO;
}

/* === Animacao === */

void room_anim_update_dir(room_player_anim_t *a, int jx, int jy)
{
    /* Linhas do sheet: 0=DOWN, 1=LEFT, 2=RIGHT, 3=UP. Eixo dominante decide. */
    if (abs(jx) > abs(jy)) {
        if (jx > 0)      a->dir = 2; /* RIGHT */
        else if (jx < 0) a->dir = 1; /* LEFT  */
    } else if (jy != 0) {
        a->dir = (jy > 0) ? 0 /* DOWN */ : 3 /* UP */;
    }
}

void room_anim_step(room_player_anim_t *a, lv_obj_t *player_img,
                    int dx, int dy, uint32_t dt_ms,
                    int frame_w, int frame_h)
{
    if (dx != 0 || dy != 0) {
        a->walk_ms += dt_ms;
        if (a->walk_ms >= PLAYER_WALK_PERIOD_MS) {
            a->walk_ms -= PLAYER_WALK_PERIOD_MS;   /* carrega sobra p/ proximo ciclo */
            a->walk_idx = (a->walk_idx + 1) % WALK_SEQ_LEN;
        }
    } else {
        a->walk_idx = 1;     /* idle */
        a->walk_ms  = 0;
    }

    /* Aplica frame: coluna do sheet pelo walk_idx, linha pelo dir. */
    const int8_t col = WALK_SEQ[a->walk_idx];
    lv_image_set_offset_x(player_img, -col * frame_w);
    lv_image_set_offset_y(player_img, -a->dir * frame_h);
}
