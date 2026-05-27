# `docs/ai-rules/` — regras do projeto para humanos e agentes

> Convenções versionadas no repositório. **Leitura obrigatória** antes de
> abrir PR ou refatorar componentes deste projeto. Aplica-se igualmente
> a desenvolvedores humanos e a agentes de IA (Claude Code, etc.).

## Por que existe

Decisões arquiteturais e convenções de estilo costumam viver em
conversas de Slack, comentários de PR, ou memórias privadas de um único
dev. Quando isso acontece:

- novos colaboradores re-debatem as mesmas decisões;
- agentes de IA propõem soluções inconsistentes com o resto do código;
- código diverge silenciosamente do padrão pretendido.

Esta pasta consolida as **decisões fechadas** sobre como pensar e
escrever código neste projeto. Se você quer mudar uma regra, abra PR
**deste arquivo** primeiro.

## Arquivos

### Firmware (01-06)

| # | Arquivo | Cobre |
|---|---|---|
| 01 | [`01-architecture.md`](01-architecture.md) | Camadas (hardware → hal_bridge → engine/ui → main), regras de dependência |
| 02 | [`02-fsm-pattern.md`](02-fsm-pattern.md) | Como modelar máquinas de estado |
| 03 | [`03-event-model.md`](03-event-model.md) | Eventos via queue própria (e por que não `esp_event`) |
| 04 | [`04-hal-contract.md`](04-hal-contract.md) | Interface entre firmware e hardware (`components/hardware/`) |
| 05 | [`05-freertos-rules.md`](05-freertos-rules.md) | Tasks, filas, notificações, ISR, watchdog |
| 06 | [`06-coding-standard.md`](06-coding-standard.md) | Convenções alinhadas ao ESP-IDF style guide oficial |

### Gameplay (07-09)

| # | Arquivo | Cobre |
|---|---|---|
| 07 | [`07-entity-system.md`](07-entity-system.md) | struct Entity (wrapper de `lv_obj_t`), pool estático, pivot bottom-center, flags |
| 08 | [`08-y-sort-and-collision.md`](08-y-sort-and-collision.md) | Insertion sort, AABB com separação X/Y, camadas, tilemap, debug visual |
| 09 | [`09-asset-pipeline.md`](09-asset-pipeline.md) | PNG → bin RGB565 → NAND; mapas Tiled JSON → arrays C pré-compilados |

## Ordem de leitura

Para entender o **panorama** do projeto: ler na ordem 01 → 02 → 03 → 04 → 05 → 06.

Para resolver uma **dúvida pontual**, vá direto ao arquivo:
- "onde colocar este código?" → 01
- "como representar este novo estado?" → 02
- "como sinalizar evento entre tasks?" → 03
- "como expor este periférico?" → 04
- "que prioridade dar à task?" → 05
- "como nomear esta função?" → 06
- "como representar player, NPC ou móvel?" → 07
- "como funciona Y-sort e colisão?" → 08
- "como sprites e mapas chegam ao firmware?" → 09

## Hierarquia de autoridade

Quando regras conflitarem:

1. **ESP-IDF style guide oficial** (https://docs.espressif.com/projects/esp-idf/en/stable/esp32/contribute/style-guide.html) — base de tudo
2. **Este conjunto de arquivos** — extensões e decisões específicas do CyberGame
3. **Código existente** — exemplos vivos do padrão pretendido
4. Preferência pessoal — irrelevante

Se você (humano ou agente) achar que uma regra está errada ou
desatualizada, **edite o arquivo** com justificativa em vez de
ignorar silenciosamente.

## Para agentes de IA

Há uma camada adicional de regras em `CyberGameCore/_AGENT/` (vault
privado, fora do git). Quando trabalhando neste repositório, **siga
estas regras de `docs/ai-rules/`** — elas são autoritativas para o
**código**. O vault `_AGENT/` é autoritativo para **navegação** no
conhecimento acumulado do projeto.

## Status e versionamento

Estes arquivos são vivos. Cada mudança vai por PR normal. Não há
versionamento separado — `git log docs/ai-rules/` é o histórico.
