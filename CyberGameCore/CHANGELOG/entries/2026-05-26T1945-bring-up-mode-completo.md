---
date: 2026-05-26 19:45
type: edit
files:
  - main/main.c
session: bring-up placa nova
---

# MODO BRING-UP completo — todos os HALs tolerantes a hardware ausente

## Motivo
Placa nova em estágio de bring-up: só ESP montado, sem display/NFC/NAND/joystick. Para iterar testes sem o firmware abortar a cada componente faltando, todos os `ESP_ERROR_CHECK` e idle-loops foram trocados por logs `ESP_LOGW` + continuação do boot. Cada componente faltando gera log com tag clara `[BRING-UP]` pra ficar óbvio no monitor UART.

Pedido explícito do usuário: **manter logs de aviso visíveis** + **fácil reversão** (todo bloco modificado tem marcador `MODO BRING-UP TEMPORARIO` pra `grep`).

## Antes (main.c linhas 103-152)
```c
/* HALs de input — button_hal claim do GPIO 3 (REC -> START) com settle interno. */
ESP_ERROR_CHECK(button_hal_init());
ESP_ERROR_CHECK(joystick_hal_init());
if (nfc_hal_init() != ESP_OK) {
    ESP_LOGW(TAG, "nfc_hal_init falhou — sem NFC. Boot continua.");
}

/* Display antes do storage: ele eh o dono do SPI2 (precisa de max_transfer_sz
 * grande para framebuffer). Storage anexa depois via spi_bus_add_device,
 * tolerando ESP_ERR_INVALID_STATE no spi_bus_initialize dele. */
if (display_hal_init() != ESP_OK) {
    ESP_LOGE(TAG, "display_hal_init falhou — sem video. Idle.");
    while (1) vTaskDelay(pdMS_TO_TICKS(1000));
}
if (hal_bridge_init() != ESP_OK) {
    ESP_LOGE(TAG, "hal_bridge_init falhou — sem UI. Idle.");
    while (1) vTaskDelay(pdMS_TO_TICKS(1000));
}
display_hal_set_backlight_percent(70);

/* Storage: nao usa ESP_ERROR_CHECK — se a NAND nao responder, queremos
 * que o resto do sistema continue funcional para diagnostico via UART. */
if (storage_hal_init() != ESP_OK) {
    ESP_LOGE(TAG, "storage_hal_init falhou — NAND inacessivel. Boot continua.");
} else if (asset_store_init() != ESP_OK) {
    ESP_LOGE(TAG, "asset_store_init falhou — assets indisponiveis. Boot continua.");
} else {
    size_t n = 0;
    asset_store_count(&n);
    ESP_LOGI(TAG, "asset_store pronto: %u entries", (unsigned)n);
}

/* Combo dev: Y+START segurados juntos por 2s -> ui_debug. */
if (detect_dev_combo()) {
    ESP_LOGW(TAG, "MODO DEV: iniciando ui_debug ao inves do engine.");
    if (ui_debug_init() != ESP_OK) {
        ESP_LOGE(TAG, "ui_debug_init falhou — boot continua em idle.");
    }
    while (1) vTaskDelay(pdMS_TO_TICKS(1000));
}

/* MODO GAME normal. */
if (engine_init() != ESP_OK) {
    ESP_LOGE(TAG, "engine_init falhou — boot continua em idle.");
    while (1) vTaskDelay(pdMS_TO_TICKS(1000));
}
if (engine_start() != ESP_OK) {
    ESP_LOGE(TAG, "engine_start falhou — engine nao recebera eventos.");
}
```

## Depois
Bloco inteiro envelopado em comentários `=== MODO BRING-UP TEMPORARIO (2026-05-26) — INICIO/FIM ===`. Cada HAL gera `ESP_LOGW(TAG, "[BRING-UP] xxxxx ausente: %s", esp_err_to_name(err))`. Sem display ou hal_bridge, cai em idle com heartbeat `"[BRING-UP] alive — sem UI"` a cada 5s.

## Resultado
Permite testar a placa em qualquer combinação de componentes conectados. Para reverter: `grep "MODO BRING-UP TEMPORARIO"` em `main/main.c` e restaurar o bloco original (preservado integralmente nesta entrada).

## Como reverter
1. Em `main/main.c`, achar `=== MODO BRING-UP TEMPORARIO (2026-05-26) — INICIO ===`
2. Substituir tudo até `=== MODO BRING-UP TEMPORARIO (2026-05-26) — FIM ===` pelo bloco "Antes" desta entrada
3. Tirar a entrada do `INDEX.md` (opcional — manter histórico é melhor)

## Links
- Entrada relacionada: [[2026-05-26T1900-nfc-tolerante]] (mesma intenção, escopo menor)
- Entrada relacionada: [[2026-05-26T1930-reaplica-pinout]] (pinout que precede esse teste)
