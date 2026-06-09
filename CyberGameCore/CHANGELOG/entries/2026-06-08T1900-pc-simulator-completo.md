# 2026-06-08T1900 — PC Simulator completo (Etapa 4)

## O que mudou

Simulador PC evoluiu de "Etapa 1 — hello LVGL" para o jogo completo rodando no PC.

### Arquivos novos
- `pc_simulator/sim_engine.c` — substitui `engine.c` no simulador; roda o tick do engine como `lv_timer_create` callback (sem FreeRTOS tasks)
- `pc_simulator/esp_idf_shim/esp_err.h` — stubs `esp_err_t`, todos os `ESP_ERR_*`
- `pc_simulator/esp_idf_shim/esp_log.h` — `ESP_LOGI/W/E` → `printf`/`fprintf(stderr)`
- `pc_simulator/esp_idf_shim/esp_check.h` — `ESP_RETURN_ON_ERROR`, `ESP_RETURN_ON_FALSE`
- `pc_simulator/esp_idf_shim/esp_heap_caps.h` — `heap_caps_malloc` → `malloc`
- `pc_simulator/esp_idf_shim/esp_random.h` — `esp_random` → `rand()`
- `pc_simulator/esp_idf_shim/esp_attr.h` — `IRAM_ATTR` etc. → vazio
- `pc_simulator/esp_idf_shim/esp_timer.h` — `esp_timer_get_time` → `SDL_GetTicks()*1000`
- `pc_simulator/esp_idf_shim/freertos/FreeRTOS.h` + `task.h` + `queue.h` + `semphr.h` + `timers.h` — stubs no-op
- `pc_simulator/hal_fake/button_hal.c` — teclado SDL (Z=A, X=B, C=X, V=Y, Enter=START)
- `pc_simulator/hal_fake/joystick_hal.c` — setas mapeiam eixos ±100
- `pc_simulator/hal_fake/ws2812_hal.c` — stubs no-op (LEDs não visíveis no sim)
- `pc_simulator/hal_fake/pmu.c`, `nfc_hal.c`, `buzzer_hal.c`, `hal_common.c` — stubs

### Arquivos modificados
- `pc_simulator/main.c` — simplificado: `lv_init` + SDL display + `engine_init/start` + loop
- `pc_simulator/CMakeLists.txt` — adiciona todos os componentes do firmware como fontes
- `pc_simulator/COMPILAR.bat` — atualiza título para "simulador completo"
- `components/assets/asset_loader.c:15` — `#ifndef ASSET_SD_DIR` guard
- `components/assets/dialog_loader.c:14` — idem
- `components/entity/room_layout_sd.c:22` — `#ifndef LAYOUT_SD_DIR` guard

### Assets redirecionados
`ASSET_SD_DIR` e `LAYOUT_SD_DIR` apontam para `../../sdcard/assets` (relativo ao
CWD `pc_simulator/bin/` definido em RODAR.bat).

## ANTES

`cybersim.exe` mostrava label estático "CyberSim — Etapa 1: pronto". Nenhum
código do firmware era compilado.

## DEPOIS

`cybersim.exe` roda o jogo completo: splash → menu → recepcao/empresa, NPCs,
Y-sort, diálogos, HUD, sistema de ataques, game over/vitória. Teclado controla
o personagem. Assets lidos de `sdcard/assets/`.
