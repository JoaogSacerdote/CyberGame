#include "Buton.h"
#include "driver/gpio.h"

void botoes_init(void) {
    // Cria uma máscara combinando os 4 pinos
    uint64_t pin_mask = (1ULL << BTN_UP) | (1ULL << BTN_DOWN) | (1ULL << BTN_LEFT) | (1ULL << BTN_RIGHT);

    gpio_config_t config_botoes = {
        .pin_bit_mask = pin_mask,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE, // Mantém o resistor interno ativado
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    
    // Aplica a configuração para todos os 4 botões de uma vez
    gpio_config(&config_botoes);
}

// Verifica se o botão específico foi apertado
bool botao_pressionado(int pino) {
    return gpio_get_level(pino) == 0;
}