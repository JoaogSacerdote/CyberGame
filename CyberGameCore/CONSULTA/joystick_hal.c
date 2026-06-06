#include "joystick_hal.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "esp_adc/adc_oneshot.h"
#include "driver/gpio.h" 
static const char *TAG = "JOYSTICK_HAL";

#define JOYSTICK_ADC_UNIT           ADC_UNIT_2    // CANAL ADC2
#define JOYSTICK_X_CHANNEL          ADC_CHANNEL_4 // GPIO 15 
#define JOYSTICK_Y_CHANNEL          ADC_CHANNEL_5 // GPIO 16 

#define MA_WINDOW_SIZE              8
#define DEADZONE_PERCENT            5

typedef struct {
    int center_x;
    int center_y;
} joystick_cal_t;

static joystick_cal_t calibration = {2048, 2048};
static joystick_data_t current_state = {0, 0};
static SemaphoreHandle_t state_mutex = NULL;
static adc_oneshot_unit_handle_t adc_handle = NULL;

static int x_buffer[MA_WINDOW_SIZE], y_buffer[MA_WINDOW_SIZE];
static int ma_index = 0, x_sum = 0, y_sum = 0;

/**
 * @brief Mapeia o valor do ADC para -100..100 baseando-se no centro calibrado.
 * Usa lógica de interpolação linear para garantir que o 0 seja sempre o repouso.
 */
static int8_t map_axis(int raw, int center) {
    int32_t val;
    
    if (raw > center) {
        val = ((raw - center) * 100) / (4095 - center);
    } else {
        val = ((raw - center) * 100) / (center - 0);
    }

    if (val > 100)  val = 100;
    if (val < -100) val = -100;

    if (abs(val) <= DEADZONE_PERCENT) return 0;
    
    return (int8_t)val;
}

static void joystick_task(void *pvParameters) {
    int rx = 0, ry = 0;
    while (1) {
        if (adc_oneshot_read(adc_handle, JOYSTICK_X_CHANNEL, &rx) == ESP_OK &&
            adc_oneshot_read(adc_handle, JOYSTICK_Y_CHANNEL, &ry) == ESP_OK) {
            
            x_sum = x_sum - x_buffer[ma_index] + rx;
            y_sum = y_sum - y_buffer[ma_index] + ry;
            x_buffer[ma_index] = rx;
            y_buffer[ma_index] = ry;
            ma_index = (ma_index + 1) % MA_WINDOW_SIZE;

            int8_t final_x = map_axis(x_sum / MA_WINDOW_SIZE, calibration.center_x);
            int8_t final_y = -map_axis(y_sum / MA_WINDOW_SIZE, calibration.center_y);

            if (xSemaphoreTake(state_mutex, pdMS_TO_TICKS(5)) == pdTRUE) {
                current_state.x = final_x;
                current_state.y = final_y;
                xSemaphoreGive(state_mutex);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

void joystick_hal_init(void) {
    state_mutex = xSemaphoreCreateMutex();

    adc_oneshot_unit_init_cfg_t init_config = {.unit_id = JOYSTICK_ADC_UNIT};
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config, &adc_handle));

    adc_oneshot_chan_cfg_t config = {
        .bitwidth = ADC_BITWIDTH_12,
        .atten = ADC_ATTEN_DB_12,
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc_handle, JOYSTICK_X_CHANNEL, &config));
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc_handle, JOYSTICK_Y_CHANNEL, &config));

    
    ESP_LOGI(TAG, "Calibrando... NÃO MEXA NO JOYSTICK!");
    int32_t cx = 0, cy = 0;
    for (int i = 0; i < 50; i++) {
        int tx, ty;
        adc_oneshot_read(adc_handle, JOYSTICK_X_CHANNEL, &tx);
        adc_oneshot_read(adc_handle, JOYSTICK_Y_CHANNEL, &ty);
        cx += tx; cy += ty;
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    calibration.center_x = cx / 50;
    calibration.center_y = cy / 50;
    ESP_LOGI(TAG, "Calibrado! Centro X: %d, Centro Y: %d", calibration.center_x, calibration.center_y);


    for(int i=0; i<MA_WINDOW_SIZE; i++) {
        x_buffer[i] = calibration.center_x;
        y_buffer[i] = calibration.center_y;
    }
    x_sum = calibration.center_x * MA_WINDOW_SIZE;
    y_sum = calibration.center_y * MA_WINDOW_SIZE;

    xTaskCreatePinnedToCore(joystick_task, "joy_task", 4096, NULL, 5, NULL, 1);
}

joystick_data_t joystick_get_state(void) {
    joystick_data_t state = {0, 0};
    if (xSemaphoreTake(state_mutex, pdMS_TO_TICKS(2)) == pdTRUE) {
        state = current_state;
        xSemaphoreGive(state_mutex);
    }
    return state;
}