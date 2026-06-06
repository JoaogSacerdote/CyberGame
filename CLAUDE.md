# CyberGame — Instruções Primárias para Agente IA

> Leia este arquivo INTEIRO antes de qualquer ação.
> Depois leia `CyberGameCore/_AGENT/ENTRYPOINT.md` para detalhes do vault.

---

## O QUE É ESTE PROJETO

**CyberGame** é um console portátil de jogo educativo sobre cibersegurança,
construído em hardware próprio com ESP32-S3. O jogo ensina conceitos de
segurança digital (senhas, backup, ransomware, DDoS) através de mecânicas
de RPG top-down 2D pixel art.

**Responsável:** João Guilherme (joaog.souza2002@gmail.com / GitHub: joaogsacerdote)
**Plataforma:** ESP32-S3 + ESP-IDF 6.0 + LVGL 9.x
**Display:** LCDWIKI MSP4030/4031 4" SPI ST7796, 480×320 px

---

## HARDWARE (resumo)

- **MCU:** ESP32-S3 (sem PSRAM Octal — boot loop, DESLIGADA)
- **Display:** SPI ST7796, 480×320, backlight NPN GPIO 42
- **Entrada:** joystick analógico (ADC) + 4 botões (A/B/X/Y)
- **Storage:** microSD via SPI (SD_CS = GPIO 47)
- **LEDs:** 3x WS2812 RGB
- Pinout completo: `hardware/PINOUT.md` e `CyberGameCore/20_Hardware_HAL/`

---

## ARQUITETURA DO FIRMWARE

```
main/
  main.c              — boot, banner, engine_start()

components/
  engine/             — loop principal, init HALs, LVGL task
  hardware/           — HALs: display, button, joystick, PMU, SD
  hal_bridge/         — bridge hardware → LVGL (sem LVGL nos HALs)
  assets/             — asset_loader (cache load-once), dialog_loader
  entity/             — entity_pool, room_layout_sd, y_sort, debug_overlay
  ui/                 — screens: splash, menu, recepcao, empresa, HUD, pause
  fsm/                — FSM de estados do jogo
  gamestate/          — estado persistente (progresso, tarefas)
```

**Regra HAL:** `components/hardware/*.h` nunca inclui LVGL.
Integração LVGL fica em `hal_bridge/`.

**Regra LVGL:** diff-gate todo update + `lv_timer_create` (não task FreeRTOS)
+ nunca `ESP_LOGI` dentro de `lv_lock`.

---

## PIPELINE DE ASSETS (CRÍTICO — leia com atenção)

### Fluxo completo

```
CyberGameCore/CONSULTA/Aseprite Projeto/
  Secretaria/   ← sprites PNG individuais + posicao.txt + INTERACOES.txt
  Escritorio/   ← idem

        ↓  tools/preparar_recepcao.py  (copia + extrai frames NPC)

assets/sprites/
  recepcao/     ← PNGs individuais prontos
  empresa/      ← PNGs individuais prontos
  Sprite_PLAYER.png
  DIALOGO.txt

assets/asset_registry.json   ← fonte de verdade: nome → ID → arquivo

        ↓  tools/gen_asset_ids.py

components/assets/include/asset_ids.h   ← ASSET_REC_PISO=0, ASSET_PLAYER=28, etc.

        ↓  tools/build_sd_assets.py

sdcard/assets/
  0_0.bin ... 0_40.bin    ← blobs RGB565 prontos para o ESP32
  layout/
    recepcao.json          ← entidades com posições exatas
    empresa.json           ← idem
```

### IDs atuais (asset_registry.json)

| Range | Sala | Qtd |
|-------|------|-----|
| 0–19  | recepcao (sprites individuais) | 20 |
| 20–27 | empresa (sprites individuais) | 8 |
| 28    | player (spritesheet) | 1 |
| 29    | rec_dialog (blob de texto) | 1 |
| 30–40 | empresa continuação (paredes, servidor, NPCs, icones) | 11 |

### Regras do pipeline

- `crop=true` → recorta ao bounding box (economiza PSRAM)
- `crop=false` → preserva tamanho original (spritesheet do player, piso)
- IDs são permanentes — nunca renumerar sem atualizar JSONs + código
- Após qualquer mudança em `asset_registry.json`, rodar `gen_asset_ids.py`
- Após qualquer mudança em sprites/registry, rodar `build_sd_assets.py`

---

## SISTEMA DE ENTIDADES

Modelo **pivot bottom-center**: `(x, y)` = centro dos pés do sprite.

```
draw_x = entity.x - sprite_w / 2
draw_y = entity.y - sprite_h
```

Flags: `SOLID(1) MOVABLE(2) CARRYABLE(4) INTERACTABLE(8) TRIGGER(16) VISIBLE(32) YSORTED(64)`

**Y-sort:** sprites com menor `sort_y` desenhados primeiro (ficam "atrás").

**room_layout_sd.c** carrega `recepcao.json` / `empresa.json` do SD em runtime.
Não há tabelas de layout compiladas no firmware.

---

## POSIÇÕES DAS ENTIDADES — FONTE AUTORITATIVA

As posições X,Y nos JSONs de layout vêm de:

