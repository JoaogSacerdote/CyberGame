# Instrucoes_Iniciais — Handoff para nova sessão de IA

> Leia este arquivo PRIMEIRO, antes de qualquer outro.
> Depois leia `CLAUDE.md` (regras do projeto) e `CyberGameCore/_AGENT/ENTRYPOINT.md` (vault).

---

## 1. O QUE É ESTE PROJETO

Console portátil educativo de cibersegurança construído em hardware próprio com
ESP32-S3. RPG top-down 2D pixel art. O firmware roda em ESP-IDF 6.0 + LVGL 9.x
em display SPI ST7796 480×320. O código-fonte está neste repositório.

**Responsável:** João Guilherme — `joaog.souza2002@gmail.com` / GitHub: `joaogsacerdote`

---

## 2. SETUP — O QUE INSTALAR ANTES DE TRABALHAR

### 2.1 Toolchain ESP-IDF

Já instalado em `C:\esp\v6.0\esp-idf\` (Windows). Para ativar no terminal:

```powershell
. "C:\Espressif\v6.0\esp-idf\export.ps1"
python idf.py build flash monitor
```

Se build falhar com erro de MinGW/paths: apague a pasta `build/` e tente novamente.

### 2.2 Python para o pipeline de assets

Requer PIL/Pillow: `pip install pillow`. Scripts em `tools/`.

### 2.3 Obsidian (vault do projeto)

O vault **`CyberGameCore/`** fica na raiz do repositório. Para abrir no Obsidian:
- Abra o Obsidian → "Open folder as vault" → selecione `CyberGameCore/`
- Plugins usados: apenas core (sem community plugins obrigatórios)
- O vault é a MEMÓRIA LONGA do projeto — notas de arquitetura, decisões, CHANGELOG

**IMPORTANTE:** `CyberGameCore/` agora está rastreada no git (commitada em 2026-06-06).
A nota `project_vault_obsidian.md` na memória do Claude ainda diz "nunca commitar"
— essa informação está desatualizada. A pasta JÁ É rastreada. Binários pesados
continuam excluídos via `.gitignore` cirúrgico.

### 2.4 SD Card

O jogo carrega assets do cartão microSD montado em `/sd`. Após qualquer mudança
nos sprites ou no `asset_registry.json`, o fluxo obrigatório é:

```powershell
python tools/gen_asset_ids.py          # regenera asset_ids.h
python tools/build_sd_assets.py        # gera sdcard/assets/*.bin
# Copie manualmente sdcard/assets/ → /assets/ na raiz do SD card
```

O SD card NÃO é atualizado automaticamente — é cópia física manual.

---

## 3. MAPA DO REPOSITÓRIO

```
main/                   boot, engine_start()
components/
  engine/               loop principal, init HALs, LVGL task
  hardware/             HALs: display, button, joystick, PMU, SD card
  hal_bridge/           bridge hardware → LVGL (HALs não incluem LVGL)
  assets/               asset_loader (cache load-once na PSRAM)
  entity/               entity_pool, room_layout_sd, y_sort, debug_overlay
  ui/                   screens: splash, menu, recepcao, empresa, HUD, pause
  fsm/                  FSM de estados (menu, gameplay, pause, game_over)
  gamestate/            estado persistente (progresso, tarefas)
assets/
  asset_registry.json   FONTE DE VERDADE dos assets (nome → ID → arquivo)
  sprites/              PNGs fonte (recepcao/ e empresa/)
  layout/               JSONs de layout das salas (recepcao.json, empresa.json)
