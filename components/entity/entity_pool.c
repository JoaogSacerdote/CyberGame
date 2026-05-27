#include "entity_pool.h"

#include <assert.h>
#include <string.h>

#include "esp_log.h"
#include "lvgl.h"

static const char *TAG = "ENTITY_POOL";

static entity_t s_pool[ENTITY_POOL_CAPACITY];
static bool     s_used[ENTITY_POOL_CAPACITY];
static size_t   s_count;
static uint32_t s_next_id;

esp_err_t entity_pool_init(void)
{
    memset(s_pool, 0, sizeof(s_pool));
    memset(s_used, 0, sizeof(s_used));
    s_count   = 0;
    s_next_id = 1;       /* 0 reservado para player */
    ESP_LOGI(TAG, "init (capacity=%d)", ENTITY_POOL_CAPACITY);
    return ESP_OK;
}

void entity_pool_clear(void)
{
    for (size_t i = 0; i < ENTITY_POOL_CAPACITY; ++i) {
        if (!s_used[i]) continue;
        if (s_pool[i].lv_obj != NULL) {
            lv_obj_delete(s_pool[i].lv_obj);
            s_pool[i].lv_obj = NULL;
        }
        s_used[i] = false;
    }
    s_count   = 0;
    s_next_id = 1;
}

entity_t *entity_pool_alloc(void)
{
    for (size_t i = 0; i < ENTITY_POOL_CAPACITY; ++i) {
        if (s_used[i]) continue;
        s_used[i] = true;
        memset(&s_pool[i], 0, sizeof(entity_t));
        s_pool[i].id = s_next_id++;
        ++s_count;
        return &s_pool[i];
    }
    ESP_LOGE(TAG, "pool cheio (capacity=%d)", ENTITY_POOL_CAPACITY);
    return NULL;
}

void entity_pool_free(entity_t *e)
{
    assert(e != NULL);
    const size_t idx = (size_t)(e - s_pool);
    assert(idx < ENTITY_POOL_CAPACITY);
    assert(s_used[idx]);

    if (e->lv_obj != NULL) {
        lv_obj_delete(e->lv_obj);
        e->lv_obj = NULL;
    }
    s_used[idx] = false;
    --s_count;
}

size_t entity_pool_count(void)
{
    return s_count;
}

entity_t *entity_pool_at(size_t i)
{
    /* Iteracao linear sobre vivos. i = indice nos VIVOS, nao no array
     * fisico. Custo O(N) por chamada, mas N <= 64. */
    size_t seen = 0;
    for (size_t k = 0; k < ENTITY_POOL_CAPACITY; ++k) {
        if (!s_used[k]) continue;
        if (seen == i) return &s_pool[k];
        ++seen;
    }
    return NULL;
}
