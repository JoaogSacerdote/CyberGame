---
tipo: agent-reference
status: vigente
audiencia: claude-code
ultima-atualizacao: 2026-05-27
---

# VOCAB — Vocabulário fechado do vault

> Toda nota neste vault DEVE usar valores deste vocabulário em seu
> frontmatter. Se precisar de termo novo, atualize este arquivo
> primeiro, com justificativa.

## Campo `tipo:` — o que esta nota É

Vocabulário FECHADO. Escolha exatamente um.

| valor | significa | onde fica |
|---|---|---|
| `agent-entrypoint` | porta de entrada para o agente | `_AGENT/` |
| `agent-reference` | referência operacional do agente (este arquivo, MAINTENANCE, VOCAB, CANONICAL_INDEX) | `_AGENT/` |
| `moc` | Map of Content — agrega links por tema | `_AGENT/MOC_*.md` |
| `canonical` | síntese vigente da verdade sobre um tema | `10_Arquitetura/`, `20_Hardware_HAL/` |
| `decision` | decisão fechada (com data, motivo, alternativas descartadas) | `_AGENT/`, `10_Arquitetura/` |
| `synthesis` | extração/condensação de uma ou mais fontes (não é canonical ainda) | `00_Inbox/` ou área temática |
| `procedure` | passo-a-passo executável (runbook, checklist) | `50_Templates/` ou área temática |
| `template` | modelo para gerar notas novas | `50_Templates/` |
| `log` | append-only (sync, sessões, observações datadas) | `00_Inbox/`, `CHANGELOG/` |
| `changelog-entry` | uma entrada de mudança de código | `CHANGELOG/entries/` |
| `roadmap` | plano de trabalho futuro | `00_Inbox/` ou `_AGENT/` |
| `source-extract` | conteúdo extraído de PDF/datasheet de CONSULTA | `CONSULTA/` ou `_AGENT/` |
| `historical` | nota que já foi canonical/synthesis mas está obsoleta; mantida para arqueologia | `90_Historico/` |
| `scratch` | rascunho temporário (deve ser promovido ou apagado em ≤30 dias) | `00_Inbox/` |

## Campo `status:` — situação atual

Vocabulário FECHADO.

| valor | significa | ação requerida |
|---|---|---|
| `vigente` | atualizado, confiável, autoritativo | usar livremente |
| `decidido` | decisão fechada que continua valendo | usar como base |
| `em-revisao` | sob escrutínio, pode mudar; **avise antes de agir** | confirmar antes de citar |
| `bloqueado` | aguarda input externo (usuário, hardware, terceiro) | identificar quem é o gating |
| `obsoleto` | foi vigente, agora é incorreto; preservado para histórico | NÃO usar para decisão atual |
| `historico` | nunca mais será atualizado; arquivado | apenas leitura curiosa |

## Campo `area:` — domínio do projeto

Vocabulário FECHADO. Use exatamente um (ou `cross-cutting` se for transversal).

| valor | escopo |
|---|---|
| `hardware` | pinout, periféricos físicos, PCB, calibração de hardware |
| `firmware` | HALs em C, drivers ESP-IDF, integração de bibliotecas |
| `fsm` | máquinas de estado de gameplay, transições, eventos |
| `game-design` | regras, balanceamento, salas, NPCs, mecânicas, scoring |
| `pedagogia` | fundamentação acadêmica, BNCC, Kolb, Papert |
| `build` | CMake, sdkconfig, ferramentas, CI |
| `infra` | recovery, USB, OTA, particionamento de flash |
| `assets` | sprites, áudio, fontes, pipeline de assets |
| `style` | coding standards, convenções, doxygen |
| `meta` | sobre o vault, sobre o agente, sobre o processo |
| `cross-cutting` | transversal a múltiplas áreas |

## Campo `canonical-for:` (opcional, só em notas `tipo: canonical`)

Lista de subtemas/conceitos para os quais esta nota é a verdade vigente.
Permite ao agente encontrar a canonical sem ler o INDEX.

Exemplo:
```yaml
canonical-for: [fsm-macro, fsm-substate-gameplay, sala-recepcao, sala-empresa]
```

## Campo `replaces:` (opcional, em notas que substituem canonical anterior)

Lista de wikilinks de notas que esta sobrescreve. A nota antiga deve ser
movida para `90_Historico/` e ter status mudado para `obsoleto`.

```yaml
replaces: [[driver-st7796s]]
```

## Campo `requires:` (opcional, em notas tipo `procedure`)

Pré-requisitos para executar o procedimento.

```yaml
requires:
  - hardware em modo bring-up
  - placa em COM17 conectada
  - sdkconfig limpo
```

## Frontmatter mínimo obrigatório

Toda nota DEVE ter, no mínimo:

```yaml
---
tipo: <um do vocab acima>
status: <um do vocab acima>
area: <um do vocab acima>
ultima-atualizacao: YYYY-MM-DD
---
```

Campos legados toleráveis (não obrigam migração imediata):
- `tags:` — manter se existir, sem valor para agente
- `projeto:` — sempre `CyberSec`, redundante
- `data:` — sinônimo de `ultima-atualizacao`
- `componente:` — específico de notas de hardware

## Quando adicionar novo termo

Se você (agente) sentir necessidade real de um termo novo:
1. Adicione ao vocabulário com justificativa breve.
2. Mencione em `MAINTENANCE.md` a primeira nota que usou.
3. Se em ≤3 notas após 1 mês: remova (era ad-hoc desnecessário).
