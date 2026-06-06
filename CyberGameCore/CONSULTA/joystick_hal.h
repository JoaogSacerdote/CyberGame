#ifndef JOYSTICK_HAL_H
#define JOYSTICK_HAL_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Estrutura que guarda o estado processado do joystick */
typedef struct {
    int8_t x; /* Eixo X: -100 (Esquerda) a +100 (Direita), 0 no centro */
    int8_t y; /* Eixo Y: -100 (Baixo) a +100 (Cima), 0 no centro */
} joystick_data_t;

/**
 * @brief Inicializa o hardware do ADC, cria as primitivas de sincronização 
 * e inicia a tarefa de amostragem do FreeRTOS.
 */
void joystick_hal_init(void);

/**
 * @brief Retorna o último estado processado e limpo do joystick.
 * Função thread-safe (pode ser chamada pela lógica do jogo em outro core).
 * * @return joystick_data_t Estrutura com valores de X e Y normalizados.
 */
joystick_data_t joystick_get_state(void);

#ifdef __cplusplus
}
#endif

#endif // JOYSTICK_HAL_H