---
data: 2026-05-28T08:40
tipo: add (codigo)
escopo: components/ui
trigger: usuario pediu para construir o loader em runtime do sistema de layout de salas
commits:
  - (pendente)
---

# Loader em runtime do room_layout (room_layout_spawn)

## Por que

O pipeline de dados ([[2026-05-28T0820-pipeline-layout-entidades-posicoes]])
gera `room_layouts[]`, mas nada consumia em runtime. Este loader instancia as
entidades da sala no `entity_pool` a partir desses dados.

## O que mudou (aditivo — telas vivas NAO tocadas ainda)

- `components/ui/include/room_layout_loader.h` + `room_layout_loader.c`:
  `size_t room_layout_spawn(lv_obj_t *parent, const char *room_name, entity_t **out_player)`.
  Para cada room_entity_def_t: carrega o asset (asset_loader), aloca entity_t
  (entity_pool), cria lv_image filho de `parent` (se VISIBLE), preenche
  tipo/flags/colisao/tamanho/pivot, posiciona (entity_apply_lv_pos) e marca
  y_sort dirty.
- **Tolerante**: asset que falha (ex.: fundo grande sem PSRAM) -> loga e pula;
  a sala sobe com o que couber. Permite testar entidades pequenas sem PSRAM.
- **Player**: tamanho do sprite = 1 frame (PLAYER_FRAME_W/H), nao o sheet
  inteiro; lv_image com inner_align TOP_LEFT pra tela animar via offset depois.
- **from_image**: converte o offset do crop (top-left) pra pivot base-centro
  (x = off_x + w/2; y = off_y + h).
- `ui/CMakeLists.txt`: +room_layout_loader.c, +REQUIRES entity.

Build VERDE (CyberGame.bin 0xa4aa0).

## Pendente (proximo)

**Wiring nas telas** ([[task #24]]): screen_empresa/recepcao ainda criam as
entidades a mao. Trocar por `room_layout_spawn` e integrar o player (movimento/
animacao do screen_room) ao entity_t retornado. Fazer 1 sala primeiro, validar
no aparelho (EOD). A funcao existe e compila, mas ainda nao e chamada.

NOTA: firmware atual segue com a tentativa PSRAM Octal@40 + memtest off
(aguardando boot-log). Layout/loader sao independentes da PSRAM (so os fundos
grandes dependem dela).

## Links
- [[2026-05-28T0820-pipeline-layout-entidades-posicoes]]
- docs/ai-rules/07-entity-system.md
