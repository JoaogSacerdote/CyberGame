#include "button_hal.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/timers.h" 

static const char *TAG = "BUTTON_HAL";


#define GPIO_BTN_A GPIO_NUM_11
#define GPIO_BTN_B GPIO_NUM_12
#define GPIO_BTN_X GPIO_NUM_13
#define GPIO_BTN_Y GPIO_NUM_14

static const gpio_num_t button_gpios[BTN_MAX_COUNT] = {
    GPIO_BTN_A, GPIO_BTN_B, GPIO_BTN_X, GPIO_BTN_Y
};

// Configurações de Fila e Debounce
#define EVENT_QUEUE_SIZE 20
#define DEBOUNCE_TIME_MS 50 

static QueueHandle_t button_queue = NULL;
static TimerHandle_t debounce_timers[BTN_MAX_COUNT] = {NULL};

static button_state_t last_stable_state[BTN_MAX_COUNT] = {BTN_RELEASED, BTN_RELEASED, BTN_RELEASED, BTN_RELEASED};

/**
 * @brief Callback do Timer de Software do FreeRTOS.
 * Avalia a estabilidade do sinal após 50ms da última interrupção.
 */
static void debounce_timer_callback(TimerHandle_t xTimer) {
    uint32_t btn_idx = (uint32_t) pvTimerGetTimerID(xTimer);
    
    if (btn_idx >= BTN_MAX_COUNT) return;

    int level = gpio_get_level(button_gpios[btn_idx]);
    button_state_t current_physical_state = (level == 0) ? BTN_PRESSED : BTN_RELEASED;

    if (current_physical_state != last_stable_state[btn_idx]) {
        last_stable_state[btn_idx] = current_physical_state;

        button_event_t event;
        event.id = (button_id_t)btn_idx;
        event.state = current_physical_state;

        xQueueSend(button_queue, &event, 0);
    }
}

/**
 * @brief Tratador de Interrupção de Hardware.
 */
static void IRAM_ATTR button_isr_handler(void* arg) {
    uint32_t btn_idx = (uint32_t) arg;
    
    if (btn_idx >= BTN_MAX_COUNT) return;

    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    if (debounce_timers[btn_idx] != NULL) {
        xTimerResetFromISR(debounce_timers[btn_idx], &xHigherPriorityTaskWoken);
    }

    if (xHigherPriorityTaskWoken) {
        portYIELD_FROM_ISR();
    }
}

bool button_hal_init(void) {

    if (button_queue != NULL) {
        return true;
    }

    // 1. Cria a Fila de Eventos
    button_queue = xQueueCreate(EVENT_QUEUE_SIZE, sizeof(button_event_t));
    if (button_queue == NULL) {
        ESP_LOGE(TAG, "Falha ao alocar a fila de botões.");
        return false;
    }

    for (uint32_t i = 0; i < BTN_MAX_COUNT; i++) {
        debounce_timers[i] = xTimerCreate("btn_debounce", 
                                          pdMS_TO_TICKS(DEBOUNCE_TIME_MS), 
                                          pdFALSE,
                                          (void *)i, 
                                          debounce_timer_callback);
        if (debounce_timers[i] == NULL) {
            ESP_LOGE(TAG, "Falha ao alocar timer de debounce para o botão %lu", i);
            return false;
        }
    }

    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_ANYEDGE,     
        .mode = GPIO_MODE_INPUT,            
        .pin_bit_mask = (1ULL << GPIO_BTN_A) | (1ULL << GPIO_BTN_B) | 
                        (1ULL << GPIO_BTN_X) | (1ULL << GPIO_BTN_Y),
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .pull_up_en = GPIO_PULLUP_ENABLE    
    };
    
    if (gpio_config(&io_conf) != ESP_OK) {
        ESP_LOGE(TAG, "Falha na configuração do GPIO.");
        return false;
    }

    esp_err_t isr_res = gpio_install_isr_service(ESP_INTR_FLAG_IRAM);
    if (isr_res != ESP_OK && isr_res != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "Falha ao instalar o serviço ISR do GPIO: %s", esp_err_to_name(isr_res));
        return false;
    }

    for (uint32_t i = 0; i < BTN_MAX_COUNT; i++) {
        gpio_isr_handler_add(button_gpios[i], button_isr_handler, (void*) i);
    }

    ESP_LOGI(TAG, "HAL de Botões inicializada (Debounce Timer: %dms).", DEBOUNCE_TIME_MS);
    return true;
}

bool button_get_event(button_event_t *event, uint32_t timeout_ms) {
    if (button_queue == NULL || event == NULL) {
        return false;
    }
    
    TickType_t ticks_to_wait = (timeout_ms == portMAX_DELAY) ? portMAX_DELAY : pdMS_TO_TICKS(timeout_ms);
    return (xQueueReceive(button_queue, event, ticks_to_wait) == pdTRUE);
}