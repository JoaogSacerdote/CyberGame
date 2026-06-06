---
tipo: log
status: vigente
area: meta
data-inicial: 2026-05-11
ultima-atualizacao: 2026-05-11
formato: append-only (uma seção por sync; mais recente NO TOPO)
tags: [cybersec, sync, log]
---

# Log de Sincronizacoes /sync-consulta

## Sync 2026-05-11

**Arquivos processados:**
- 🆕 NEW: `notas_temp.txt` (577 B) — notas curtas do usuario sobre 2 alteracoes recentes no projeto.

**Sobreposicoes de memoria aplicadas (transistor PNP → NPN no GPIO 42):**
- `project_hardware`: tabela de pinout (linha 42) e descricao detalhada do GPIO 42 reescritas. Polaridade invertida: agora **1=LIGA, 0=CORTA**. Adicionado aviso de que `CONSULTA/display_hal.c` ainda usa logica PNP e precisa ser invertida ao adaptar.
- `project_display_pending_changes`: parte 1 (controle GPIO 42 no init) marcada como **FEITA** (usuario ja implementou em `display_hal_power_on`); restante (`display_hal_sleep`) atualizado para nova polaridade (sleep agora usa 42=0, nao 42=1). Adicionada nota sobre comentario stale na linha ~20 de `display_hal.c`.
- `CyberGameCore/20_Hardware_HAL/driver-st7796s.md`: tabela de pinout (linha 41), fluxo init (linha 74), bloco sleep (linha 96) e nota de referencia (linha 103) atualizados.
- `CyberGameCore/00_Inbox/auditoria-hal-legada.md`: linha 90 (status do display) e linhas 105-106 (referencias CONSULTA) atualizadas com polaridade NPN.

**Extensoes (sem conflito):**
- `esp_lcd_panel_mirror(s_panel, true, true)` corrigiu texto invertido — ja aplicado por usuario em `components/hardware/display_hal.c:186`. Nao requer sobreposicao de memoria (decisao puramente de codigo, nao documentada antes).
- Texto de teste no main expandido com alfabeto inteiro — irrelevante para memoria.

**Pendencias para o usuario:**
- ⚠️ Comentario em `components/hardware/display_hal.c:20` ainda diz `/* PNP high-side switch: 0 = LIGA VCC, 1 = CORTA VCC */` — esta inconsistente com o codigo (que ja foi mudado para NPN logic). Precisa decisao explicita do usuario para corrigir (regra `feedback_no_unauthorized_fixes`).
- ⚠️ Arquivo `CONSULTA/display_hal.c` continua com codigo PNP — nao tocado (regra read-only da skill). Eh apenas referencia, mas convem lembrar ao adaptar `display_hal_sleep`.

**Sem alteracoes em:** Artigo.pdf, pmu.[ch], display_hal.[ch], button_hal.[ch], joystick_hal.[ch] (9 arquivos UNCHANGED).

---

## Sync 2026-05-08 (primeira execucao real de conteudo)

**Arquivos processados (todos NEW — snapshot anterior estava vazio):**
- 🆕 NEW: `Artigo.pdf` (2.5 MB) — ja extraido em sync prepatorio anterior (`Artigo_extracted.txt`, 56 KB), nao reprocessado
- 🆕 NEW: `pmu.h` (1.2 KB) — referencia de PMU funcional anterior
- 🆕 NEW: `pmu.c` (3.8 KB) — referencia de PMU funcional anterior
- 🆕 NEW: `display_hal.h` (1.8 KB) — referencia funcional do display ST7796S
- 🆕 NEW: `display_hal.c` (5.9 KB) — idem
- 🆕 NEW: `button_hal.h` (1.0 KB) — padrao ISR+queue+xTimer (nao ha equivalente em components/hardware/ ainda)
- 🆕 NEW: `button_hal.c` (4.3 KB) — idem
- 🆕 NEW: `joystick_hal.h` (0.9 KB) — task ADC + moving average + calibracao
- 🆕 NEW: `joystick_hal.c` (4.1 KB) — idem

**Sobreposicoes de memoria aplicadas:** *(nenhuma)*

> Nenhum pinout, frequencia ou identificador da memoria atual foi sobreposto. A CONSULTA contem CODIGO LEGADO (com pinout antigo) e a memoria `project_hardware` ja reflete o **pinout definitivo confirmado em 2026-05-07**, que prevalece.

**Conflitos identificados (codigo CONSULTA usa hardware antigo — NAO sobrepor):**

