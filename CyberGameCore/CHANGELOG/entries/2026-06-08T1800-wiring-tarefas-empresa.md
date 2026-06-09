# 2026-06-08T1800 — Wiring das telas de tarefa no screen_empresa

**Tipo:** edit  
**Arquivos modificados:**
- `components/ui/CMakeLists.txt`
- `components/ui/screen_empresa.c`
- `components/ui/include/screen_tarefa_amarela.h` (criado — estava faltando)

## O que mudou

### Antes

`screen_empresa.c` tinha um placeholder (`s_lbl_tarefa`) que mostrava um label
estático "[TAREFA VERDE]" ao pressionar A perto do PC. Não havia suporte a
`AREA_TAREFA_AMARELA`. `screen_tarefa_verde.c` e `screen_tarefa_amarela.c`
existiam mas não estavam conectados à empresa nem no `CMakeLists.txt`.

### Depois

**`components/ui/CMakeLists.txt`**  
- Adicionados `screen_tarefa_verde.c` e `screen_tarefa_amarela.c` em SRCS.

**`components/ui/screen_empresa.c`**  
- Removidos: `s_tarefa_open`, `s_b_cache`, `s_lbl_tarefa` (placeholder).
- Guard no tick: após `screen_hud_tick()`, retorna cedo se qualquer tarefa
  estiver aberta — HUD continua avançando, movimento bloqueado.
- `fsm_set_player_at_equipment` agora cobre ambas as áreas (VD e AM).
- Prompt `[A]` aparece próximo às duas áreas de tarefa.
- A próximo de `AREA_TAREFA_VERDE` → `screen_tarefa_verde_build(on_tarefa_vd_done)`
- A próximo de `AREA_TAREFA_AMARELA` → `screen_tarefa_amarela_build(on_tarefa_am_done)`
- Callbacks `on_tarefa_vd_done` / `on_tarefa_am_done` apenas logam o resultado
  por ora (TODO: registrar no gamestate quando implementado).

**`components/ui/include/screen_tarefa_amarela.h`**  
- Criado — estava faltando da sessão anterior.

## Pendente

- `screen_tarefa_amarela_reset()` deve ser chamado ao iniciar nova partida.
  Não foi adicionado em `gamestate.c` para evitar dependência circular
  (`gamestate` ← `ui`). Deve ser chamado de quem inicia o novo jogo na camada UI.
