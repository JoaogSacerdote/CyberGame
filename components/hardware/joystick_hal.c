#include "joystick_hal.h"
#include "board_pins.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_check.h"
#include "esp_log.h"

static const char *TAG = "JOYSTICK_HAL";

/* No ESP32-S3, ADC1_CH0 = GPIO1 e ADC1_CH1 = GPIO2.
 * O driver ADC oneshot opera em termos de canal (nao de GPIO), mas os
 * BOARD_PIN_JOY_*_ADC em board_pins.h apontam exatamente para esses pinos.
 * Os asserts abaixo travam o build se alguem mudar o pinout sem mudar o canal. */
#define JOY_ADC_UNIT            ADC_UNIT_1
#define JOY_X_CHANNEL           ADC_CHANNEL_0
#define JOY_Y_CHANNEL           ADC_CHANNEL_1

_Static_assert(BOARD_PIN_JOY_X_ADC == GPIO_NUM_1,
               "BOARD_PIN_JOY_X_ADC nao bate com ADC1_CH0 (GPIO1) no ESP32-S3");
_Static_assert(BOARD_PIN_JOY_Y_ADC == GPIO_NUM_2,
               "BOARD_PIN_JOY_Y_ADC nao bate com ADC1_CH1 (GPIO2) no ESP32-S3");

#define JOY_ADC_BITWIDTH        ADC_BITWIDTH_12
#define JOY_ADC_ATTEN           ADC_ATTEN_DB_12     /* full-scale 0–3.3V */
#define JOY_ADC_RAW_MAX         4095

#define JOY_SAMPLE_PERIOD_MS    20      /* 50 Hz */
#define JOY_MA_WINDOW           8
#define JOY_DEADZONE_PERCENT    5

#define JOY_CAL_SAMPLES         50
#define JOY_CAL_INTERVAL_MS     10
#define JOY_CAL_CENTER_DEFAULT  2048
#define JOY_CAL_CENTER_MIN      1500
#define JOY_CAL_CENTER_MAX      2500

#define JOY_TASK_STACK          3072
#define JOY_TASK_PRIO           5
#define JOY_TASK_CORE           1       /* deixa core 0 livre para game logic */

static adc_oneshot_unit_handle_t s_adc        = NULL;
static SemaphoreHandle_t         s_mutex      = NULL;
static joystick_data_t           s_state      = { 0, 0 };

static int s_center_x = JOY_CAL_CENTER_DEFAULT;
static int s_center_y = JOY_CAL_CENTER_DEFAULT;

static int s_x_buf[JOY_MA_WINDOW];
static int s_y_buf[JOY_MA_WINDOW];

static int8_t map_axis(int raw, int center)
{
    int32_t v;
    if (raw > center) {
        v = ((int32_t)(raw - center) * 100) / (JOY_ADC_RAW_MAX - center);
    } else {
        v = ((int32_t)(raw - center) * 100) / center;
    }
    if (v >  100) v =  100;
    if (v < -100) v = -100;
    if (v >= -JOY_DEADZONE_PERCENT && v <= JOY_DEADZONE_PERCENT) {
        return 0;
    }
    return (int8_t)v;
}

static esp_err_t joystick_calibrate_center(void)
{
    ESP_LOGI(TAG, "Calibrando centro (mantenha o joystick parado por %d ms)...",
             JOY_CAL_SAMPLES * JOY_CAL_INTERVAL_MS);

    int32_t sx = 0, sy = 0;
    int valid = 0;

    for (int i = 0; i < JOY_CAL_SAMPLES; ++i) {
        int rx = 0, ry = 0;
        if (adc_oneshot_read(s_adc, JOY_X_CHANNEL, &rx) == ESP_OK &&
            adc_oneshot_read(s_adc, JOY_Y_CHANNEL, &ry) == ESP_OK) {
            sx += rx;
            sy += ry;
            ++valid;
        }
        vTaskDelay(pdMS_TO_TICKS(JOY_CAL_INTERVAL_MS));
    }

    if (valid == 0) {
        ESP_LOGE(TAG, "Calibracao falhou: nenhuma amostra valida.");
        return ESP_FAIL;
    }

    const int cx = (int)(sx / valid);
    const int cy = (int)(sy / valid);

    /* Sanidade: se centro lido nao for plausivel, joystick foi mexido durante
     * calibracao ou esta com falha. Cai no default 2048 para evitar drift. */
    if (cx < JOY_CAL_CENTER_MIN || cx > JOY_CAL_CENTER_MAX ||
        cy < JOY_CAL_CENTER_MIN || cy > JOY_CAL_CENTER_MAX) {
        ESP_LOGW(TAG, "Centro fora da faixa esperada (X=%d Y=%d). Usando default %d.",
                 cx, cy, JOY_CAL_CENTER_DEFAULT);
        s_center_x = JOY_CAL_CENTER_DEFAULT;
        s_center_y = JOY_CAL_CENTER_DEFAULT;
    } else {
        s_center_x = cx;
        s_center_y = cy;
        ESP_LOGI(TAG, "Calibrado: centro X=%d, Y=%d", s_center_x, s_center_y);
    }
    return ESP_OK;
}

