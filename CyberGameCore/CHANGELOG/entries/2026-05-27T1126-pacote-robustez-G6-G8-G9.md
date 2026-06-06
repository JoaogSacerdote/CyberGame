---
data: 2026-05-27T11:26
tipo: feat (codigo)
escopo: components/hardware, components/engine, main
trigger: gaps identificados na auditoria de 2026-05-27, autorizados em sequencia pelo usuario
commits:
  - 8d25c48 feat(hardware): asserts nos pontos de entrada publicos dos HALs (G6)
  - 2f87793 feat(engine): subscreve engine_task ao Task Watchdog (G8)
  - af739e1 feat(main): banner de boot com versao + IDF + reset reason (G9)
---

# Pacote robustez G6+G8+G9 — asserts, watchdog, version banner

## Por que

Auditoria de 2026-05-27 contra Beningo / Elecia / ESP-IDF style guide
apontou 3 gaps de robustez tecnica de baixo risco. Usuario autorizou
pacote consolidado por valor/risco favoravel.

## O que mudou (3 commits separados)

### G6 — Design-by-Contract asserts nos HALs (commit 8d25c48)

Adiciona `assert()` nas APIs publicas dos HALs validando pre-conditions
do contrato. Em DEBUG: bug de chamador panica com file:line. Em RELEASE
(NDEBUG): zero overhead.

Distincao Beningo:
- `assert()` = violacao programatica (bug, nao deveria acontecer)
- `esp_err_t` = erro de runtime (estado nao-inicializado, condicao
  transiente)

Convertidos `if (ptr == NULL) return ESP_ERR_INVALID_ARG` para `assert()`
quando NULL e bug do chamador (ex.: `storage_hal_read_jedec_id`).
Mantido como `esp_err_t` quando representa estado runtime
(ex.: `if (s_spi == NULL) return ESP_ERR_INVALID_STATE`).

Arquivos: `display_hal.c`, `button_hal.c`, `nfc_hal.c`, `storage_hal.c`.

### G8 — Task Watchdog no engine_task (commit 2f87793)

`engine_task` agora subscreve ao Task WDT via `esp_task_wdt_add(NULL)`
no inicio e chama `esp_task_wdt_reset()` a cada iteracao do loop.

Se algum handler da FSM travar (>5s sem reset), Task WDT panica e deixa
rastro no log. Sem isso, FSM travada = sistema mudo, sem indicacao.

Confirmado em sdkconfig:
- `CONFIG_ESP_TASK_WDT_EN=y`
- `CONFIG_ESP_TASK_WDT_INIT=y` (idle tasks dos 2 cores ja monitoradas)
- `CONFIG_ESP_TASK_WDT_TIMEOUT_S=5`
- `CONFIG_BROWNOUT_DET=y, level 7` (max)
- `CONFIG_ESP_INT_WDT=y` (300ms)

`button_reader_task` NAO foi adicionada — ela bloqueia indefinidamente
em `button_hal_get_event(..., UINT32_MAX)` por design, o que disparia
WDT falso. Regra: tarefa bloqueante indefinida fica fora do WDT.

CMakeLists do engine: `esp_system` adicionado em PRIV_REQUIRES (para
`esp_task_wdt.h`).

### G9 — Banner de boot com versao + IDF + reset reason (commit af739e1)

Cria `main/version.h` com semver `0.1.0` e adiciona banner ASCII no
inicio de `app_main`:

```
======================================================
  CyberGame v0.1.0 (idf v6.0)
  reset reason: POWERON
======================================================
```

`esp_reset_reason()` mapeada via funcao local porque IDF nao expoe
`esp_reset_reason_get_name()`. Casos: POWERON, EXT, SW, PANIC, INT_WDT,
TASK_WDT, OTHER_WDT, DEEPSLEEP_WAKE, BROWNOUT, SDIO, UNKNOWN.

Sinergia com G8: se reset reason for TASK_WDT, banner sinaliza no
proximo boot.

`esp_app_desc.h` (build indicator git/hash detalhado) nao existe em IDF
v6.0 na forma esperada — adiado. Por hora versao manual e suficiente.

## Tamanho do binario

Antes (refactor board_pins): 0x8c6c0 bytes (86% livre)
Depois (G6+G8+G9):           0x8cee0 bytes (86% livre)
Delta:                       +0x820 bytes (~2 KB)

Overhead vem majoritariamente dos asserts (cada `assert()` compila como
chamada + string com filename — removido em release com `-DNDEBUG`).

## Pendencias residuais (para proxima rodada)

Ver `_AGENT/OPEN_QUESTIONS.md` — gaps G2, G3, G4, G5 (este ultimo
parcialmente coberto por docs/ai-rules/):

- **G2** input service (UI/FSM/engine ainda tocam HAL direto) — ALTO valor + risco
- **G3** callback registration nos HALs de input
- **G4** init com config table (Beningo tip #9)
- **G10** `esp_event` — decidido NAO usar, registrar em DECISION_LOG

Bloqueadores externos:
- **B1** UIDs reais das 3 cartas NFC (usuario)
- **B2** recovery USB CDC expandir PUT/GET/LIST
- **B3** arte das salas faltantes
