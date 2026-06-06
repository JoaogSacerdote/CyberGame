---
tipo: historical
status: obsoleto
area: firmware
data: 2026-05-08
arquivada-em: 2026-05-27
motivo-arquivamento: pinout HARDCODED ESTÁ ERRADO (cita MOSI=7, SCK=8, CS=17, DC=18, RST=21, BL=38 — todos diferentes do board_pins.h vigente). A documentação textual do fluxo de init / DMA / sleep ainda tem valor de referência conceitual.
tags: [cybersec, firmware, hal, hardware, display, fase2]
componente: ST7796S
barramento: SPI
---

> [!warning] NOTA OBSOLETA — pinout incorreto
> Esta nota tem **pinout incorreto** (de 2026-05-08, antes da PCB nova).
> Para pinout vigente: `components/hardware/include/board_pins.h`
> (`BOARD_PIN_DISP_*`).
> Para estado atual do driver: `components/hardware/display_hal.c`.
> O conteúdo textual sobre fluxo de init, DMA em PSRAM e protocolo de
> sleep anti-backpowering ainda é referência conceitual válida.

# Driver ST7796S — `display_hal`

> Fronteira de hardware do display TFT 480×320. Fala apenas a lingua do ESP-IDF (`esp_lcd`, `spi_master`, `ledc`). **Nao conhece LVGL** — a integracao com LVGL e responsabilidade de [[hal-bridge]].

## 1. Resumo arquitetural

| Item | Decisao | Justificativa |
|---|---|---|
| API ESP-IDF | `esp_lcd` panel API + componente `espressif/esp_lcd_st7796` | Padrao oficial, suporte a DMA SPI, integra naturalmente com LVGL via `flush_cb` |
| Bus SPI | `SPI2_HOST` a 40 MHz inicialmente (com headroom para 80 MHz) | ST7796S suporta ate 75 MHz; 40 MHz da margem para o bus compartilhado com NAND W25N01G |
| CS | Distinto por dispositivo (`DISPLAY_CS`, `NAND_CS`) | Multiplexacao no mesmo bus SPI sem conflito (`spi_bus_add_device` por chip) |
| Color depth | **RGB565** (16 bpp, 2 bytes/pixel) | Trade-off otimo: 480×320×2 = 307,2 KB por frame, suportavel em PSRAM |
| Buffers | **Double-buffer em PSRAM Octal 80 MHz** (`MALLOC_CAP_SPIRAM \| MALLOC_CAP_DMA`) | Conforme decisao de arquitetura. ~614 KB total em PSRAM (8 MB disponivel) |
| Backlight | LEDC PWM em GPIO 38 (10 kHz, duty 13-bit) | Permite dimming progressivo e fade-in suave no boot |
| Reset | GPIO 21, ativo-baixo, pulso minimo 10 ms (manter 120 ms para tela limpa) | Conforme datasheet ST7796S Rev. 1.0 |

> [!warning] Divergencia com o simulador
> O simulador em `simulation/lv_port_pc_eclipse/lv_conf.h` usa `LV_COLOR_DEPTH = 32` (ARGB8888) porque roda em PC sem restricao de memoria. **No firmware embarcado, sera obrigatoriamente 16 (RGB565)** para nao estourar a PSRAM e maximizar a vazao SPI. Assets pixel-art convertidos com `lvgl-image-converter` devem ser exportados em RGB565 para o ESP-IDF — confirmado em `CONSULTA/Artigo.pdf` Secao 3.4: *"Os Elementos graficos do jogo sao produzidos em estilo pixel art e convertidos para arrays C por meio da ferramenta lvgl-image-converter, que os codifica no formato RGB565 compativel com a LVGL"*.

## 2. Mapa de pinos

Conforme [[pinout-mestre]] (memoria `project_hardware`, 2026-05-07):

| Sinal | GPIO | Tipo | Observacao |
|---|---|---|---|
| MOSI | 7 | SPI saida | **Compartilhado com NAND W25N01G** |
| SCK | 8 | SPI clock | **Compartilhado com NAND** |
| CS | 17 | GPIO out | Exclusivo do display (NAND tem CS=10) |
| DC | 18 | GPIO out | Data/Command select (alto = data, baixo = comando) |
| RST | 21 | GPIO out | Ativo-baixo |
| BL | 38 | LEDC PWM | Backlight (10 kHz, duty 13-bit) |
| **VCC enable** | **42** | **GPIO out (NPN)** | **1=LIGA, 0=CORTA** (substituido de PNP em 2026-05-10). Sequenciamento e protocolo de sleep abaixo |