sdcard/assets/          .bin gerados (NÃO commitar — excluídos no .gitignore)
tools/                  scripts Python do pipeline
CyberGameCore/          vault Obsidian (memória do projeto)
CLAUDE.md               regras primárias para o agente IA (LEIA ANTES DE AGIR)
```

---

## 4. ESTADO ATUAL DO PROJETO (2026-06-06)

### O que está funcionando

- Hardware: display, botões, joystick, PMU, SD card
- Engine: loop LVGL, HAL bridge, boot banner
- Asset pipeline: PNG → .bin via `build_sd_assets.py`
- Entity system: pool, movimento, Y-sort, debug overlay
- Room layout SD: carrega JSON do SD em runtime
- Recepcão: sprites individuais, NPC com dialog, colisão
- Empresa: sprites individuais com room_layout_spawn
- FSM: estados básicos (menu, gameplay, pause, game_over)

### Bugs recentes CORRIGIDOS nesta sessão (2026-06-06)

**Bug 1 — Furniture invisível (root cause: dangling pointer)**
- `room_layout_loader.c` chamava `lv_image_set_src(lv_obj, &la.dsc)` onde `la` é
  variável local de pilha. Quando a função retorna, LVGL fica com ponteiro inválido.
- FIX: adicionado `asset_loader_get_dsc(type, id)` em `asset_loader.c/.h` que retorna
  ponteiro persistente direto do cache. `room_layout_loader.c` agora usa esse ponteiro.
- RESULTADO: furniture deve aparecer normalmente.

**Bug 2 — NPC virado para a direita (frame errado)**
- `preparar_recepcao.py` extraía `(col=1, row=2)` do spritesheet do NPC.
  No formato LPC (Liberated Pixel Cup): `row=2 = direita`, `row=3 = frente (front-facing)`.
- FIX: extraindo com `(col=1, row=3)` para NPC_01_IDLE, NPC_01_DIALOG, NPC_02_IDLE,
  NPC_02_DIALOG, NPC_03_IDLE.
- ATENÇÃO: se o spritesheet NÃO for LPC padrão, o frame ainda pode estar errado.
  O usuário deve confirmar visualmente. Se errado, ajuste os números de `(col, row)`
  em `tools/preparar_recepcao.py` e re-extraia NPC_02/03 via Python inline.

**Bug 3 — cJSON não encontrado no build**
- `entity/CMakeLists.txt` usava `"json"` que não existe no ESP-IDF 6.0.
- FIX: `"espressif__cjson"` + `entity/idf_component.yml` com dependência declarada.
  `managed_components/espressif__cjson/` foi extraído localmente do cache do IDF.
  `.component_hash` foi criado manualmente com o hash do `dependencies.lock`.

### Erros ainda pendentes / não totalmente resolvidos

- O usuário reportou "alguns erros" mas não detalhou. Provavelmente:
  1. NPC ainda com frame errado (ver Bug 2 acima — pode precisar de ajuste fino)
  2. Collision boxes da empresa.json com valores estimados (nunca calibrados em jogo)
  3. Posições de alguns sprites podem estar ligeiramente deslocadas pelo cálculo
     `pivot_x - cropped_w/2` que ignora o crop offset (`la.off_x/off_y`)

---

## 5. PIPELINE DE ASSETS — FLUXO COMPLETO

```
CyberGameCore/CONSULTA/Aseprite Projeto/
  Secretaria/   ← sprites PNG + posicao.txt + INTERACOES.txt (recepcao)
  Escritorio/   ← idem (empresa)

        ↓  tools/preparar_recepcao.py  (copia + extrai frames NPC)

assets/sprites/
  recepcao/     ← PNGs individuais prontos
  empresa/      ← PNGs individuais prontos

assets/asset_registry.json   ← fonte de verdade: nome → ID → arquivo

        ↓  tools/gen_asset_ids.py

components/assets/include/asset_ids.h   ← ASSET_REC_PISO=0, ASSET_PLAYER=28, etc.

        ↓  tools/build_sd_assets.py

sdcard/assets/
  0_0.bin ... 0_40.bin    ← blobs RGB565 para o ESP32
  layout/
    recepcao.json
    empresa.json

        ↓  cópia manual para o cartão SD
