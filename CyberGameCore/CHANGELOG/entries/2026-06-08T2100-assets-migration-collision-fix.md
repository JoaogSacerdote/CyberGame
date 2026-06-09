# 2026-06-08T2100 — Migração de imagens + colisão + bugs UI

## O que mudou

### 1. Migração de sprites (Task #1 — sessão anterior + continuação)

Todos os sprites antigos movidos para `assets/sprites/IMAGENS_ANTIGAS/recepcao/` e
`empresa/`. Novos sprites do CONSULTA copiados para `assets/sprites/recepcao/` e
`empresa/`.

**Reaproveitados de IMAGENS_ANTIGAS** (sem versão nova no CONSULTA):

| Arquivo | ID | Motivo |
|---|---|---|
| recepcao/PAREDE_REC_01.png | 11 | Sem equivalente na Secretaria |
| recepcao/PAREDE_REC_02.png | 12 | Idem |
| recepcao/NPC_01_DIALOG.png | 15 | Sem versão nova |
| recepcao/ICONE_DE_NOTIFICACAO_RECEPCIONISTA_04.png | 16 | Idem |
| recepcao/RECEPCIONISTA_OLHANDO_PARA_JOGADOR…_06.png | 17 | Idem |
| recepcao/CAIXA_DE_DIALOGO_RECEPCIONISTA_07.png | 18 | Idem |
| recepcao/CAIXA_DELIMITADORA…_08.png | 19 | Idem |
| empresa/NPC_02_DIALOG.png | 37 | Sem versão nova |
| empresa/ICONE_TAREFA_VERDE_04.png | 39 | Idem |
| empresa/ICONE_TAREFA_AMARELA_05.png | 40 | Idem |

### 2. asset_registry.json — caminhos atualizados + 4 novos IDs

Arquivos renomeados:
- ID 0: `PISO_CAMADA_00.png` → `CHAO_01.png`
- ID 4: `CADEIRA_COSTA_01_REC.png` → `CadeiraCosta.png`
- ID 5: `CADEIRA_DIREITA_01.png` → `CadeiraDireita.png`
- ID 6: `CADEIRA_FRENTE_01.png` → `CadeiraFrente.png`
- ID 14: `NPC_01_IDLE.png` → `NPC_01.png`
- ID 36: `NPC_02_IDLE.png` → `NPC_02.png`
- ID 38: `NPC_03_IDLE.png` → `NPC_03.png`

Novos IDs adicionados:
- ID 66: `emp_bebedouro` = empresa/BEBEDOURO_01.png
- ID 67: `emp_lixeira` = empresa/LIXEIRA_01.png
- ID 68: `rec_dialogo_01` = recepcao/DIALOGO_01.png
- ID 69: `rec_planta_02` = recepcao/PLANTA_VASO_02.png

`gen_asset_ids.py` rodado → 70 IDs em `asset_ids.h`.

### 3. collision_empresa.c — reescrito

**ANTES:** 31 obstáculos extraídos do tilemap antigo (PAREDES_E_OBJETOS_02), 6
gatilhos com posições hardcoded desatualizadas.

**DEPOIS:** 35 obstáculos derivados de `empresa.json` (pivot + coll_offset), 5
gatilhos de `INTERACOES.txt` (pivot bottom-center → top-left):
- RETORNO_RECEPCAO → AREA_PORTA_RECEPCAO @ (0,127) 12×64
- INTERAÇCAO_COMPUTADOR_SALA → AREA_TAREFA_VERDE @ (375,240) 16×58
- SERVIDOR_ESQUERDA/DIREITA → AREA_TAREFA_AMARELA @ (320,95) e (431,95) 32×16
- INTERACAO_NPC_SERVIDOR → AREA_INTERACAO_NPC @ (352,101) 22×26

**IMPORTÂNTE:** Spawn do player (24,165) não faz overlap com AREA_PORTA_RECEPCAO
(player hitbox [16,32]×[153,165] vs door [0,12]×[127,191]: 16 < 12? NÃO → seguro).

### 4. collision_recepcao.c — reescrito

**ANTES:** 23 obstáculos do tilemap, gatilhos corretos mas obstáculos desatualizados.

**DEPOIS:** 15 obstáculos derivados de `recepcao.json`, gatilhos inalterados
(INTERACOES.txt já estava correto).

### 5. Layout JSONs corrigidos

- `recepcao.json`: `planta_vaso_02.asset_id` 23 (emp) → 69 (rec_planta_02)
- `empresa.json`: `bebedouro_01.asset_id` 2 (rec) → 66 (emp_bebedouro)

### 6. screen_empresa.c — bug dos ícones + detecção NPC

- `s_icone_am` e `s_icone_vd` agora iniciam com `LV_OBJ_FLAG_HIDDEN`; só devem
  ser mostrados quando a tarefa ficar ativa (TODO: conectar ao gamestate).
- `near_npc`: substituído `AREA_INTERACAO_NPC_TI_BAIXO` → `AREA_INTERACAO_NPC`
  para alinhar com o novo collision file.

### 7. screen_recepcao.c — bug cache de botão

Adicionado `s_a_cache = button_hal_peek(BTN_A)` e `s_b_cache = button_hal_peek(BTN_B)`
no `screen_recepcao_build()` para evitar edge fantasma no primeiro tick após rebuild.
(Empresa já fazia isso; recepcao estava inconsistente.)

### 8. SD card regenerado

`build_sd_assets.py` rodado → 70 binários + 2 JSONs de layout em `sdcard/assets/`.

## ANTES

- Sprites antigos em uso (ex: tilemap PAREDES_E_OBJETOS_02)
- Colisões do escritório derivadas de tilemap antigo (31 obstáculos errados)
- Ícones verde/amarelo visíveis desde o início do jogo
- Bug potencial de edge fantasma na re-interação com NPC da recepção

## DEPOIS

- Todos os sprites atualizados para as versões novas do CONSULTA
- Colisões derivadas diretamente dos JSONs de layout (fonte autoritativa)
- Ícones de tarefa ocultos por padrão
- Button cache inicializado corretamente em ambas as telas de sala
