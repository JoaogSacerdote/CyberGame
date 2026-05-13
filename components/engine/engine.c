#include "engine.h"
#include "game_config.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_check.h"
#include "esp_log.h"

#include "fsm.h"
#include "ui.h"
#include "button_hal.h"
#include "joystick_hal.h"

static const char *TAG = "ENGINE";

#define ENGINE_QUEUE_DEPTH    32
#define ENGINE_TASK_STACK     4096
#define ENGINE_TASK_PRIO      6
#define ENGINE_TASK_CORE      0           /* Core 0; LVGL fica em Core 1 */

static QueueHandle_t s_event_queue   = NULL;
static TaskHandle_t  s_task          = NULL;
static bool          s_initialized   = false;

static void button_reader_task(void *pv)
{
    (void)pv;
    button_event_t bev;
    while (1) {
        if (button_hal_get_event(&bev, UINT32_MAX)) {
            const fsm_event_t fev = {
                .kind = FSM_EVT_BUTTON,
                .payload.button.id    = (uint8_t)bev.id,
                .payload.button.state = (uint8_t)bev.state,
            };
            xQueueSend(s_event_queue, &fev, 0);
        }
    }
}

static void engine_task(void *pv)
{
    (void)pv;
    TickType_t last_tick = xTaskGetTickCount();
    fsm_event_t evt;

    while (1) {
        /* Consome eventos com timeout pequeno; se nao houver, emite tick. */
        const TickType_t wait_ticks = pdMS_TO_TICKS(ENGINE_TICK_PERIOD_MS);
        if (xQueueReceive(s_event_queue, &evt, wait_ticks)) {
            fsm_handle_event(&evt);
        }

        /* Emite FSM_EVT_TICK a cada ENGINE_TICK_PERIOD_MS aproximadamente. */
        const TickType_t now = xTaskGetTickCount();
        const uint32_t dt_ms = pdTICKS_TO_MS(now - last_tick);
        if (dt_ms >= ENGINE_TICK_PERIOD_MS) {
            const fsm_event_t tick_evt = {
                .kind = FSM_EVT_TICK,
                .payload.tick.dt_ms = dt_ms,
            };
            fsm_handle_event(&tick_evt);
            last_tick = now;
        }
    }
}

esp_err_t engine_init(void)
{
    if (s_initialized) {
        return ESP_OK;
    }

    s_event_queue = xQueueCreate(ENGINE_QUEUE_DEPTH, sizeof(fsm_event_t));
    ESP_RETURN_ON_FALSE(s_event_queue != NULL, ESP_ERR_NO_MEM, TAG, "queue alloc failed");

    ESP_RETURN_ON_ERROR(fsm_init(), TAG, "fsm_init failed");

    const esp_err_t ui_err = ui_init();
    if (ui_err != ESP_OK) {
        ESP_LOGE(TAG, "ui_init falhou (%s) — abortando engine_init", esp_err_to_name(ui_err));
        return ui_err;
    }
    ui_show_splash();

    s_initialized = true;
    ESP_LOGI(TAG, "engine_init OK");
    return ESP_OK;
}

esp_err_t engine_start(void)
{
    ESP_RETURN_ON_FALSE(s_initialized, ESP_ERR_INVALID_STATE, TAG, "chame engine_init antes");
    if (s_task != NULL) {
        return ESP_OK;
    }

    const BaseType_t rd = xTaskCreate(button_reader_task, "btn_reader",
                                       2560, NULL, 5, NULL);
    ESP_RETURN_ON_FALSE(rd == pdPASS, ESP_ERR_NO_MEM, TAG, "btn_reader spawn failed");

    const BaseType_t ok = xTaskCreatePinnedToCore(engine_task, "engine",
                                                   ENGINE_TASK_STACK, NULL,
                                                   ENGINE_TASK_PRIO, &s_task,
                                                   ENGINE_TASK_CORE);
    ESP_RETURN_ON_FALSE(ok == pdPASS, ESP_ERR_NO_MEM, TAG, "engine task spawn failed");

    ESP_LOGI(TAG, "engine_start OK (core=%d, prio=%d)", ENGINE_TASK_CORE, ENGINE_TASK_PRIO);
    return ESP_OK;
}
