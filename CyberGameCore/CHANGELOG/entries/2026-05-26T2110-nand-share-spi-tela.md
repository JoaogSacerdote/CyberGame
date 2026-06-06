---
date: 2026-05-26 21:10
type: edit
files:
  - components/hardware/storage_hal.c
  - components/hardware/display_hal.c
session: bring-up placa nova
---

# NAND compartilha SPI2 com a tela nova

## Motivo
Após re-pinagem da tela (T2100), os pinos antigos da NAND colidiram com a nova distribuição. Usuário escolheu **compartilhar SPI2** entre tela e NAND (mesmos MOSI/MISO/SCK, CS diferente — padrão SPI multi-device). Solução clássica e barata em GPIO.

## Antes (storage_hal.c linhas 14-17)
```c
#define STORAGE_PIN_MOSI       15
#define STORAGE_PIN_SCK        7
#define STORAGE_PIN_MISO       5
#define STORAGE_PIN_CS         41
```

## Depois
```c
#define STORAGE_PIN_MOSI       18
#define STORAGE_PIN_SCK        16
#define STORAGE_PIN_MISO       15
#define STORAGE_PIN_CS         41
```

Pinos MOSI/SCK/MISO agora iguais aos da tela (display_hal.c). Cada dispositivo tem seu CS:
- Tela: CS=5
- NAND: CS=41

Mensagem de erro do JEDEC também atualizada:
```c
/* antes */
ESP_LOGE(TAG, "Verificar fios: CS=GPIO41, MISO=GPIO5, MOSI=GPIO15, SCK=GPIO7, ...");
/* depois */
ESP_LOGE(TAG, "Verificar fios: CS=GPIO41, MISO=GPIO15, MOSI=GPIO18, SCK=GPIO16, ...");
```

Comentário do MISO no display_hal.c também atualizado pra "compartilhado com NAND" (era condicional).

## Resultado
SPI2 compartilhado. Display é dono do bus (chama `spi_bus_initialize` primeiro com `max_transfer_sz` grande pro framebuffer). Storage anexa via `spi_bus_add_device` tolerando `ESP_ERR_INVALID_STATE` (padrão atual do código já lida com isso).

## Links
- Entrada anterior: [[2026-05-26T2100-pinout-tela-v2]] (origem do conflito)
