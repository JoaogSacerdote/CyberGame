#include "button_hal.h"
#include "board_pins.h"
#include "hal_common.h"

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/timers.h"
#include "driver/gpio.h"
#include "esp_check.h"
#include "esp_log.h"

static const char *TAG = "BUTTON_HAL";

#define BTN_QUEUE_DEPTH     16
#define BTN_DEBOUNCE_MS     50

/* Ordem aqui DEVE bater com a ordem do button_id_t em button_hal.h
 * (BTN_A, BTN_B, BTN_X, BTN_Y, BTN_START). */
static const gpio_num_t s_gpio[BTN_MAX_COUNT] = {
    BOARD_PIN_BTN_A, BOARD_PIN_BTN_B, BOARD_PIN_BTN_X,
    BOARD_PIN_BTN_Y, BOARD_PIN_BTN_START,
};

static QueueHandle_t  s_queue = NULL;
static TimerHandle_t  s_debounce_timer[BTN_MAX_COUNT] = { NULL };
static button_state_t s_last_stable[BTN_MAX_COUNT]   = { BTN_RELEASED };

static void debounce_timer_cb(TimerHandle_t timer)
{
    const uint32_t btn_idx = (uint32_t)(uintptr_t)pvTimerGetTimerID(timer);
    if (btn_idx >= BTN_MAX_COUNT) return;

    const button_state_t now =
        (gpio_get_level(s_gpio[btn_idx]) == 0) ? BTN_PRESSED : BTN_RELEASED;

    if (now != s_last_stable[btn_idx]) {
        s_last_stable[btn_idx] = now;
        const button_event_t ev = { .id = (button_id_t)btn_idx, .state = now };
        xQueueSend(s_queue, &ev, 0);
    }
}

static void IRAM_ATTR button_isr(void *arg)
{
    const uint32_t btn_idx = (uint32_t)(uintptr_t)arg;
    if (btn_idx >= BTN_MAX_COUNT) return;

    BaseType_t hp_woken = pdFALSE;
    if (s_debounce_timer[btn_idx] != NULL) {
        xTimerResetFromISR(s_debounce_timer[btn_idx], &hp_woken);
    }
    if (hp_woken == pdTRUE) {
        portYIELD_FROM_ISR();
    }
}

esp_err_t button_hal_init(void)
{
    if (s_queue != NULL) {
        return ESP_OK;
    }

    s_queue = xQueueCreate(BTN_QUEUE_DEPTH, sizeof(button_event_t));
    ESP_RETURN_ON_FALSE(s_queue != NULL, ESP_ERR_NO_MEM, TAG, "queue alloc failed");

    for (uint32_t i = 0; i < BTN_MAX_COUNT; ++i) {
        s_debounce_timer[i] = xTimerCreate("btn_debounce",
                                           pdMS_TO_TICKS(BTN_DEBOUNCE_MS),
                                           pdFALSE,
                                           (void *)(uintptr_t)i,
                                           debounce_timer_cb);
        ESP_RETURN_ON_FALSE(s_debounce_timer[i] != NULL, ESP_ERR_NO_MEM, TAG,
                            "debounce timer %lu alloc failed", i);
    }

    const gpio_config_t cfg = {
        .pin_bit_mask = (1ULL << BOARD_PIN_BTN_A) | (1ULL << BOARD_PIN_BTN_B) |
                        (1ULL << BOARD_PIN_BTN_X) | (1ULL << BOARD_PIN_BTN_Y) |
                        (1ULL << BOARD_PIN_BTN_START),
        .mode         = GPIO_MODE_INPUT,
        .pull_up_en   = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_ANYEDGE,
    };
    ESP_RETURN_ON_ERROR(gpio_config(&cfg), TAG, "gpio_config failed");

    /* BTN_START compartilha GPIO com PMU_REC (ver _Static_assert em board_pins.h).
     * Se o usuario ainda esta segurando o botao apos o boot, armar ISR agora
     * dispararia evento espurio imediato. */
    vTaskDelay(pdMS_TO_TICKS(100));
    while (gpio_get_level(BOARD_PIN_BTN_START) == 0) {
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    ESP_RETURN_ON_ERROR(hal_isr_service_install_once(), TAG, "isr service install failed");

    for (uint32_t i = 0; i < BTN_MAX_COUNT; ++i) {
        ESP_RETURN_ON_ERROR(
            gpio_isr_handler_add(s_gpio[i], button_isr, (void *)(uintptr_t)i),
            TAG, "isr handler add for btn %lu failed", i);
    }

    ESP_LOGI(TAG, "button_hal initialized (debounce %d ms, queue %d, pinos A=%d B=%d X=%d Y=%d START=%d)",
             BTN_DEBOUNCE_MS, BTN_QUEUE_DEPTH,
             BOARD_PIN_BTN_A, BOARD_PIN_BTN_B, BOARD_PIN_BTN_X,
             BOARD_PIN_BTN_Y, BOARD_PIN_BTN_START);
    return ESP_OK;
}

bool button_hal_get_event(button_event_t *event, uint32_t timeout_ms)
{
    if (s_queue == NULL || event == NULL) {
        return false;
    }
    const TickType_t ticks = (timeout_ms == UINT32_MAX)
                                 ? portMAX_DELAY
                                 : pdMS_TO_TICKS(timeout_ms);
    return xQueueReceive(s_queue, event, ticks) == pdTRUE;
}

button_state_t button_hal_peek(button_id_t id)
{
    if (id >= BTN_MAX_COUNT) {
        return BTN_RELEASED;
    }
    return s_last_stable[id];
}
