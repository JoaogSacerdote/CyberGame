/**
 * @file pmu.c
 * @brief Implementação da Unidade de Gestão de Energia usando ESP-IDF
 * @context Project Blacksmith (ESP32-S3)
 */

#include "pmu.h"
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/rtc_io.h"
#include "esp_sleep.h"
#include "esp_log.h"

static const char *TAG = "PMU_BLACKSMITH";

// Tempo de handshake obrigatório em milissegundos
#define PMU_HANDSHAKE_TIME_MS 5000
#define PMU_CHECK_INTERVAL_MS 100

void pmu_init(void) {
    ESP_LOGI(TAG, "Inicializando Unidade de Gestão de Energia...");

    // Configuração do Botão REC (GPIO Padrão)
    gpio_config_t rec_conf = {
        .pin_bit_mask = (1ULL << PMU_PIN_REC),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,     // Acionamento em LOW/GND
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&rec_conf);

    // Configuração do Botão PWR (GPIO RTC)
    // Inicialmente configurado como GPIO padrão para leitura em modo ativo.
    // O domínio RTC assume o controle deste pino durante o Deep Sleep.
    gpio_config_t pwr_conf = {
        .pin_bit_mask = (1ULL << PMU_PIN_PWR),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,     // Acionamento em LOW/GND
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&pwr_conf);
}

pmu_boot_mode_t pmu_check_boot_state(void) {
    ESP_LOGI(TAG, "Iniciando Handshake de Boot (Filtro Temporal de %d ms)...", PMU_HANDSHAKE_TIME_MS);

    int checks_required = PMU_HANDSHAKE_TIME_MS / PMU_CHECK_INTERVAL_MS;

    // Estado 1: Filtro temporal de 5 segundos
    for (int i = 0; i < checks_required; i++) {
        // Como possui pull-up, nível LOW (0) significa pressionado.
        if (gpio_get_level(PMU_PIN_PWR) == 1) {
            ESP_LOGW(TAG, "Botao PWR solto antes de 5 segundos. Toque acidental detectado.");
            // Executa aborto de boot e volta a dormir imediatamente
            pmu_shutdown();
        }
        vTaskDelay(pdMS_TO_TICKS(PMU_CHECK_INTERVAL_MS));
    }

    ESP_LOGI(TAG, "Handshake concluido. Lendo modificador de boot...");

    // Seleção de Modo (Boot Modifier) após os 5 segundos
    // Verifica estado do botão REC. Nível LOW (0) significa pressionado.
    if (gpio_get_level(PMU_PIN_REC) == 0) {
        ESP_LOGI(TAG, "Botao REC detectado. Entrando em MODO_MANUTENCAO.");
        return MODO_MANUTENCAO;
    } else {
        ESP_LOGI(TAG, "Botao REC solto. Entrando em MODO_OPERACIONAL.");
        return MODO_OPERACIONAL;
    }
}

void pmu_shutdown(void) {
    ESP_LOGI(TAG, "Preparando para Soft Power Off (Deep Sleep)...");

    // Aqui seria implementada a lógica de salvar logs no NVS/SD Card
    ESP_LOGI(TAG, "Salvando logs de sistema... (Stub)");

    // Estado 0: Configuração de Hibernação
    // Isola os pads digitais para economizar energia, exceto o pino do PWR
    // Configura o pino PWR no domínio RTC para funcionar com resistores pull-up durante o sono
    rtc_gpio_init(PMU_PIN_PWR);
    rtc_gpio_set_direction(PMU_PIN_PWR, RTC_GPIO_MODE_INPUT_ONLY);
    rtc_gpio_pullup_en(PMU_PIN_PWR);
    rtc_gpio_pulldown_dis(PMU_PIN_PWR);

    // Define o ext0 wake-up no domínio RTC
    // Acorda o ESP32-S3 quando o pino PWR vai para nível baixo (0 / LOW)
    esp_err_t err = esp_sleep_enable_ext0_wakeup(PMU_PIN_PWR, 0);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Falha ao configurar ext0 wake-up: %s", esp_err_to_name(err));
    }

    ESP_LOGI(TAG, "Sistema entrando em Deep Sleep agora. Aguardando novo acionamento do PWR.");

    // Pequeno delay para garantir a impressão dos logs na UART antes de desligar
    vTaskDelay(pdMS_TO_TICKS(100)); 

    // Entra em hibernação profunda
    esp_deep_sleep_start();
}