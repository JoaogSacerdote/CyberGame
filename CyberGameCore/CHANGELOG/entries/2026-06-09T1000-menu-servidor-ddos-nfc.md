---
date: 2026-06-09T10:00
tipo: add+edit
escopo: ui+engine+assets
---

# Menu de servidor + tela DDoS + NFC integrado

## Arquivos criados

- `components/ui/include/screen_servidor_menu.h` — enum `servidor_menu_result_t`, callback typedef, build/destroy/is_open
- `components/ui/screen_servidor_menu.c` — overlay text-only, fundo `#1A3A4A`, 2 opções (Backup / WEB), nav joystick com cooldown
- `components/ui/include/screen_tarefa_vermelha.h` — build/destroy/is_open
- `components/ui/screen_tarefa_vermelha.c` — tela de status do Servidor WEB; detecta DDoS via `fsm_get_attack_active()`; auto-fecha ao ser mitigado

## Arquivos editados

- `components/assets/include/collision_data.h` — adicionado `AREA_SERVIDOR` ao enum (entre `AREA_TAREFA_AMARELA` e `AREA_SPAWN`)
- `components/assets/collision/collision_empresa.c` — ambos `SERVIDOR_ESQUERDA` e `SERVIDOR_DIREITA` mudados de `AREA_TAREFA_AMARELA` → `AREA_SERVIDOR`
- `components/ui/screen_empresa.c` — inclui novos headers; `on_servidor_menu_done` callback (Backup→tarefa_amarela, WEB→tarefa_vermelha); `near_srv` adicionado ao tick; tick guard expandido; BTN_A abre servidor_menu quando `near_srv`
- `components/ui/CMakeLists.txt` — srcs: `screen_servidor_menu.c` + `screen_tarefa_vermelha.c`
- `components/engine/engine.c` — inclui `nfc_hal.h`, `nfc_config.h`, `screen_tarefa_vermelha.h`; `s_nfc_initialized`; `nfc_resolve_uid()` (log UID + fallback CARTA_BALANCEAMENTO); `nfc_poll_tick()` (só quando `screen_tarefa_vermelha_is_open()`); `nfc_hal_init()` em `engine_init()`; `nfc_hal_start_scanning()` em `engine_start()`

## Comportamento

1. Player encosta em SERVIDOR_ESQUERDA ou SERVIDOR_DIREITA → [A] → menu "[ SERVIDOR ]"
2. Menu: "Servidor de Backup" → abre tarefa_amarela; "Servidor WEB" → abre tela vermelha
3. Tela vermelha: mostra status (Normal ou "ATAQUE DDoS DETECTADO!"); monitora `fsm_get_attack_active()`
4. NFC: engine loga UID da carta no monitor (copiar para `nfc_config.h`); fallback assume `CARTA_BALANCEAMENTO`; chama `threat_mitigate()` → LEDs piscam verde → tela exibe "ATAQUE MITIGADO!" → fecha em 2s