> [!info] MISO ausente?
> O ST7796S nao precisa de leitura para renderizacao normal (apenas write-only no fluxo grafico). MISO fica reservado para a NAND (GPIO 9). Se um dia precisarmos ler ID do display para self-test, adicionar leitura via mesmo MISO da NAND com mux por CS.

## 3. Memoria e DMA — fundamentacao tecnica

### 3.1 Por que double-buffer em PSRAM?

O ESP32-S3 tem ~512 KB de SRAM interna, mas grande parte e ocupada pelo runtime FreeRTOS, stacks de tarefas e buffers do esp_lcd / Wi-Fi. **Um framebuffer 480×320 RGB565 = 307,2 KB ocuparia ~60 % da SRAM interna**, deixando margem insuficiente para as 4 tasks concorrentes do projeto (descritas em `CONSULTA/Artigo.pdf` Secao 3.4: Tarefa de Jogo, Tarefa Grafica, Tarefa NFC, Tarefa de Audio).

Solucao: alocar os dois framebuffers em **PSRAM Octal** via `heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_DMA)`. A PSRAM Octal a 80 MHz oferece banda suficiente para o pipeline LVGL→SPI sem stalls perceptiveis.

### 3.2 Por que double-buffer (vs single)?

Single-buffer parcial obriga LVGL a renderizar e flushar **regiao por regiao** sequencialmente — cada flush bloqueia a renderizacao da proxima. Double-buffer permite o classico **ping-pong**: enquanto o buffer A esta sendo transmitido por DMA SPI ao display, o buffer B e renderizado pela CPU. Resultado: animacoes suaves para os efeitos visuais criticos do jogo (chamas DDoS, criptografia ransomware) descritos em [[matriz-reacao-ataques]].

### 3.3 Restricoes de DMA em PSRAM

> [!warning] Caveat documentado
> No ESP32-S3, o DMA pode ler diretamente da PSRAM Octal via EDMA, **desde que** o buffer esteja alinhado ao limite da cache (tipicamente 32 ou 64 bytes). `heap_caps_malloc` com `MALLOC_CAP_DMA` ja garante alinhamento minimo, mas `LV_DRAW_BUF_ALIGN` em `lv_conf.h` deve ser elevado para 64 (ou pelo menos 16) na configuracao do firmware. O simulador usa 4 (suficiente para PC, insuficiente para nosso DMA).

### 3.4 Sincronizacao entre DMA e LVGL

O `esp_lcd` panel API expoe um callback `on_color_trans_done` disparado pelo driver SPI quando a transferencia DMA completa. Esse callback e a **unica ponte** que `display_hal` precisa expor para a `hal_bridge`: a bridge registra um trampolim que chama `lv_display_flush_ready()` da LVGL, fechando o ciclo de renderizacao.

Importante: o tipo do callback exposto pelo `display_hal.h` e `void (*)(void*)` — **nao e tipo LVGL**, e a bridge faz o cast/adaptacao. Mantem a fronteira limpa.

## 4. Fluxo de inicializacao previsto

```
display_hal_init()
  ├── 0. gpio_hold_dis em todos os pinos do display (caso voltando de Deep Sleep)
  ├── 1. Configurar GPIO 42 (VCC enable NPN) como output e setar 1 → LIGA VCC
  │       vTaskDelay(50 ms) para estabilizar 3.3V no painel
  ├── 2. Configurar GPIOs RST, DC (output)
  ├── 3. spi_bus_initialize(SPI2_HOST, ...) com PSRAM-aware DMA
  ├── 4. esp_lcd_new_panel_io_spi(...) com pinout DC/CS, ack callback
  ├── 5. esp_lcd_new_panel_st7796(...) panel handle
  ├── 6. esp_lcd_panel_reset() (pulso GPIO 21)
  ├── 7. esp_lcd_panel_init() (envia comandos de power/sleep-out)
  ├── 8. esp_lcd_panel_swap_xy(true) + esp_lcd_panel_mirror(...) → orientacao landscape 480×320
  ├── 9. esp_lcd_panel_invert_color(true) → ST7796S costuma exigir invert
  ├── 10. ledc_timer_config + ledc_channel_config (BL PWM 10 kHz)
  └── 11. esp_lcd_panel_disp_on_off(true) + fade-in backlight 0→100% em 200 ms
```

### 4.1 Protocolo de sleep (anti-backpowering)

> Necessario porque, com o VCC do painel cortado, qualquer linha SPI em nivel alto vira fonte parasita de corrente atraves dos diodos de protecao internos do ST7796S. Em Deep Sleep prolongado isso drena a bateria.

