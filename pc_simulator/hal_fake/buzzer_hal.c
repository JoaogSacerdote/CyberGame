#include "buzzer_hal.h"

esp_err_t buzzer_hal_init(void)                             { return ESP_OK; }
esp_err_t buzzer_hal_tone(uint32_t freq_hz)                 { (void)freq_hz; return ESP_OK; }
esp_err_t buzzer_hal_stop(void)                             { return ESP_OK; }
esp_err_t buzzer_hal_beep(uint32_t freq_hz, uint32_t ms)    { (void)freq_hz; (void)ms; return ESP_OK; }
void      buzzer_hal_set_muted(bool muted)                  { (void)muted; }
bool      buzzer_hal_is_muted(void)                         { return true; }