| Arquivo CONSULTA | GPIO no codigo | GPIO no memory atual | Decisao |
|---|---|---|---|
| `pmu.c` | PWR=1, REC=5 | PWR=4 (sem REC) | memory atual prevalece |
| `button_hal.c` | A=11, B=12, X=13, Y=14 | (atualizado em 2026-05-08: A=11, B=12, X=13, Y=14 — agora batem com CONSULTA) | sem conflito hoje |
| `joystick_hal.c` | X=GPIO15/ADC2_CH4, Y=GPIO16/ADC2_CH5 | X=GPIO1/ADC1, Y=GPIO2/ADC1 | memory atual prevalece |
| `display_hal.c` | RST=-1 (sem reset fisico) | RST=21 | memory atual prevalece |

**Extensoes (info nova sem contradicao — registrar para uso futuro):**

- 🆕 **GPIO 42 = transistor PNP de controle do VCC do display** (CONSULTA `display_hal.c` linha 30). Uso: `gpio_set_level(42, 0)` LIGA, `gpio_set_level(42, 1)` CORTA. **AMBIGUO — requer confirmacao manual:** o hardware fisico atual (rev. 2026-05-07) inclui esse transistor? Se sim, vale adicionar ao `project_hardware` e portar para `display_hal.c` atual.
- 🆕 **Padrao de PMU com 3 modos de boot** (HIBERNACAO/OPERACIONAL/MANUTENCAO via botao REC). Atual usa apenas bool (boot-hold ok/falhou). Se tivermos um botao secundario para manutencao no futuro, vale considerar.
- 🆕 **Protocolo de sleep do display contra backpowering** (CONSULTA `display_hal_sleep`): puxa MOSI/CLK/CS/DC/RST para niveis seguros, ativa `gpio_hold_en` no dominio RTC para travar durante Deep Sleep, corta VCC pelo PNP. Atual `display_hal.c` NAO implementa sleep — eh uma extensao importante para preservar bateria.
- 🆕 **Padrao button_hal**: ISR -> `xTimerResetFromISR` (timer de debounce 50ms) -> queue de eventos. Mais robusto que polling. Vale adotar quando reconstruir input.
- 🆕 **Padrao joystick_hal**: task dedicada (core 1) + moving average de 8 amostras + calibracao automatica de centro nos primeiros 500ms + deadzone 5%. Vale portar para o ADC1 atual (apenas trocar canais e unidade).

**Ambiguos (registrar e adiar decisao):**
- ⚠️ `PMU_HANDSHAKE_TIME_MS`: atual=2000, CONSULTA=5000. Atual eh mais responsivo, CONSULTA eh mais resistente a toques acidentais. Decisao do usuario.

**Resolucoes posteriores nesta sessao:**
- ✅ **GPIO 42 (transistor PNP do VCC do display): CONFIRMADO existir no hardware atual** (usuario, 2026-05-08). Memoria `project_hardware` atualizada com a linha `42 | Display VCC enable (PNP, 0=LIGA, 1=CORTA)`. Vault `driver-st7796s.md` atualizado com sequencia de boot incluindo passo 1 (LIGA VCC + 50 ms de estabilizacao) e nova secao 4.1 (protocolo de sleep anti-backpowering).
- ⏸️ **Implementacao no `display_hal.c`: ADIADA** (usuario, 2026-05-08). Tela vai ser atacada mais para frente. Memoria nova `project_display_pending_changes` registra a pendencia para nao deixar cair no esquecimento. NAO mexer em `components/hardware/display_hal.c` para essa finalidade ate o usuario autorizar.

**Avaliacao do PMU atual vs CONSULTA (em resposta a observacao do usuario):**

O PMU atual em `components/hardware/pmu.c` ja eh evolucao da versao CONSULTA — mais limpo (esp_err_t/ESP_RETURN_ON_ERROR), monitora shutdown durante operacao via task dedicada, e nao spamma logs (apenas 1 log no init, 1 no inicio do shutdown e 1 no aviso de soltura do botao). Os logs sao todos `ESP_LOGI`/`ESP_LOGW` chamados uma unica vez por evento — nao ha loops com log periodico. Em Deep Sleep o ESP32-S3 esta literalmente desligado, entao nao ha como cuspir logs nesse estado. **O codigo atual mantem a qualidade do CONSULTA, nao precisa rollback.**

**Sem alteracoes em:** *(snapshot vazio anterior — todos sao NEW)*

---
