---
tipo: agent-entrypoint
status: vigente
audiencia: claude-code
ultima-atualizacao: 2026-05-27
---

# ENTRYPOINT — Como começar qualquer sessão neste vault

> Este arquivo é a primeira coisa que você (Claude Code) lê neste vault.
> Ele não é para humanos. Não tente ser bonito.

## 0. Premissa

`CyberGameCore/` é a **memória de longo prazo** do agente para o projeto
**CyberGame** (console portátil ESP32-S3 de edutainment em ciberseguranca).
O usuário quase não lê estes arquivos. Eles existem para você.

Memória de sessão (`~/.claude/.../memory/MEMORY.md` + arquivos `.md` ao
lado) é COMPLEMENTAR — focada em preferências de usuário e estado de
sessão. Este vault é focado em CONHECIMENTO DO PROJETO.

## 1. Hierarquia de verdade (quando fontes divergirem)

Ordem de autoridade, do mais autoritativo para o menos:

1. **Código no repositório** (`components/`, `main/`, `sdkconfig`) — fonte
   de verdade absoluta sobre o que o firmware faz hoje.
2. **`CHANGELOG/entries/`** — registro datado de mudanças, com "antes/depois".
3. **`_AGENT/CANONICAL_INDEX.md`** — aponta a nota canônica vigente em
   cada área.
4. **Notas com `tipo: canonical` + `status: vigente`** — síntese ativa.
5. **`CONSULTA/RESPOSTAS.txt`** + **`CONSULTA/A resolver.txt`** — decisões
   e dúvidas brutas do usuário. Confiáveis para decisões de design, não
   para estado de implementação.
6. **`CONSULTA/Artigo.pdf`** — fundamentação acadêmica imutável.
7. **Memórias em `~/.claude/.../memory/`** — contexto rápido, mas pode
   estar stale. Sempre validar contra código + CHANGELOG antes de agir.
8. **Notas com `status: obsoleto` ou em `90_Historico/`** — não confiar
   para estado vigente. Apenas contexto histórico.

**Regra de ouro**: se memória ou nota canonical contradiz o código,
**o código vence**. Atualize a memória/nota.

## 2. Rotas por intenção

### Quero entender o estado atual de uma área
1. Abra `_AGENT/CANONICAL_INDEX.md`.
2. Vá direto à área (hardware, firmware, fsm, game-design, build, infra).
3. Siga a nota canônica indicada.

### Quero saber o que mudou recentemente
1. `CHANGELOG/INDEX.md` — ordem cronológica reversa.
2. Para detalhe de uma mudança: `CHANGELOG/entries/<slug>.md`.

### Vou fazer mudança não-trivial em código
1. **ANTES** de editar: criar entrada em `CHANGELOG/entries/` (regra do `CHANGELOG/README.md`).
2. Verificar canonical da área para não regressar decisão prévia.
3. Após mudar: atualizar canonical se afetou contrato/estado.

### Quero saber o que está em aberto
1. `_AGENT/OPEN_QUESTIONS.md` — síntese atualizada de bloqueios e dúvidas.
2. `CONSULTA/A resolver.txt` — versão raw, mais detalhada.

### Quero entender uma decisão tomada
1. `_AGENT/DECISION_LOG.md` — resumo de decisões fechadas + onde foram tomadas.
2. `CONSULTA/RESPOSTAS.txt` — diálogo bruto que originou.

### Quero criar uma nota nova
1. Ler `_AGENT/MAINTENANCE.md` antes — define onde colocar, qual `tipo:`, qual `status:`.
2. Usar vocabulário fechado de `_AGENT/VOCAB.md`.

## 3. Mapa físico do vault

```
CyberGameCore/
├── _AGENT/              ★ memória operacional do agente (ESTE espaço)
├── 00_Inbox/            scratch ativo, log de sync
├── 10_Arquitetura/      canonical de FSM, matriz de ataques, plano MVP
├── 20_Hardware_HAL/     canonical de drivers (display, calibração)
├── 50_Templates/        templates para criar notas novas
├── 90_Historico/        notas que viraram obsoletas mas preservadas
├── CHANGELOG/           ★ timeline canônica de mudanças no código
├── CONSULTA/            ★ fontes externas — read-mostly
│   ├── *.pdf            literatura (Artigo, Beningo, Elecia, ESP-IDF)
│   ├── *.txt            scratch do usuário (RESPOSTAS, A resolver)
│   ├── *.md             extratos consolidados
│   └── IGNORAR/         assets brutos — ignore
└── .obsidian, .sync-state    config — não tocar
```

Pastas `30_Ciberseguranca/` e `40_Academico/` **foram removidas** em
2026-05-27 por estarem vazias e induzirem busca em vão.

## 4. MOCs por área crítica

Mapas de conteúdo (point-of-truth para navegar por tema):

- [[MOC_hardware]] — pinout, periféricos, calibração, board_pins
- [[MOC_firmware]] — HALs, hal_bridge, engine, recovery, asset_store
- [[MOC_fsm]] — máquina de estados, sub-states, eventos
- [[MOC_game_design]] — matriz ataques, salas, NPCs, scoring, decisões MVP
- [[MOC_externals]] — PDFs em CONSULTA, onde está cada referência

## 5. Convenções obrigatórias

- **Frontmatter obrigatório**: `tipo`, `status`, `area`, `data` (ver `VOCAB.md`).
- **Wikilinks sempre que possível**: `[[nome-da-nota]]` (Obsidian resolve por nome único — mover entre pastas preserva o link).
- **Codepaths como `components/hardware/button_hal.c:13`**.
- **Não duplicar conteúdo entre notas**: link em vez de copiar.
- **Nunca deletar nota com histórico relevante** — mover para `90_Historico/` com `status: historico`.
- **Updates de canonical viram entrada de CHANGELOG** quando refletem mudança no código.

## 6. Anti-padrões a evitar

❌ Criar nota nova sem `tipo:` no frontmatter.
❌ Marcar status novo fora do vocabulário fechado.
❌ Copy-paste de seção entre notas (sempre link).
❌ Inventar wikilink para nota que não existe sem criar a nota (gera link morto).
❌ Tocar em `CONSULTA/Artigo.pdf` ou outros PDFs (são source imutável).
❌ Confiar em memória sem validar contra código quando há divergência.

## 7. Backstop: regras meta do projeto

Sempre ativas, vêm da memória do usuário:

- `[[hal_boundary_contract]]` — `components/hardware/*.h` não fala LVGL.
- `[[feedback_no_unauthorized_fixes]]` — não aplicar correção sem autorização explícita.
- `[[feedback_no_coauthor_in_commits]]` — sem trailer `Co-Authored-By:`.
- `[[feedback_lvgl_diff_gating]]` — LVGL: diff-gate + `lv_timer_create` + nunca `ESP_LOGI` dentro de `lv_lock`.
- `[[skill_changelog_system]]` — registrar antes/depois de toda mudança não-trivial.
- `[[project_vault_obsidian]]` — `CyberGameCore/` é confidencial; **nunca commitar para git**.
