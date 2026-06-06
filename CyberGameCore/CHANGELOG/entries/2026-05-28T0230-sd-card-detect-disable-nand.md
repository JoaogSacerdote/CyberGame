---
data: 2026-05-28T02:30
tipo: add + edit (codigo)
escopo: components/hardware, main
trigger: usuario decidiu abandonar a NAND (defeituosa) e usar o slot microSD embutido no modulo de display LCDWIKI como armazenamento
commits:
  - (pendente — ainda nao commitado)
---

# Deteccao do microSD (sd_hal) + NAND desativada no boot

## Por que

A NAND W25N01GV vinha dando problema (ver
[[project_pinout_revisao_2026_05_21]] "NAND com defeito"). O modulo de
display LCDWIKI MSP4030/4031 ja tem um **slot microSD embutido** (ver
[[project_display_module]]) — muito mais simples de usar (tira o cartao,
copia arquivos no PC, recoloca; sem upload por USB CDC).

Decisao do usuario 2026-05-28: abandonar a NAND e migrar o armazenamento
para o microSD. **Esta fase = so DETECTAR o cartao** (montar FATFS + listar
arquivos). A migracao do asset_loader/dialog_loader pra ler do FATFS fica
para a Fase 2.

Hardware (confirmado com o usuario):
- NAND continua **soldada** -> SD precisa de CS proprio (nao da pra
  reaproveitar o GPIO 41 da NAND, senao os dois respondem ao mesmo select).
- SD_CS = **GPIO 47** (livre).
- SD compartilha MOSI(15)/SCK(16)/MISO(18) — as mesmas linhas do display e
  da NAND. Com a NAND desativada no software (CS 41 nunca acionado), o chip
  fica em tri-state e nao briga na linha MISO.
- Pinos do modulo que o usuario precisa ligar (estavam NC): **9 SDO(MISO) ->
  GPIO 18** e **14 SD_CS -> GPIO 47**.

## Antes

`board_pins.h`: sem define de SD. Ultima secao era "Feedback".

`main.c` boot NORMAL inicializava a NAND:
```c
if (storage_hal_init() != ESP_OK) {
    ESP_LOGW(TAG, "[BRING-UP] NAND ausente — boot continua sem asset_store.");
} else if (asset_store_init() != ESP_OK) {
    ESP_LOGW(TAG, "[BRING-UP] asset_store_init falhou — boot continua sem assets.");
} else {
    size_t n = 0;
    asset_store_count(&n);
    ESP_LOGI(TAG, "asset_store pronto: %u entries", (unsigned)n);
}
```

Nao havia nenhum codigo de SD/FATFS/SDSPI no projeto.

## Depois

Arquivos novos:
- `components/hardware/include/sd_hal.h` + `sd_hal.c`
  Monta o microSD via **SDSPI no SPI2_HOST** (mesmo barramento que o
  display_hal ja inicializa — sd_hal NAO chama spi_bus_initialize de novo),
  FATFS em `/sd`. API: `sd_hal_init` / `sd_hal_is_mounted` / `sd_hal_list_root`.

Edicoes:
- `board_pins.h`: secao "Cartao microSD" com `BOARD_PIN_SD_CS=GPIO_NUM_47`
  + comentario das linhas compartilhadas + `_Static_assert(SD_CS != NAND_CS)`.
- `CMakeLists.txt` (hardware): +`sd_hal.c`; REQUIRES +`fatfs` +`sdmmc`
  +`esp_driver_sdspi`.
- `main.c`: bloco da NAND (storage_hal_init + asset_store_init) COMENTADO
  com marcador "NAND DESATIVADA 2026-05-28"; adicionado sd_hal_init +
  sd_hal_list_root logo apos o display (que sobe o barramento SPI2).

## Resultado

**Build VERDE** (idf.py build, exit 0). sd_hal.c compila. CyberGame.bin
cresceu de 0x931e0 -> 0xa2810 (~666 KB, 84%% livre) por causa do
FATFS/SDSPI/sdmmc. Componentes IDF (fatfs/sdmmc/esp_driver_sdspi) — sem
managed_component novo.

Pendente flash + validacao em HW. Esperado no log de boot: SDSPI monta,
imprime tipo/tamanho do cartao (`sdmmc_card_print_info`) e lista o conteudo
de `/sd`. Se nao montar, a mensagem de erro ja aponta o que checar
(cartao inserido, MISO=18, SD_CS=47, barramento SPI2).

**ATUALIZACAO 2026-05-28 — VALIDADO EM HW.** O cartao monta (log: `SU02G
SDSC 1886MB`). E a "tela nao acende com SD" tinha causa raiz **cartao
DEFEITUOSO/mal-formatado**: ele puxava demais o rail VCC compartilhado (ou
segurava o barramento), derrubando o painel. Com um cartao bom + FAT32, a
tela acende normal. **Conclusao: o rail compartilhado e adequado pra um
cartao saudavel — NAO precisa do upgrade de energia (MOSFET/cap) sugerido
antes.** Era o cartao, nao a fonte.

**Efeito colateral esperado**: com a NAND/asset_store desativados, as telas
(recepcao/empresa) carregam SEM imagens — asset_loader_load retorna erro e o
boot segue (modo bring-up ja tolera). Isso se resolve na Fase 2 (migrar o
asset_loader pra ler do FATFS).

## Links
- Memorias: [[project_display_module]], [[project_pinout_revisao_2026_05_21]],
  [[build_environment]], [[feedback_no_unauthorized_fixes]]
- Entrada anterior: [[2026-05-28T0158-feedback-hal-ws2812-buzzer]]
