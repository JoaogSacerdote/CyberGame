# CyberSim — Simulador PC do firmware

Roda os mesmos `components/ui/screen_*.c` do firmware numa janela SDL no PC,
sem precisar flashar o aparelho a cada mudança visual.

## Estado atual

**Etapa 1** — Prova de conceito: SDL2 + LVGL 9.3.0 funcionando.
Ainda não integra os components do firmware.

| Etapa | Status |
|---|---|
| 1. Setup SDL + LVGL                                       | ✅ |
| 2. HALs fake (joystick/botões via teclado) + FreeRTOS shim| ⏳ |
| 3. Storage fake (lê `nand.bin`) + asset_store + dialog    | ⏳ |
| 4. Engine + FSM + telas do firmware compilando            | ⏳ |

## Pré-requisitos

- MSYS2 instalado em `C:\msys64` com `mingw64/bin` (gcc, cmake, ninja, SDL2)
- Pacote: `pacman -S mingw-w64-x86_64-SDL2`

## Compilar e rodar

```bat
COMPILAR.bat
RODAR.bat
```

Resultado esperado da Etapa 1: janela 480×320 com fundo escuro e mensagem
"CyberSim — Etapa 1: pronto". Feche pra encerrar.

## Estrutura (futura)

```
pc_simulator/
  CMakeLists.txt           # build CMake usando SDL2 + MinGW
  main.c                   # app_main equivalente
  lv_conf.h                # config LVGL pra SDL (LV_COLOR_DEPTH=32, LV_USE_SDL=1)
  esp_idf_shim/            # esp_log, esp_err, esp_check, esp_heap_caps → stubs
  freertos_shim/           # xTaskCreate, xQueue, vTaskDelay → pthread
  hal_fake/                # display/joystick/button/nfc/pmu/storage fakes
  tools/                   # gerar nand.bin a partir do asset_registry.json
```
