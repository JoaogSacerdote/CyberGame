#include "melody_player.h"

#include "buzzer_hal.h"
#include "buzzer_melodies.h"

#include "esp_timer.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

static const char *TAG = "MELODY";

/* Tabela de melodias indexada por melody_id_t */
static const struct {
    const uint16_t (*notes)[2];
    uint16_t        len;
} MELODIES[] = {
    [MELODY_RECEPCAO]   = { melody_recepcao,   MELODY_RECEPCAO_LEN   },
    [MELODY_ESCRITORIO] = { melody_escritorio, MELODY_ESCRITORIO_LEN },
    [MELODY_ATAQUE]     = { melody_ataque,     MELODY_ATAQUE_LEN     },
};
#define MELODY_COUNT  3u

/* Estado compartilhado entre engine_task e esp_timer callback */
static esp_timer_handle_t s_timer     = NULL;
static SemaphoreHandle_t  s_mutex     = NULL;

static melody_id_t  s_cur_id  = MELODY_NONE;
static uint16_t     s_cur_idx = 0;
static bool         s_loop    = false;

static void advance_note(void);  /* forward */

static void timer_cb(void *arg)
{
    (void)arg;
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    advance_note();
    xSemaphoreGive(s_mutex);
}

/* Toca a nota em s_cur_idx e agenda o timer para ela. Mutex DEVE estar preso. */
static void advance_note(void)
{
    if (s_cur_id >= MELODY_COUNT) {
        return;
    }
    const uint16_t len = MELODIES[s_cur_id].len;

    if (s_cur_idx >= len) {
        if (s_loop) {
            s_cur_idx = 0;
        } else {
            s_cur_id = MELODY_NONE;
            buzzer_hal_stop();
            return;
        }
    }

    const uint16_t freq = MELODIES[s_cur_id].notes[s_cur_idx][0];
    const uint16_t dur  = MELODIES[s_cur_id].notes[s_cur_idx][1];
    s_cur_idx++;

    /* Frequências abaixo de 100 Hz mal audíveis no piezo — trata como pausa */
    if (freq < 100) {
        buzzer_hal_stop();
    } else {
        buzzer_hal_tone(freq);
    }

    esp_timer_start_once(s_timer, (uint64_t)dur * 1000ULL);
}

/* ── API pública ───────────────────────────────────────────────────────────── */

esp_err_t melody_player_init(void)
{
    if (s_timer != NULL) {
        return ESP_OK;
    }

    s_mutex = xSemaphoreCreateMutex();
    if (!s_mutex) {
        return ESP_ERR_NO_MEM;
    }

    const esp_timer_create_args_t args = {
        .callback = timer_cb,
        .name     = "melody",
    };
    const esp_err_t err = esp_timer_create(&args, &s_timer);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_timer_create falhou: %s", esp_err_to_name(err));
        return err;
    }

    ESP_LOGI(TAG, "melody_player pronto (%u melodias)", MELODY_COUNT);
    return ESP_OK;
}

void melody_player_play(melody_id_t id, bool loop)
{
    if (!s_timer || id >= MELODY_COUNT) {
        melody_player_stop();
        return;
    }

    xSemaphoreTake(s_mutex, portMAX_DELAY);

    if (s_cur_id == id) {
        /* Mesma melodia já rodando — não reinicia. */
        xSemaphoreGive(s_mutex);
        return;
    }

    esp_timer_stop(s_timer);
    s_cur_id  = id;
    s_cur_idx = 0;
    s_loop    = loop;

    advance_note();  /* toca a primeira nota imediatamente */

    xSemaphoreGive(s_mutex);
}

void melody_player_stop(void)
{
    if (!s_timer) return;
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    esp_timer_stop(s_timer);
    s_cur_id  = MELODY_NONE;
    s_cur_idx = 0;
    xSemaphoreGive(s_mutex);
    buzzer_hal_stop();
}

melody_id_t melody_player_current(void)
{
    return s_cur_id;
}