static void joystick_task(void *pv)
{
    (void)pv;

    for (int i = 0; i < JOY_MA_WINDOW; ++i) {
        s_x_buf[i] = s_center_x;
        s_y_buf[i] = s_center_y;
    }
    int x_sum = s_center_x * JOY_MA_WINDOW;
    int y_sum = s_center_y * JOY_MA_WINDOW;
    int idx = 0;

    while (1) {
        int rx = 0, ry = 0;
        if (adc_oneshot_read(s_adc, JOY_X_CHANNEL, &rx) == ESP_OK &&
            adc_oneshot_read(s_adc, JOY_Y_CHANNEL, &ry) == ESP_OK) {

            x_sum += rx - s_x_buf[idx];
            y_sum += ry - s_y_buf[idx];
            s_x_buf[idx] = rx;
            s_y_buf[idx] = ry;
            idx = (idx + 1) % JOY_MA_WINDOW;

            const int8_t fx = map_axis(x_sum / JOY_MA_WINDOW, s_center_x);
            const int8_t fy = map_axis(y_sum / JOY_MA_WINDOW, s_center_y);

            if (xSemaphoreTake(s_mutex, pdMS_TO_TICKS(5)) == pdTRUE) {
                s_state.x = fx;
                s_state.y = fy;
                xSemaphoreGive(s_mutex);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(JOY_SAMPLE_PERIOD_MS));
    }
}

esp_err_t joystick_hal_init(void)
{
    if (s_mutex != NULL) {
        return ESP_OK;
    }

    s_mutex = xSemaphoreCreateMutex();
    ESP_RETURN_ON_FALSE(s_mutex != NULL, ESP_ERR_NO_MEM, TAG, "mutex alloc failed");

    const adc_oneshot_unit_init_cfg_t unit_cfg = { .unit_id = JOY_ADC_UNIT };
    ESP_RETURN_ON_ERROR(adc_oneshot_new_unit(&unit_cfg, &s_adc),
                        TAG, "adc_oneshot_new_unit failed");

    const adc_oneshot_chan_cfg_t chan_cfg = {
        .bitwidth = JOY_ADC_BITWIDTH,
        .atten    = JOY_ADC_ATTEN,
    };
    ESP_RETURN_ON_ERROR(adc_oneshot_config_channel(s_adc, JOY_X_CHANNEL, &chan_cfg),
                        TAG, "config X channel failed");
    ESP_RETURN_ON_ERROR(adc_oneshot_config_channel(s_adc, JOY_Y_CHANNEL, &chan_cfg),
                        TAG, "config Y channel failed");

    ESP_RETURN_ON_ERROR(joystick_calibrate_center(), TAG, "calibrate failed");

    const BaseType_t ok = xTaskCreatePinnedToCore(joystick_task, "joy_hal",
                                                  JOY_TASK_STACK, NULL,
                                                  JOY_TASK_PRIO, NULL,
                                                  JOY_TASK_CORE);
    ESP_RETURN_ON_FALSE(ok == pdPASS, ESP_ERR_NO_MEM, TAG, "task create failed");

    ESP_LOGI(TAG, "joystick_hal initialized (X=GPIO1 Y=GPIO2 ADC1, %d Hz, MA=%d, deadzone=%d%%)",
             1000 / JOY_SAMPLE_PERIOD_MS, JOY_MA_WINDOW, JOY_DEADZONE_PERCENT);
    return ESP_OK;
}

joystick_data_t joystick_hal_get_state(void)
{
    joystick_data_t snap = { 0, 0 };
    if (s_mutex == NULL) {
        return snap;
    }
    if (xSemaphoreTake(s_mutex, pdMS_TO_TICKS(2)) == pdTRUE) {
        snap = s_state;
        xSemaphoreGive(s_mutex);
    }
    return snap;
}
