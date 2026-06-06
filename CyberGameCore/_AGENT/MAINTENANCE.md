---
tipo: agent-reference
status: vigente
area: meta
ultima-atualizacao: 2026-05-27
audiencia: claude-code (eu mesmo, em sessões futuras)
---

# MAINTENANCE — Regras que eu sigo neste vault

> Estas regras existem para mim. Se eu as quebrar, o vault degrada.
> Se uma regra atrapalhar trabalho real, eu **atualizo a regra** em vez
> de violá-la silenciosamente.

## R1. Toda nota nova tem frontmatter mínimo

```yaml
---
tipo: <ver VOCAB.md>
status: <ver VOCAB.md>
area: <ver VOCAB.md>
ultima-atualizacao: YYYY-MM-DD
---
```

Sem isso, eu (em sessão futura) não consigo classificar. Não pular.

## R2. Onde cada `tipo:` mora

| tipo | pasta |
|---|---|
| `agent-*`, `moc` | `_AGENT/` |
| `canonical` | `10_Arquitetura/` ou `20_Hardware_HAL/` (por área) |
| `decision` | `_AGENT/DECISION_LOG.md` (sumário) + `10_Arquitetura/` (detalhe se grande) |
| `synthesis` | área temática (10/20) ou `00_Inbox/` se ainda processando |
| `procedure`, `template` | `50_Templates/` |
| `log` | `00_Inbox/` ou `CHANGELOG/` (se de código) |
| `changelog-entry` | `CHANGELOG/entries/` |
| `roadmap` | `00_Inbox/` (até ser ativado) |
| `source-extract` | `CONSULTA/` (ou `_AGENT/` se for síntese leve para meu uso) |
| `historical` | `90_Historico/` |
| `scratch` | `00_Inbox/` (apagar/promover em ≤30 dias) |

## R3. Quando promover, sobrescrever, ou aposentar uma canonical

### Promover (`synthesis` → `canonical`)
- Quando a síntese passa a representar a verdade vigente em um tema.
- Mudar `tipo: synthesis` → `tipo: canonical`.
- Adicionar entrada em `_AGENT/CANONICAL_INDEX.md`.
- Adicionar `canonical-for: [...]` no frontmatter.

### Sobrescrever (canonical A → canonical B)
- A nova canonical declara `replaces: [[A]]` no frontmatter.
- A antiga: mover para `90_Historico/`, mudar `status: obsoleto`.
- Atualizar linha em `CANONICAL_INDEX.md`.
- Se a substituição é por mudança de código: criar entrada em `CHANGELOG/entries/`.

### Aposentar (canonical → historical)
- Quando a verdade vigente passou a ser outro lugar (ex.: foi pro código).
- Mover para `90_Historico/`, `status: historico`.
- Remover linha de `CANONICAL_INDEX.md` (ou apontar para a nova fonte).
- **Não deletar** — preservar para arqueologia.

## R4. Evitar duplicação de conteúdo

**Antes de criar nota nova**, perguntar:
1. Já existe nota cobrindo esse tema?
2. Se sim — eu deveria expandir a existente, ou esta nota nova é
   genuinamente diferente?

Se for expandir: editar a nota existente, atualizar `ultima-atualizacao`.
Se for nova: usar wikilink para a relacionada em vez de copiar conteúdo.

## R5. Wikilinks: prefira existentes, marque inexistentes

- `[[nome-existente]]` — Obsidian resolve sem path. Move-friendly.
- `[[nome-inexistente]]` — link morto. Se for promessa de nota futura,
  marcar com comentário: `[[nome-inexistente]] (a criar)`.

## R6. Quando atualizar `_AGENT/`

| evento | atualizar |
|---|---|
| Nova canonical em área existente | `CANONICAL_INDEX.md` + MOC dessa área |
| Nova área temática surgindo | criar `MOC_<area>.md` + adicionar entrada em `VOCAB.md` (campo `area:`) |
| Decisão importante tomada | adicionar entry em `DECISION_LOG.md` |
| Item de pendência resolvido | marcar `[x]` em `OPEN_QUESTIONS.md` (e em `CONSULTA/A resolver.txt`) |
| Regra deste vault muda | atualizar `MAINTENANCE.md` (este arquivo) |
| Vocabulário expande | atualizar `VOCAB.md` |

## R7. Quando NÃO criar nota

- Conteúdo que cabe em comentário inline no código.
- Resposta única que o usuário não vai reler.
- Anti-padrão "diário de bordo" sem decisão (usar CHANGELOG/entries em vez).
- Cópia de algo que já está em CONSULTA/ ou memória.

## R8. Tratamento de CONSULTA/

Read-mostly. Não editar:
- `Artigo.pdf` (imutável)
- PDFs de literatura (Beningo, Elecia, INCB, MSP4030 datasheet)
- `JC4880P443C_I_W/` (docs externas)
- `IGNORAR/` (assets brutos)

OK editar (e devo manter atualizados):
- `RESPOSTAS.txt` — só quando usuário responde nova decisão
- `A resolver.txt` — marcar `[x]` ao resolver itens
- `notas_temp.txt` — apagar após assimilar (ver R10)
- `consulta_esquema_eletrico.md`, `lista-de-imagens-c2-personagem-sala2.md`
  — manter como referência consolidada

## R9. Pastas vazias = falsas promessas

Nunca deixar pasta vazia "para o futuro" (ex.: as deletadas
`30_Ciberseguranca/`, `40_Academico/`). Se categoria não tem conteúdo,
não existe.

## R10. Notas marcadas como temporárias

Se uma nota se autodeclara "apagar após assimilar" (ex.:
`CONSULTA/notas_temp.txt`):
1. Extrair conteúdo relevante para nota canonical / memória / decision log.
2. Deletar o arquivo temporário.
3. NÃO deixar lá "por garantia" — vira ruído.

## R11. Detecção de stale (verdade desatualizada)

Sinais de que uma nota canonical está stale:
- Pinout citado não bate com `board_pins.h`.
- Componente "EM IMPLEMENTAÇÃO" há mais de 30 dias.
- Status `refatorando` há mais de 30 dias.
- Wikilinks órfãos que nunca foram preenchidos.

Ação: mudar `status:` para `em-revisao`. Em ≤1 sessão: atualizar ou
mover para `90_Historico/`.

## R12. Auditoria periódica

Quando? Quando o usuário pedir, OU quando eu notar que ≥3 notas estão
stale em uma área.

O que fazer:
1. Reler `CANONICAL_INDEX.md`.
2. Para cada canonical: validar contra código + CHANGELOG recente.
3. Sinalizar discrepâncias em `OPEN_QUESTIONS.md`.
4. Atualizar `ultima-atualizacao` das notas validadas.

## R13. Anti-prosa

Notas devem ser densas. Listas, tabelas, ponteiros. Não escrever parágrafos
contemplativos. Se eu (em sessão futura) só vou skim este arquivo, ele
precisa ser skimável.

## R14. Linking forte sobre folders profundos

Preferir hierarquia rasa + wikilinks fortes sobre subpastas aninhadas.
Vault inteiro vive em ≤2 níveis de profundidade.

## R15. Backup nunca formal — git é o "backup"

`CyberGameCore/` é confidencial, **NÃO** vai para git (memória
`[[project_vault_obsidian]]`). Backup do vault é responsabilidade do
usuário (Obsidian Sync, cópia local). Eu não me preocupo com isso.

## R16. Não tocar em `.obsidian/` ou `.sync-state/`

Config do Obsidian e estado interno do skill `/sync-consulta`. Read-only
do meu ponto de vista.
