---
tipo: roadmap
status: decidido
area: hardware
data: 2026-05-26
ultima-atualizacao: 2026-05-27
decisao: NÃO migrar agora; manter foco no S3 e entregar MVP. Plano B para v2.
---

# Migração futura → Placa ESP32-P4 JC4880P443C_I_W

Documento criado em 2026-05-26 após avaliação técnica. Quando/se o time decidir migrar pra v2 deluxe, esta nota tem tudo pra retomar sem refazer análise.

## Placa avaliada

**JC4880P443C_I_W** (fabricante chinês, série Guition/JC). Documentação completa em `CONSULTA/JC4880P443C_I_W/`.

| Recurso | Especificação |
|---|---|
| MCU | ESP32-P4 (RISC-V dual @400 MHz) |
| Display | 480 × 800 IPS 4.3" capacitivo MIPI-DSI (driver **ST7701**) |
| Touch | GT911 via I2C (SDA=GPIO7, SCL=GPIO8) |
| WiFi/BT | ESP32-C6 escravo integrado (transparente) — **NÃO usar nesse projeto** |
| SD Card | Slot SD-MMC 4-bit (D0=39, D1=40, D2=41, D3=42, CMD=44, CLK=43) |
| Áudio | Codec ES8311 + amplificador + alto-falante |
| Câmera | Conector MIPI-CSI (sensor OV02C10 opcional, não usamos) |
| ADC bateria | ADC_UNIT_2 ch4 já com divisor de tensão (V_min=2.25V, V_max=2.45V) |
| USB | 2x USB-C (full speed + high speed 480 Mbps) |
| Header expand | JP1 26 pinos com GPIOs livres: 52, 51, 50, 49, 35, 34, 32, 28, 33, 31, 30, 29 + ES_I2C_SDA/SCL |

## Por que considerar migrar

1. **Display MIPI-DSI já conectado** — elimina o maior risco do projeto S3 (transistor power-enable + level shifter + backlight + driver SPI ST7796).
2. **SD-MMC 4-bit em vez de SPI NAND** — 4× mais rápido, FAT32 nativo, cartão removível, fluxo de dev muito melhor (arrasta arquivos no Windows e plug & play).
3. **Áudio integrado** — codec ES8311 + alto-falante. Música ambiente, sons de NFC scan, alarmes de DDoS, efeitos de "demissão". Eleva produção do jogo.
4. **Bateria de graça** — `battery: 87%` no HUD sem hardware extra. ADC2 ch4 + cálculo no demo `adc_test.ino`.
5. **USB 2.0 HS** — recovery de assets cai de ~10s pra ~250ms.
6. **PSRAM obrigatória** mas problema chato resolvido pelo fabricante (testado em produção).

## Por que NÃO migrar agora

1. **Trabalho parado:** 80% do projeto S3 funcional. Migrar agora joga fora integração de hardware.
2. **Resolução muda de 480×320 paisagem → 480×800 retrato** — redesenho completo de TODOS os screens.
3. **Reescrita de 3 HALs:** `display_hal.c` (MIPI-DSI ST7701), `storage_hal.c` (SD-MMC), `joystick_hal.c` (API ADC nova).
4. **WiFi/BT presente mas indesejado** no projeto — não há motivo pra trocar plataforma só por isso.
5. **ESP-IDF para P4 mais recente** — alguns componentes managed (ex: `esp_tinyusb`) podem precisar ajuste.

## Estratégia de migração quando decidir

**Paralelizar redesign de UI (gargalo principal) com 4 pessoas:**
- Pessoa 1: `screen_splash`, `screen_menu`, `screen_credits`
- Pessoa 2: `screen_recepcao` + dialog system
- Pessoa 3: `screen_empresa` + collision maps
- Pessoa 4: `screen_hud`, `screen_pause`, `screen_game_over`

**Sequencial (1 pessoa):**
- Reescrever `display_hal.c` pra MIPI-DSI ST7701 (~2 dias)
- Reescrever `storage_hal.c` pra SD-MMC (~1 dia)
- Adaptar `joystick_hal.c` pra nova API ADC (~2 horas)
- Reajustar `pmu.c` se precisar (LP-RISC-V do P4)
- Migrar `sdkconfig` (`idf.py set-target esp32p4`)
- Pinout completo no novo layout

**Bônus opcionais (depois do MVP P4 rodar):**
- Áudio: música ambiente, SFX de NFC, alarmes — usar `Audio.h` + ES8311 + SD card
- Bateria: HUD com porcentagem
- Touch como debug menu secreto (dois toques + senha)
- USB HS pra acelerar dev workflow

**Não usar:**
- ESP32-C6 (WiFi/BT) — sem benefício pro jogo, mais complexidade
- Câmera MIPI-CSI — sem uso
- RS-485 — sem uso

## Conexão de periféricos externos

Header JP1 expand-IO (26 pinos) acomoda tudo. Plano de pinout sugerido:

| Periférico | Pino sugerido na placa | Notas |
|---|---|---|
| Botão A | GPIO 49 | qualquer livre |
| Botão B | GPIO 50 | qualquer livre |
| Botão X | GPIO 51 | qualquer livre |
| Botão Y | GPIO 52 | qualquer livre |
| Botão START | GPIO 33 | qualquer livre |
| Botão PWR | GPIO 35 | qualquer livre |
| Joystick X | GPIO 28 (ADC2) | conferir mapeamento ADC2 do P4 |
| Joystick Y | GPIO 32 (ADC2) | conferir |
| NFC SDA | ES_I2C_SDA (header) | I2C compartilha com touch — endereços diferentes |
| NFC SCL | ES_I2C_SCL (header) | idem |
| NFC IRQ | GPIO 29 | qualquer livre |

⚠️ Os botões físicos da placa (BOOT, RESET) **não** podem ser reaproveitados como START/PWR — BOOT força modo download se segurado no boot, RESET é hardware-only. Todos os 7 botões precisam vir no header expand-IO.

## Esforço total estimado

| Cenário | Tempo |
|---|---|
| Migrar mantendo UI rotacionada (paisagem 800×480 via software) | 3-4 dias |
| Migrar e redesenhar UI nativa 480×800 retrato (4 pessoas paralelo) | 1-1.5 semanas |
| Aproveitar bônus (áudio, bateria, touch debug) | +1 semana extra |

## Decisão atual

**NÃO migrar.** Foco: entregar MVP no S3 como planejado. Esta placa fica como roadmap pra **CyberGame v2 "deluxe"** após primeira entrega.

## Referências

- Documentação completa do fabricante: `CONSULTA/JC4880P443C_I_W/`
- Schematic: `5-Schematic/` (PDF + PNG por seção)
- Specs: `2-Specification/`
- Exemplos Arduino: `1-Demo/arduino_examples/` (LVGL, ADC, MP3, WiFi)
- Exemplos IDF: `1-Demo/idf_examples/` (precisa ESP-IDF v5.4+)
