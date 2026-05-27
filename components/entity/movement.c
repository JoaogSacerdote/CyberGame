#include "movement.h"

#include <assert.h>

#include "entity_pool.h"
#include "y_sort.h"

static inline bool aabb_intersect(int16_t ax, int16_t ay, int16_t aw, int16_t ah,
                                  int16_t bx, int16_t by, int16_t bw, int16_t bh)
{
    return !(ax + aw <= bx || bx + bw <= ax ||
             ay + ah <= by || by + bh <= ay);
}

bool entity_collides_solid(const entity_t *self)
{
    assert(self != NULL);

    int16_t ax, ay, aw, ah;
    entity_get_collision_rect(self, &ax, &ay, &aw, &ah);

    const size_t n = entity_pool_count();
    for (size_t i = 0; i < n; ++i) {
        entity_t *other = entity_pool_at(i);
        if (other == NULL || other == self) continue;
        if (!(other->flags & ENTITY_FLAG_SOLID)) continue;

        int16_t bx, by, bw, bh;
        entity_get_collision_rect(other, &bx, &by, &bw, &bh);
        if (aabb_intersect(ax, ay, aw, ah, bx, by, bw, bh)) {
            return true;
        }
    }
    return false;
}

bool entity_try_move(entity_t *e, int16_t dx, int16_t dy)
{
    assert(e != NULL);
    bool moved = false;

    if (dx != 0) {
        e->x += dx;
        if (entity_collides_solid(e)) {
            e->x -= dx;
        } else {
            moved = true;
        }
    }

    if (dy != 0) {
        e->y += dy;
        if (entity_collides_solid(e)) {
            e->y -= dy;
        } else {
            moved = true;
        }
    }

    if (moved) {
        entity_apply_lv_pos(e);
        y_sort_mark_dirty();
    }
    return moved;
}

void entity_check_triggers(entity_t *mover,
                           entity_trigger_cb_t cb,
                           void *user_ctx)
{
    assert(mover != NULL);
    assert(cb != NULL);

    int16_t mx, my, mw, mh;
    entity_get_collision_rect(mover, &mx, &my, &mw, &mh);

    const size_t n = entity_pool_count();
    for (size_t i = 0; i < n; ++i) {
        entity_t *other = entity_pool_at(i);
        if (other == NULL || other == mover) continue;
        if (!(other->flags & ENTITY_FLAG_TRIGGER)) continue;

        int16_t bx, by, bw, bh;
        entity_get_collision_rect(other, &bx, &by, &bw, &bh);
        if (aabb_intersect(mx, my, mw, mh, bx, by, bw, bh)) {
            cb(other, user_ctx);
        }
    }
}
