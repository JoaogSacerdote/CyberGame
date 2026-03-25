#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "Buton.h"

void app_main(void) {
    printf("Sistema Iniciado! Configurando D-Pad...\n");

    // Inicializa todos os botões
    botoes_init();

    printf("Botões prontos! Pressione-os no Wokwi.\n");

    while (1) {
        // Verifica cada botão individualmente
        if (botao_pressionado(BTN_UP)) {
            printf("-> CIMA (Pino 15) pressionado!\n");
        }
        if (botao_pressionado(BTN_DOWN)) {
            printf("-> BAIXO (Pino 16) pressionado!\n");
        }
        if (botao_pressionado(BTN_LEFT)) {
            printf("-> ESQUERDA (Pino 17) pressionado!\n");
        }
        if (botao_pressionado(BTN_RIGHT)) {
            printf("-> DIREITA (Pino 18) pressionado!\n");
        }
        
        // Pausa curta para leitura rápida e sem travar o chip
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}