#include "buzzer_hal.h"

#include "board_pins.h"
#include "driver/ledc.h"
#include "esp_timer.h"
#include "esp_log.h"

static const char *TAG = "BUZZER_HAL";

/* LEDC dedicado ao buzzer. O backlight do display usa TIMER_0 + CHANNEL_0
 * (ver display_hal.c) — aqui usamos TIMER_1 + CHANNEL_1 de proposito: o tom
 * varia a frequencia do timer, o que NAO pode afetar o brilho da tela. */
#define BUZZER_LEDC_MODE       LEDC_LOW_SPEED_MODE
#define BUZZER_LEDC_TIMER      LEDC_TIMER_1
#define BUZZER_LEDC_CHANNEL    LEDC_CHANNEL_1
#define BUZZER_LEDC_RES        LEDC_TIMER_10_BIT
#define BUZZER_DUTY_ON         512u    /* 50%% de (2^10 - 1) */
#define BUZZER_DEFAULT_FREQ    2000u   /* freq inicial do timer; tone() troca */

static bool              s_inited     = false;
static bool              s_muted      = false;
static esp_timer_handle_t s_beep_timer = NULL;

static void beep_stop_cb(void *arg)
{
    (void)arg;
    /* esp_timer chama em contexto de task dedicada — seguro chamar LEDC. */
    ledc_set_duty(BUZZER_LEDC_MODE, BUZZER_LEDC_CHANNEL, 0);
    ledc_update_duty(BUZZER_LEDC_MODE, BUZZER_LEDC_CHANNEL);
}

esp_err_t buzzer_hal_init(void)
{
    if (s_inited) {
        return ESP_OK;   /* idempotente */
    }

    const ledc_timer_config_t timer_cfg = {
        .speed_mode      = BUZZER_LEDC_MODE,
        .timer_num       = BUZZER_LEDC_TIMER,
        .duty_resolution = BUZZER_LEDC_RES,
        .freq_hz         = BUZZER_DEFAULT_FREQ,
        .clk_cfg         = LEDC_AUTO_CLK,
    };
    esp_err_t err = ledc_timer_config(&timer_cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "ledc_timer_config falhou: %s", esp_err_to_name(err));
        return err;
    }

    const ledc_channel_config_t ch_cfg = {
        .gpio_num   = BOARD_PIN_BUZZER,
        .speed_mode = BUZZER_LEDC_MODE,
        .channel    = BUZZER_LEDC_CHANNEL,
        .timer_sel  = BUZZER_LEDC_TIMER,
        .duty       = 0,                 /* comeca silencioso */
        .hpoint     = 0,
        /* intr_type omitido: default zero == LEDC_INTR_DISABLE (campo
         * deprecado em IDF 6.0; nao usamos interrupcao de fade aqui). */
    };
    err = ledc_channel_config(&ch_cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "ledc_channel_config falhou: %s", esp_err_to_name(err));
        return err;
    }

    const esp_timer_create_args_t beep_args = {
        .callback = beep_stop_cb,
        .name     = "buzzer_beep",
    };
    err = esp_timer_create(&beep_args, &s_beep_timer);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_timer_create falhou: %s", esp_err_to_name(err));
        return err;
    }

    s_inited = true;
    ESP_LOGI(TAG, "Buzzer pronto no GPIO %d (LEDC timer %d / canal %d)",
             BOARD_PIN_BUZZER, BUZZER_LEDC_TIMER, BUZZER_LEDC_CHANNEL);
    return ESP_OK;
}

esp_err_t buzzer_hal_tone(uint32_t freq_hz)
{
    if (!s_inited) {
        return ESP_ERR_INVALID_STATE;
    }
    if (s_muted || freq_hz == 0) {
        return buzzer_hal_stop();
    }

    const esp_err_t err = ledc_set_freq(BUZZER_LEDC_MODE, BUZZER_LEDC_TIMER, freq_hz);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "ledc_set_freq(%u) falhou: %s", (unsigned)freq_hz, esp_err_to_name(err));
        return err;
    }
    ledc_set_duty(BUZZER_LEDC_MODE, BUZZER_LEDC_CHANNEL, BUZZER_DUTY_ON);
    return ledc_update_duty(BUZZER_LEDC_MODE, BUZZER_LEDC_CHANNEL);
}

esp_err_t buzzer_hal_stop(void)
{
    if (!s_inited) {
        return ESP_ERR_INVALID_STATE;
    }
    ledc_set_duty(BUZZER_LEDC_MODE, BUZZER_LEDC_CHANNEL, 0);
    return ledc_update_duty(BUZZER_LEDC_MODE, BUZZER_LEDC_CHANNEL);
}

esp_err_t buzzer_hal_beep(uint32_t freq_hz, uint32_t duration_ms)
{
    if (!s_inited) {
        return ESP_ERR_INVALID_STATE;
    }
    if (s_muted) {
        return ESP_OK;
    }

    const esp_err_t err = buzzer_hal_tone(freq_hz);
    if (err != ESP_OK) {
        return err;
    }

    esp_timer_stop(s_beep_timer);   /* reinicia se um beep anterior ainda contava */
    return esp_timer_start_once(s_beep_timer, (uint64_t)duration_ms * 1000ULL);
}

void buzzer_hal_set_muted(bool muted)
{
    s_muted = muted;
    if (muted && s_inited) {
        buzzer_hal_stop();
    }
}

bool buzzer_hal_is_muted(void)
{
    return s_muted;
}
