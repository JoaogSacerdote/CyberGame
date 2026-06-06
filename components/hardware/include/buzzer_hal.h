#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Buzzer piezo passivo no GPIO BOARD_PIN_BUZZER, acionado por LEDC PWM.
 * Volume e binario (mudo ou no maximo) por decisao de game design
 * 2026-05-12: piezo passivo gera tom pela frequencia, nao pela amplitude;
 * a duty fica fixa em 50%% quando soando. Apenas SFX, sem musica de fundo. */

/* Inicializa LEDC (timer + canal dedicados, separados do backlight do
 * display) e o esp_timer do beep. Idempotente. Comeca silencioso. */
esp_err_t buzzer_hal_init(void);

/* Liga um tom continuo na frequencia dada (duty 50%%). Substitui o tom
 * anterior. freq_hz == 0 ou mudo -> silencia. */
esp_err_t buzzer_hal_tone(uint32_t freq_hz);

/* Silencia imediatamente (duty 0). */
esp_err_t buzzer_hal_stop(void);

/* Beep nao-bloqueante: liga o tom e agenda o stop apos duration_ms via
 * esp_timer one-shot. Retorna na hora; nao bloqueia o chamador. No-op se
 * mudo. Chamadas em sequencia reiniciam a contagem. */
esp_err_t buzzer_hal_beep(uint32_t freq_hz, uint32_t duration_ms);

/* Mute global. Quando true, tone/beep viram no-op e o buzzer e silenciado
 * na hora. */
void buzzer_hal_set_muted(bool muted);
bool buzzer_hal_is_muted(void);

#ifdef __cplusplus
}
#endif
