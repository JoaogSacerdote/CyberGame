---
date: 2026-05-26 19:00
type: edit
files:
  - main/main.c
session: bring-up placa nova
---

# NFC tolerante em main.c — bench sem NFC físico

## Motivo
Após o boot finalmente rodar (PSRAM disabled), firmware crashou em `ESP_ERROR_CHECK(nfc_hal_init())` porque o módulo NFC não está conectado na placa nova (placa em estágio de bring-up, só ESP montado). Usuário confirmou "está ok porque realmente está sem ele" e pediu pra tornar NFC tolerante.

## Antes
```c
/* main/main.c linha 106 */
ESP_ERROR_CHECK(nfc_hal_init());
```

## Depois
```c
if (nfc_hal_init() != ESP_OK) {
    ESP_LOGW(TAG, "nfc_hal_init falhou — sem NFC. Boot continua.");
}
```

## Resultado
Não testado ainda (usuário não vai gravar agora). Próximas iterações provavelmente vão precisar do mesmo tratamento para `display_hal_init` e `storage_hal_init`, mas usuário pediu mudança mínima por ora.

## Links
- Entrada anterior: [[2026-05-26T1845-fixes-build-pos-reset]]
- Memória: [[project_status_hals]]
