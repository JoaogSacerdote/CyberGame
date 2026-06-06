#ifndef BUTTON_HAL_H
#define BUTTON_HAL_H

#include <stdint.h>
#include <stdbool.h>


typedef enum {
    BTN_A = 0,
    BTN_B,
    BTN_X,
    BTN_Y,
    BTN_MAX_COUNT 
} button_id_t;


typedef enum {
    BTN_RELEASED = 0, // Pull-up atuando (Nível Alto)
    BTN_PRESSED  = 1  // Contato com GND (Nível Baixo)
} button_state_t;

typedef struct {
    button_id_t id;
    button_state_t state;
} button_event_t;

/**
 * @brief Inicializa o hardware dos botões, configura ISRs e a Fila RTOS.
 * @return true se inicializado com sucesso, false caso contrário.
 */
bool button_hal_init(void);

/**
 * @brief Consome eventos da fila de botões. Bloqueante até o timeout.
 * @param[out] event Estrutura onde o evento será copiado.
 * @param[in] timeout_ms Tempo máximo de espera em milissegundos (Use portMAX_DELAY para infinito).
 * @return true se um evento foi capturado, false em caso de timeout.
 */
bool button_get_event(button_event_t *event, uint32_t timeout_ms);

#endif // BUTTON_HAL_H