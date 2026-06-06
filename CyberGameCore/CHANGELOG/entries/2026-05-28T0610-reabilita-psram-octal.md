---
data: 2026-05-28T06:10
tipo: edit (config)
escopo: sdkconfig
trigger: assets do SD falhavam com ESP_ERR_NO_MEM (307200 B) — PSRAM estava desligada desde o bring-up de 26/05
commits:
  - (pendente — ainda nao commitado)
---

# Reabilita PSRAM Octal 80MHz

## Por que

Fase 2 (assets no SD) funcionou na leitura, mas `asset_loader` falhava ao
alocar a imagem do piso (480x320 RGB565 = 307200 B) com "PSRAM+DRAM
esgotadas". Causa: a **PSRAM estava desligada** desde 2026-05-26
([[2026-05-26T1830-disable-psram]]), deixando so ~234 KB de RAM interna —
nao cabe um asset de 300 KB.

O desligamento foi um paliativo de bring-up (placa nova entrava em boot loop
no `cpu_start` com PSRAM Octal). Desde entao varios fixes entraram; e o
`sdkconfig.defaults` SEMPRE manteve a PSRAM Octal 80MHz (o desligamento era
um override local so no `sdkconfig`).

## Antes (sdkconfig)
```
# CONFIG_SPIRAM is not set
...
# CONFIG_ESP32S3_SPIRAM_SUPPORT is not set
```
heap_init no boot mostrava so RAM interna (~234+21+32 KiB), sem linha de PSRAM.

## Depois (sdkconfig, confirmado pos-reconfigure)
```
CONFIG_SPIRAM=y
CONFIG_SPIRAM_MODE_OCT=y
CONFIG_SPIRAM_SPEED_80M=y
CONFIG_SPIRAM_USE_MALLOC=y
CONFIG_ESP32S3_SPIRAM_SUPPORT=y
```
Alinha o `sdkconfig` ao `sdkconfig.defaults` (que ja pedia Octal 80MHz).

## Resultado

reconfigure RC=0. **A confirmar em HW**: o motivo original do desligamento
era boot loop no `cpu_start` (RTC WDT) ao iniciar a PSRAM Octal. Tem que
observar o boot:
- Se subir e o `heap_init` mostrar a regiao grande de PSRAM (~8 MB) -> OK,
  os assets passam a caber.
- Se voltar a travar em `cpu_start` -> reverter e tentar **Octal @40MHz**
  (CONFIG_SPIRAM_SPEED_40M), depois **Quad** (CONFIG_SPIRAM_MODE_QUAD).

## REVERTIDO 2026-05-28 (mesmo dia)

**A PSRAM Octal @80MHz REPRODUZIU o boot loop.** Apos flash (pelo usuario), o
ESP parou de bootar: nem chegava ao menu, PWR parecia morto (o firmware
reseta antes de ler o botao). Exatamente o sintoma de
[[2026-05-26T1830-disable-psram]].

Revertido: `sdkconfig` voltou a `# CONFIG_SPIRAM is not set` +
`# CONFIG_ESP32S3_SPIRAM_SUPPORT is not set` (reconfigure RC=0, removeu os
dependentes). Firmware sem PSRAM reconstruido para regravar e devolver o
aparelho ao estado bom (boota ate o menu; salas falham com NO_MEM, que e
aceitavel ate resolver a PSRAM direito).

**NAO foi bricado** — esptool reseta por RTS e regrava mesmo em boot loop
(so precisou liberar a COM17, que estava ocupada por outro programa).

**Proxima tentativa de PSRAM (com cuidado, opt-in do usuario):** ainda
PRECISAMOS de PSRAM pros assets. Hipoteses a testar UMA por vez, validando o
boot a cada passo: (1) Octal @40MHz (CONFIG_SPIRAM_SPEED_40M); (2) Quad
(CONFIG_SPIRAM_MODE_QUAD) — pode ser que o modulo desta placa nova nao seja
Octal de verdade. Verificar tambem VDD_SPI/strapping (GPIO45).

## Links
- [[2026-05-26T1830-disable-psram]] (desligamento original + hipotese Quad)
- [[2026-05-28T0540-fase2-assets-na-sd]], [[project_storage_nand_to_sd]]
- [[build_environment]]
