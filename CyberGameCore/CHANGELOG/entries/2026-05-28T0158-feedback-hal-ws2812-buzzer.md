---
data: 2026-05-28T01:58
tipo: add (codigo)
escopo: components/hardware, main, idf_component.yml
trigger: usuario pediu para definir pinos do LED + buzzer e criar o HAL de ambos (reativa "Etapa B / feedback_hal" que estava adiada pos-MVP)
commits:
  - (pendente — ainda nao commitado)
---

# feedback_hal — ws2812_hal + buzzer_hal (Etapa B)

## Por que

A "Etapa B" (feedback_hal: WS2812 + buzzer) tinha sido adiada para pos-MVP
em 2026-05-13 (ver [[game_logic_decisions]]). Usuario reativou em 2026-05-28
pedindo: definir os pinos e criar o HAL de ambos.

Pinos confirmados pelo usuario contra o schematic KiCad de 2026-05-21:
- WS2812 Data = GPIO 8
- Buzzer PWM  = GPIO 21

Ambos estavam LIVRES no `board_pins.h` atual (nenhuma memoria batia: a antiga
dizia LED=48/buzzer=15; a de 2026-05-21 dizia 8/21 mas o codigo nao tinha
nenhum dos dois). Codigo era a verdade -> adicionados do zero.

Decisoes de design (confirmadas):
- Dois HALs separados (`ws2812_hal` + `buzzer_hal`), um .c por periferico,
  seguindo o padrao de button/joystick/nfc_hal.
- Driver WS2812 = componente gerenciado `espressif/led_strip` (RMT).
- HALs "burros": expoem primitivas de hardware, SEM semantica de jogo. A
  logica composta de progressao de ataque (LED 3+2+1 piscando etc., ver
  [[game_logic_decisions]]) fica no game logic, NAO no HAL (regra
  [[hal_boundary_contract]]).

## Antes

`board_pins.h`: nao tinha NENHUM define de WS2812 nem buzzer. Ultima secao
era NAND:
```c
#define BOARD_PIN_NAND_MISO          GPIO_NUM_18
#define BOARD_PIN_NAND_CS            GPIO_NUM_41
```

`components/hardware/CMakeLists.txt` SRCS terminava em `display_hal.c`, sem
ws2812_hal.c/buzzer_hal.c; REQUIRES sem `espressif__led_strip` nem `esp_timer`.

`main/idf_component.yml` dependencias: so `espressif/esp_lcd_st7796`.

`main.c` boot NORMAL: inicializava button/joystick/nfc/display/storage; sem
ws2812_hal_init nem buzzer_hal_init.

## Depois

Arquivos novos:
- `components/hardware/include/ws2812_hal.h` + `ws2812_hal.c`
  API: init / set_pixel / set_all / clear / refresh. `WS2812_LED_COUNT 3`.
  led_strip RMT 10 MHz, formato GRB, sem DMA.
- `components/hardware/include/buzzer_hal.h` + `buzzer_hal.c`
  API: init / tone(freq) / stop / beep(freq,ms) nao-bloqueante / set_muted.
  LEDC **TIMER_1 + CHANNEL_1** (backlight do display usa TIMER_0/CHANNEL_0 —
  separados de proposito pra mudar freq do tom nao alterar o brilho da tela).
  beep usa esp_timer one-shot pra auto-stop sem bloquear o chamador.

Edicoes:
- `board_pins.h`: nova secao "Feedback (LED + buzzer)" com
  `BOARD_PIN_WS2812_DATA=GPIO_NUM_8` e `BOARD_PIN_BUZZER=GPIO_NUM_21`.
- `CMakeLists.txt` (hardware): +2 SRCS, +`espressif__led_strip` +`esp_timer`
  em REQUIRES.
- `components/hardware/idf_component.yml`: +`espressif/led_strip: "^3.0"`.
- `main.c`: ws2812_hal_init + buzzer_hal_init no boot NORMAL, padrao
  bring-up tolerante (log + segue; sem ESP_ERROR_CHECK que brica boot).

## Resultado

**Build VERDE** (idf.py build, exit 0). CyberGame.bin = 0x931e0 bytes
(~602 KB), 86%% livre na partição app de 4M. ws2812_hal.c + buzzer_hal.c
compilam sem warning.

**Gotcha do led_strip x IDF 6.0**: comecei com `"^2.5"` -> resolveu 2.5.5,
que NAO compila no IDF 6.0 (dois erros: backend SPI sem include de
`esp_heap_caps.h` -> `MALLOC_CAP_*` indefinido; e API antiga
`led_pixel_format` em vez de `color_component_format`). Subi para `"^3.0"`
-> resolveu **3.0.3**, que tem a API nova (que meu ws2812_hal.c ja usa) e
compila limpo. **Licao: no IDF 6.0 usar led_strip >= 3.0.**

Tambem foi preciso apagar `build/` (estava com generator MinGW Makefiles de
uma tentativa antiga via MSYS; IDF 6.0 quer Ninja).

Pendente: validacao em HW (acender os 3 LEDs + tocar um beep) apos flash.

## Links
- Memorias: [[game_logic_decisions]], [[project_pinout_revisao_2026_05_21]],
  [[hal_boundary_contract]], [[feedback_no_unauthorized_fixes]]
- Pendencia relacionada: OPEN_QUESTIONS nao listava Etapa B como gap (estava
  fora do MVP) — reativada por pedido direto.