```
display_hal_sleep()
  ├── 1. Backlight → 0% (LEDC duty 0)
  ├── 2. esp_lcd_panel_disp_on_off(false) + vTaskDelay(20 ms)
  ├── 3. gpio_set_level(GPIO 42, 0) → CORTA VCC do painel (NPN: LOW=desliga)
  ├── 4. Puxar linhas para niveis seguros:
  │       MOSI=0, CLK=0, DC=0, RST=0, CS=1, BL=0
  ├── 5. gpio_hold_en em todos esses pinos (trava durante Deep Sleep RTC)
  └── 6. (PMU pode chamar esp_deep_sleep_start em seguida)
```

Padrao de referencia: `CONSULTA/display_hal.c` funcao `display_hal_sleep` — **atencao: aquele arquivo ainda usa polaridade PNP (0=LIGA, 1=CORTA)**, inverter ao adaptar. Memoria `project_hardware` atualizada em 2026-05-10 com NPN.

A `hal_bridge` chamara apos isso:
```
display_hal_get_panel_handle() → esp_lcd_panel_handle_t
display_hal_register_trans_done_cb(bridge_trampoline, lv_disp)
```

## 5. Componentes ESP-IDF necessarios

`components/hardware/CMakeLists.txt` precisara declarar:

```cmake
REQUIRES driver esp_driver_spi esp_lcd esp_driver_ledc esp_driver_gpio esp_psram log
```

E `components/hardware/idf_component.yml` (a criar):

```yaml
dependencies:
  espressif/esp_lcd_st7796: "^1.2"
```

Adicionar via `idf.py add-dependency "espressif/esp_lcd_st7796^1.2"`.

## 6. Configuracoes de `sdkconfig`

Confirmar (ou adicionar via `idf.py menuconfig`):

```
CONFIG_SPIRAM=y
CONFIG_SPIRAM_MODE_OCT=y
CONFIG_SPIRAM_SPEED_80M=y
CONFIG_SPIRAM_USE_MALLOC=y
CONFIG_SPIRAM_MALLOC_ALWAYSINTERNAL=16384   # so allocs > 16K vao para PSRAM
CONFIG_SPI_MASTER_IN_IRAM=y
CONFIG_LCD_RGB_ISR_IRAM_SAFE=y
```

## 7. Riscos e pontos de atencao

- **Bus SPI compartilhado com NAND**: garantir que `spi_bus_add_device` use estruturas separadas para display e NAND, com `cs_io_num` distintos. Risco baixo se feito corretamente.
- **Inversao de cor**: ST7796S frequentemente requer `esp_lcd_panel_invert_color(true)` — testar cores na primeira boot e ajustar.
- **Tearing**: se aparecer, considerar habilitar TE (Tearing Effect) pin se hardware suportar — checar schematic. Atualmente nao mapeado.
- **Cache invalidation em PSRAM**: ao escrever no framebuffer e antes de despachar via DMA, garantir flush da cache se o ESP-IDF nao fizer automaticamente (depende da versao).

## 8. API publica acordada (resumo, ver `display_hal.h`)

```c
esp_err_t  display_hal_init(void);
esp_err_t  display_hal_deinit(void);
esp_err_t  display_hal_draw_bitmap(int x_start, int y_start, int x_end, int y_end, const void *pixel_data);
esp_err_t  display_hal_register_trans_done_cb(display_hal_trans_done_cb_t cb, void *user_ctx);
esp_err_t  display_hal_set_backlight_percent(uint8_t pct);
esp_err_t  display_hal_on(bool on);
uint16_t   display_hal_get_width(void);
uint16_t   display_hal_get_height(void);
uint8_t    display_hal_get_bytes_per_pixel(void);
void      *display_hal_get_panel_handle(void);   /* opaque, hal_bridge fara cast */
```

## 9. Referencias

- `CONSULTA/Artigo.pdf` Secao 3.3 (Arquitetura de Hardware, Tabela 2) — display ST7796S 320×240 [sic — manual canonico em 480×320 confirmado pelo pinout]
- `CONSULTA/Artigo.pdf` Secao 3.4 (Arquitetura de Software) — assets RGB565 via `lvgl-image-converter`
- `CONSULTA/Artigo.pdf` Secao 2.6 (Interface Grafica Embarcada: LVGL)
- `simulation/lv_port_pc_eclipse/lv_conf.h` (somente leitura) — versao **LVGL 9.3.0**, `LV_COLOR_DEPTH=32`, `LV_DEF_REFR_PERIOD=33`
- Memoria `project_hardware` — pinout definitivo
- ESP-IDF docs: `esp_lcd` Component, `driver_spi_master`, `esp_psram`
