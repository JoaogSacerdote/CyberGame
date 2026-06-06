---
date: 2026-05-26 18:30
type: edit
files:
  - sdkconfig
session: bring-up placa nova
---

# Desligamento de PSRAM — resolveu o boot loop

## Motivo
Placa nova entrava em boot loop infinito travando em `cpu_start: Multicore app`, RTC WDT estouro. Sintoma persistiu mesmo com placa NUA (zero periféricos), eliminando hipóteses de hardware externo. Usuário gravou sketch Arduino simples → boot OK → confirma que o módulo ESP32-S3 está fisicamente íntegro. Logo o problema é do nosso build ESP-IDF, não do hardware.

Hipótese: Arduino IDE por padrão não habilita PSRAM. Nosso `sdkconfig` habilita PSRAM Octal @80MHz. Suspeita: módulo da placa nova detecta PSRAM Octal genérica no tuning mas falha na inicialização real da CPU1 que tenta usar a PSRAM, RTC WDT estoura.

## Antes
```
# sdkconfig linha 1820
CONFIG_SPIRAM=y

# (configurações dependentes)
CONFIG_SPIRAM_MODE_OCT=y
CONFIG_SPIRAM_SPEED_80M=y
CONFIG_SPIRAM_BOOT_HW_INIT=y
CONFIG_SPIRAM_BOOT_INIT=y
```

## Depois
```
# sdkconfig linha 1820
# CONFIG_SPIRAM is not set
```

## Resultado
✅ **Hipótese confirmada.** Build passou (após fixes de inconsistências do HEAD — ver entrada T1845). Boot avançou pela primeira vez além do `cpu_start: Multicore app`. Firmware rodou até crashar no `nfc_hal_init` (esperado — NFC não conectado fisicamente).

**Efeito colateral pendente:** PSRAM desligada deixa apenas ~520 KB de SRAM interna no S3. O framebuffer LVGL grande pode não caber. Quando reabilitar PSRAM no futuro, investigar se o módulo aceita **PSRAM Quad** (`CONFIG_SPIRAM_MODE_QUAD=y`) em vez de Octal — pode resolver o boot loop sem perder PSRAM.

## Links
- Memória: [[project_status_hals]]
- Entrada relacionada: [[2026-05-26T1815-git-reset-hard]]
- Próxima entrada: [[2026-05-26T1845-fixes-build-pos-reset]]
