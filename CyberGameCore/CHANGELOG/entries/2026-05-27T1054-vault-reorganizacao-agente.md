---
data: 2026-05-27T10:54
tipo: refactor (vault, não código)
escopo: CyberGameCore/
trigger: usuário pediu que o vault fosse otimizado para uso por agente Claude, não para humano
---

# Vault reorganizado para uso por agente

## Por que

Usuário sinalizou que `CyberGameCore/` não é "segundo cérebro humano".
Existe para o agente. Pediu otimização para recuperação rápida de
contexto, redução de ambiguidade canonical, e separação clara entre
fonte/síntese/decisão/execução.

## O que mudou (sem tocar em código do firmware)

### Criada infraestrutura `_AGENT/`

11 arquivos novos:
- `ENTRYPOINT.md` — porta de entrada única para o agente
- `VOCAB.md` — vocabulário fechado (tipo, status, area)
- `CANONICAL_INDEX.md` — verdade vigente por área
- `MOC_hardware.md`, `MOC_firmware.md`, `MOC_fsm.md`,
  `MOC_game_design.md`, `MOC_externals.md` — 5 mapas por área
- `OPEN_QUESTIONS.md` — pendências consolidadas (síntese de `A resolver.txt`)
- `DECISION_LOG.md` — decisões fechadas (síntese de `RESPOSTAS.txt`)
- `MAINTENANCE.md` — regras que o agente segue

### Notas existentes — frontmatter padronizado

7 notas tiveram frontmatter atualizado com `tipo:`, `status:`, `area:`
seguindo o vocabulário fechado de `_AGENT/VOCAB.md`:
- `10_Arquitetura/diagramas-do-projeto.md` → `canonical`
- `10_Arquitetura/matriz-reacao-ataques.md` → `canonical`
- `10_Arquitetura/plano-implementacao-game-logic.md` → `roadmap`
- `20_Hardware_HAL/calibracao-cores-display.md` → `canonical`
- `00_Inbox/futuro-migrar-p4-JC4880P443.md` → `roadmap`
- `00_Inbox/log-sincronizacao-consulta.md` → `log`

### Notas arquivadas (movidas para 90_Historico/)

- `auditoria-hal-legada.md` → `tipo: historical`, `status: historico`
  (auditoria pré-implementação; tabela de progresso totalmente
  desatualizada — todos os HALs já estão implementados)
- `driver-st7796s.md` → `tipo: historical`, `status: obsoleto`
  (pinout HARDCODED ERRADO: cita MOSI=7 etc.; vigente é `board_pins.h`)

Ambas ganharam aviso `> [!warning]` no topo apontando para o canonical
vigente.

### Pastas deletadas

- `30_Ciberseguranca/` — vazia, falsa promessa de organização
- `40_Academico/` — idem

### Arquivos deletados

- `CONSULTA/notas_temp.txt` — o próprio arquivo se autodeclarava
  apagável após assimilação; conteúdo já estava processado no log de
  sync de 2026-05-11

### README.md raiz reescrito

Antes apontava para humanos. Agora declara explicitamente que o vault
é memória do agente e redireciona Claude para `_AGENT/ENTRYPOINT.md`.

### Memória pessoal adicionada

`~/.claude/.../memory/vault_agent_entrypoint.md` — para que Claude
encontre o entrypoint do vault em sessões futuras sem ter que descobrir.

## Pinout e código — NÃO mexido

Esta refactor é 100% no vault. Nenhum arquivo em `components/`, `main/`
ou `sdkconfig` foi tocado. Build continua verde (CyberGame.bin 0x8c6c0).

## Como reverter (se necessário)

O vault inteiro está fora do git. Para reverter, restaurar de backup
local / Obsidian Sync. Mudanças não são automáticas.

## Próximas decisões abertas (pós-vault)

Ver `_AGENT/OPEN_QUESTIONS.md` — G2 a G10 (gaps de arquitetura
identificados na auditoria de 2026-05-27 contra Beningo/Elecia/ESP-IDF
style guide) ainda pendentes de decisão do usuário.
