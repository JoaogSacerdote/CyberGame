---
date: 2026-05-26 22:00
type: edit
files:
  - components/hardware/display_hal.c
  - components/hardware/storage_hal.c
session: bring-up placa nova
---

# Inversão MOSI ↔ MISO (correção do swap acidental do usuário)

## Motivo
Após PCB nova chegar e usuário flashar firmware com pinos novos (T2100), tela ficou branca eterna (estado de fábrica do ST7796 — RAM = 0xFFFF). Diagnóstico inicial cogitou MOSI/MISO swap no schematic da PCB; multímetro confirmou continuidade nos pinos certos.

**Causa real:** usuário trocou os nomes "MISO" e "MOSI" quando me ditou os pinos da tela na T2100. Ou seja, a PCB está correta, mas o firmware ficou com os 2 sinais invertidos. Como display é write-only e manda tudo por MOSI, comandos nunca chegavam → tela fica em estado de fábrica.

## Mudanças

| Função | T2100 (errado) | T2200 (correto) |
|---|---|---|
| LCD MOSI | 18 | **15** |
| LCD MISO | 15 | **18** |
| NAND MOSI | 18 | **15** |
| NAND MISO | 15 | **18** |

Demais pinos da tela (CS=5, RST=6, DC=7, SCK=16, BL=17, PWR_EN=42) e NAND (SCK=16, CS=41) **não mudam**.

## Antes

### `display_hal.c`
```c
#define DISP_PIN_MOSI       18
/* linha 135 */
.miso_io_num     = 15,  /* MISO compartilhado SPI2: tela (write-only) + NAND (read) */
```

### `storage_hal.c`
```c
#define STORAGE_PIN_MOSI       18
#define STORAGE_PIN_MISO       15
```

### Mensagem de erro JEDEC (storage_hal.c)
```c
ESP_LOGE(TAG, "Verificar fios: CS=GPIO41, MISO=GPIO15, MOSI=GPIO18, SCK=GPIO16, ...");
```

## Depois

### `display_hal.c`
```c
#define DISP_PIN_MOSI       15
/* linha 135 */
.miso_io_num     = 18,  /* MISO compartilhado SPI2: tela (write-only) + NAND (read) */
```

### `storage_hal.c`
```c
#define STORAGE_PIN_MOSI       15
#define STORAGE_PIN_MISO       18
```

### Mensagem de erro JEDEC atualizada
```c
ESP_LOGE(TAG, "Verificar fios: CS=GPIO41, MISO=GPIO18, MOSI=GPIO15, SCK=GPIO16, ...");
```

## Resultado esperado
Tela deve sair do branco e mostrar conteúdo (splash screen, depois UI do jogo) quando o usuário reflashar e bootar. NAND deve voltar a ler JEDEC ID corretamente.

## Lição
Continuidade de multímetro confirma **conexão elétrica entre dois pontos**, mas não verifica **se o nome do sinal naquele ponto está correto**. MOSI/MISO swap é o tipo de erro que multímetro não pega — só osciloscópio ou comparação cruzada com datasheet do display.

## Links
- Entrada substituída: [[2026-05-26T2100-pinout-tela-v2]] (a versão com swap)
- Entrada substituída: [[2026-05-26T2110-nand-share-spi-tela]] (NAND com pinos antes do swap)