- `CyberGameCore/CONSULTA/Aseprite Projeto/Secretaria/posicao.txt` — recepcao
- `CyberGameCore/CONSULTA/Aseprite Projeto/Escritorio/posicao.txt` — empresa
- `CyberGameCore/CONSULTA/Aseprite Projeto/Secretaria/INTERACOES.txt` — áreas + spawns recepcao
- `CyberGameCore/CONSULTA/Aseprite Projeto/Escritorio/INTERACOES.txt` — áreas + spawns empresa

**NÃO inventar posições. Sempre usar os valores desses arquivos.**

Formato: `NOME_SPRITE | X,Y` onde (X,Y) é o pivot bottom-center no canvas 480×320.

---

## SALAS ATUAIS

### Recepção (`assets/layout/recepcao.json`)

- 13 entidades: player (spawn 161,83), recepcionista NPC (86,306), móveis
- Retorno do escritório: spawn em (466, 168) — hardcoded em `screen_recepcao.c`
- NPC usa `asset_id=14` (idle) / `asset_id=15` (dialog) — swap no tick

### Escritório/Empresa (`assets/layout/empresa.json`)

- 29 entidades: player (spawn 12,160), NPC_02 (190,195), NPC_03 (337,127)
- Móveis: mesa L grande, impressora, cafeteira, plantas, servidores, cadeiras
- Paredes individuais (PAREDE_REC_03 a 07) montadas como tiles

---

## ESTADO DO DESENVOLVIMENTO (2026-06-06)

### Concluído

- [x] Hardware: display, botões, joystick, PMU, SD card funcionando
- [x] Engine: loop LVGL, HAL bridge, boot banner
- [x] Asset pipeline: PNG → .bin via build_sd_assets.py
- [x] Entity system: pool, movimento, Y-sort, debug overlay
- [x] Room layout SD: carrega JSON do SD card em runtime
- [x] Recepcão: sprites individuais, NPC com dialog, colisão
- [x] Empresa: sprites individuais com room_layout_spawn
- [x] FSM: estados básicos (menu, gameplay, pause, game_over)

### Pendente / Próximos passos

- [ ] Calibrar caixas de colisão das entidades em jogo (especialmente empresa)
- [ ] Colisão baseada em entidades (entity_t.collision_*) integrada ao movimento do player
- [ ] Sprites escritório: revisão de Y-sort e sobreposição de objetos
- [ ] Tela de demissão (game over específico)
- [ ] Sprite do player melhorado (PROTAGONISTA_01.png novo em CONSULTA/Secretaria/)
- [ ] Sistema de tarefas completo (tarefa verde, amarela, vermelha)
- [ ] 3 LEDs WS2812 integrados ao gameplay
- [ ] NFC gating (tag NFC libera acesso)
- [ ] Sala 3+ (servidor, etc.)

---

## REGRAS DE TRABALHO

### Commits

- **SEM** trailer `Co-Authored-By: Claude` nos commits
- Mensagens em português, formato `tipo(escopo): descrição`
- Nunca `git push --force` sem confirmação explícita

### Código

- Não aplicar correções sem autorização explícita do usuário
- Uma correção por vez — nunca encadear `ESP_ERROR_CHECK` em init novo
- Não adicionar features além do solicitado
- Não criar arquivos `.md` de documentação sem ser pedido

### Vault / CyberGameCore

- `CyberGameCore/_AGENT/ENTRYPOINT.md` — ler no início de toda sessão nova
- `CyberGameCore/CHANGELOG/` — registrar ANTES/DEPOIS de toda edição não-trivial
- `CyberGameCore/CONSULTA/Aseprite Projeto/*/posicao.txt` — fonte autoritativa de posições

### Componente `simulation/`

- **NÃO TOCAR** — é código do colega de equipe

---

## COMO BUILDAR

```powershell
# Ativar ambiente IDF (PowerShell)
. "C:\Espressif\v6.0\esp-idf\export.ps1"

# Build + flash
python idf.py build flash monitor

# Gerar .bin dos assets (após mudar sprites ou registry)
python tools/gen_asset_ids.py
python tools/build_sd_assets.py
# Copiar sdcard/assets/ para o cartão SD
```

Se build falhar com erro de MinGW/paths: apagar pasta `build/` e tentar novamente.

---

## NAVEGAÇÃO RÁPIDA

| Quero saber... | Onde olhar |
|---|---|
| Estado atual do projeto | `CyberGameCore/_AGENT/CANONICAL_INDEX.md` |
| Mudanças recentes | `CyberGameCore/CHANGELOG/` |
| Decisões de design tomadas | `CyberGameCore/_AGENT/DECISION_LOG.md` |
| Questões em aberto | `CyberGameCore/_AGENT/OPEN_QUESTIONS.md` |
| Posições dos sprites | `CyberGameCore/CONSULTA/Aseprite Projeto/*/posicao.txt` |
| IDs dos assets | `assets/asset_registry.json` + `components/assets/include/asset_ids.h` |
| Pinout GPIO | `hardware/PINOUT.md` |
| Preferências do usuário | `.claude/projects/.../memory/MEMORY.md` |