```

**IDs dos assets (asset_registry.json):**
- 0–13: recepcao sprites (piso, mobiliário, NPC frames)
- 14–19: recepcao overlays (NPC_01_IDLE/DIALOG, ícone, caixas de diálogo)
- 20–27: empresa sprites (piso, mobiliário)
- 28: player spritesheet (crop=false)
- 29: rec_dialog (blob de texto)
- 30–40: empresa continuação (paredes, servidor, NPCs, ícones)

**Regra crítica:** `crop=true` recorta ao bounding box dos pixels não-transparentes.
`crop=false` preserva tamanho original. O player e o piso usam `crop=false`.

---

## 6. SISTEMA DE ENTIDADES — RESUMO

**Pivot bottom-center:** `(x, y)` = centro dos pés do sprite.

```c
draw_x = entity.x - sprite_w / 2
draw_y = entity.y - sprite_h
```

**Fonte das posições:** SEMPRE usar os valores de:
- `CyberGameCore/CONSULTA/Aseprite Projeto/Secretaria/posicao.txt` (recepcao)
- `CyberGameCore/CONSULTA/Aseprite Projeto/Escritorio/posicao.txt` (empresa)
- `*/INTERACOES.txt` — áreas de colisão e spawns

NUNCA inventar posições. Esses arquivos são a única fonte autoritativa.

**Y-sort:** sprites com menor `sort_y` desenhados primeiro (ficam "atrás").
`y_sort_run()` é chamado pelo engine a cada frame via `entity_render_sync()`.

**room_layout_spawn(parent, "nome", &player_out):** carrega JSON do SD,
instancia entities no pool, cria lv_objs. Retorna o player via out_player.

---

## 7. REGRAS OBRIGATÓRIAS DE TRABALHO

Estas regras NÃO podem ser ignoradas:

1. **Sem `Co-Authored-By: Claude`** nos commits — nunca.
2. **Não aplicar correções sem autorização explícita** do usuário. Uma por vez.
3. **`components/hardware/*.h` nunca incluem LVGL** — HAL boundary contract.
4. **LVGL:** diff-gate todo update + `lv_timer_create` (não task FreeRTOS)
   + nunca `ESP_LOGI` dentro de `lv_lock`.
5. **`simulation/`** — NÃO TOCAR. É código do colega de equipe.
6. **Antes de qualquer edição não-trivial:** criar entrada em
   `CyberGameCore/CHANGELOG/entries/` (ver `CHANGELOG/README.md`).
7. **PSRAM Octal@80MHz** está DESLIGADA (causa boot loop). Não religar sem
   testar 40MHz/Quad primeiro.
8. **Mensagens de commit em português**, formato `tipo(escopo): descrição`.

---

## 8. ONDE BUSCAR CONTEXTO ADICIONAL

| O que quero saber | Onde olhar |
|---|---|
| Regras do projeto para o agente | `CLAUDE.md` (raiz do repo) |
| Como navegar o vault | `CyberGameCore/_AGENT/ENTRYPOINT.md` |
| Estado canônico de cada área | `CyberGameCore/_AGENT/CANONICAL_INDEX.md` |
| Decisões de design tomadas | `CyberGameCore/_AGENT/DECISION_LOG.md` |
| Pendências em aberto | `CyberGameCore/_AGENT/OPEN_QUESTIONS.md` |
| Mudanças recentes no código | `CyberGameCore/CHANGELOG/` |
| Posições dos sprites | `CyberGameCore/CONSULTA/Aseprite Projeto/*/posicao.txt` |
| IDs dos assets | `assets/asset_registry.json` + `asset_ids.h` |
| Pinout GPIO | `hardware/PINOUT.md` |
| Preferências do usuário | `C:\Users\JGril0\.claude\projects\C--Users-JGril0-Desktop-CyberGame\memory\MEMORY.md` |

---

## 9. PROXIMOS PASSOS SUGERIDOS

Prioridade sugerida para a próxima sessão:

1. **Confirmar se furniture está aparecendo** após fix do dangling pointer.
   Se sim, o bug foi resolvido. Se não, verificar logs do ESP32 — buscar
   `ROOM_LOADER: 'nome_entidade' asset falhou` para identificar qual asset.

2. **Confirmar direção do NPC** — se `row=3` ainda está errado, ajustar
   `(col, row)` em `preparar_recepcao.py` e re-extrair os frames NPC.

3. **Calibrar collision boxes** de `assets/layout/empresa.json` — valores
   estimados, precisam de teste em jogo com debug overlay ativado.

4. **Implementar colisão baseada em entidades** — atualmente a colisão usa
   tabelas hardcoded em `collision_data.h`. O plano é migrar para as
   `collision_*` fields nas entities do JSON.

5. **Sistema de tarefas** — tarefa verde (troca de senha) precisa de tela
   completa com mecânica de seleção.

---

## 10. COMO CONFIRMAR QUE O AMBIENTE ESTÁ OK

```powershell
# 1. Verificar que o build compila sem erros
. "C:\Espressif\v6.0\esp-idf\export.ps1"
python idf.py build

# 2. Verificar que os assets do SD estão gerados
ls sdcard/assets/*.bin | Measure-Object | Select-Object Count
# Deve retornar 41

# 3. Verificar asset_ids.h está atualizado
# (deve ter ASSET_EMP_ICONE_VERDE=39, ASSET_EMP_ICONE_AMARELO=40 no fim)
Select-String "ASSET_EMP_ICONE" components/assets/include/asset_ids.h
```

Se o build falhar com erro sobre `espressif__cjson`:
```powershell
python idf.py update-dependencies   # baixa cjson do registry
python idf.py build                 # tenta novamente
```

---

*Documento criado em 2026-06-06 para handoff de sessão.*
*Última sessão: corrigidos dangling pointer (furniture invisível), frame NPC (row=3),*
*cJSON (espressif__cjson), pipeline de assets, CLAUDE.md criado, vault commitado.*
