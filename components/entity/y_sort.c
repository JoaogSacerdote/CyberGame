#include "y_sort.h"

#include "entity_pool.h"
#include "lvgl.h"

/* Buffer estatico de ponteiros — sem heap, dimensionado pelo teto do
 * pool. Pior caso: todas YSORTED. */
static entity_t *s_sorted[ENTITY_POOL_CAPACITY];
static bool      s_dirty;

void y_sort_init(void)
{
    s_dirty = true;
}

void y_sort_mark_dirty(void)
{
    s_dirty = true;
}

bool y_sort_is_dirty(void)
{
    return s_dirty;
}

void y_sort_run(void)
{
    if (!s_dirty) return;

    /* 1. Popular s_sorted com entities YSORTED VIVAS. Ordem inicial:
     *    a ordem de iteracao do pool. Insertion sort vai consertar. */
    size_t n = 0;
    const size_t pool_n = entity_pool_count();
    for (size_t i = 0; i < pool_n && n < ENTITY_POOL_CAPACITY; ++i) {
        entity_t *e = entity_pool_at(i);
        if (e == NULL) continue;
        if (!(e->flags & ENTITY_FLAG_YSORTED)) continue;
        s_sorted[n++] = e;
    }

    /* 2. Insertion sort por sort_y. Lista quase-ordenada (entities movem
     *    poucos pixels por frame) -> while loop quase nunca executa.
     *    Esperado O(N) no caso comum, O(N^2) no pior caso. Para N<=64
     *    aceitavel. NAO usar qsort() — overhead de function pointer eh
     *    pior para N pequeno + quase ordenado. */
    for (size_t i = 1; i < n; ++i) {
        entity_t *cur     = s_sorted[i];
        const int16_t key = entity_sort_y(cur);
        size_t j = i;
        while (j > 0 && entity_sort_y(s_sorted[j - 1]) > key) {
            s_sorted[j] = s_sorted[j - 1];
            --j;
        }
        s_sorted[j] = cur;
    }

    /* 3. Aplica ordem na arvore LVGL. lv_obj_move_foreground coloca o
     *    objeto como ultimo filho do pai — entao chamamos em ordem
     *    crescente de sort_y, deixando o de maior sort_y mais visivel. */
    for (size_t i = 0; i < n; ++i) {
        if (s_sorted[i]->lv_obj != NULL) {
            lv_obj_move_foreground(s_sorted[i]->lv_obj);
        }
    }

    s_dirty = false;
}
