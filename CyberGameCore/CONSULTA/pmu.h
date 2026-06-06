/**
 * @file pmu.h
 * @brief Header da Unidade de Gestão de Energia (PMU) e Máquina de Estados de Boot
 * @context Project Blacksmith (ESP32-S3)
 */

#ifndef PMU_H
#define PMU_H

#include <stdbool.h>

// Definições de Pinos (Ajuste conforme o esquemático real do Project Blacksmith)
// No ESP32-S3, pinos de 0 a 21 são pinos RTC (podem ser usados para ext0)
#define PMU_PIN_PWR GPIO_NUM_1
#define PMU_PIN_REC GPIO_NUM_5

/**
 * @brief Máquina de estados de modo de inicialização
 */
typedef enum {
    MODO_HIBERNACAO = 0,    // Retorno ao Deep Sleep (aborto)
    MODO_OPERACIONAL,       // Inicialização normal do sistema
    MODO_MANUTENCAO         // Inicialização em modo de segurança/recuperação
} pmu_boot_mode_t;

/**
 * @brief Inicializa a Unidade de Gestão de Energia e configura os GPIOs.
 */
void pmu_init(void);

/**
 * @brief Avalia o estado de boot executando o filtro temporal de 5 segundos.
 * Deve ser chamada no início do app_main().
 *
 * @return pmu_boot_mode_t O modo selecionado de operação.
 */
pmu_boot_mode_t pmu_check_boot_state(void);

/**
 * @brief Prepara o sistema para o desligamento (salvamento de logs) e entra em Deep Sleep.
 */
void pmu_shutdown(void);

#endif /* PMU_H */