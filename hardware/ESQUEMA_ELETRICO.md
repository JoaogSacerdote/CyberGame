# CyberGame — Informações para o Esquema Elétrico

> Console portátil **CyberSec: Network Defender**.
> Documento de referência para desenhar o schematic (KiCad em `hardware/CyberGame_kicad/`).
> Pinout confirmado pelo usuário; última revisão de GPIO em 2026-05-12.

---

## 1. Componentes principais

| Bloco | Componente | Interface | Tensão |
|---|---|---|---|
| MCU | **ESP32-S3 N16R8** (16 MB Flash, 8 MB PSRAM Octal) | — | 3V3 |
| Asset storage | **NAND SPI Winbond W25N01GV** (1 Gbit / 128 MB) | SPI | 3V3 |
| Display | **LCD TFT 4.0" 480x320**, controlador **ST7796S** | SPI 4-fios | 3V3 lógica + VCC painel |
| NFC | **PN532** (módulo) | I²C | 3V3 |
| Input analógico | Joystick analógico dual-axis (modelo PSP slim, **sem botão SEL**) | 2x ADC | 3V3 |
| Input digital | 4x push-button A/B/X/Y + REC/START + PWR | GPIO | 3V3 |
| Áudio | Buzzer **passivo** (piezo) | PWM | 3V3 |
| LEDs | **3x WS2812** endereçáveis | RMT 1-wire | 5V (ou 3V3, ver §7) |
| Energia | Bateria Li-Po + carregador **TP4056** + regulador 3V3 | — | 3.7–4.2 V → 3V3 |

⚠️ **GPIOs 26–37 são proibidos** — usados internamente pela PSRAM Octal. Não rotear nada neles.

---

## 2. Pinout definitivo do ESP32-S3

| GPIO | Função | Bloco | Observações |
|------|--------|-------|-------------|
| 1  | Joystick eixo X | ADC1_CH0 | Entrada analógica |
| 2  | Joystick eixo Y | ADC1_CH1 | Entrada analógica |
| 3  | **REC + START** (dual-use) | Botão | Boot: PMU lê como REC (combo PWR+REC = recovery). Pós-boot: BTN_START (pause). Ativo em nível baixo. |
| 4  | **PWR** | Botão | Botão de power/sleep. Fonte de wake `ext0` do Deep Sleep. Ativo em nível baixo. |
| 5  | NFC **SDA** | I²C | Precisa pull-up (ver §6) |
| 6  | NFC **SCL** | I²C | Precisa pull-up (ver §6) |
| 7  | **SPI MOSI** | SPI | **Compartilhado** Display + NAND |
| 8  | **SPI SCK** | SPI | **Compartilhado** Display + NAND |
| 9  | **MISO** | SPI | Usado pela NAND (leitura). Display é write-only. |
| 10 | NAND **CS** | SPI | Chip-select da NAND |
| 11 | Botão **A** | Botão | Confirmar / Interagir. Ativo em nível baixo. |
| 12 | Botão **B** | Botão | Cancelar / Voltar. Ativo em nível baixo. |
| 13 | Botão **X** | Botão | Dashboard do Analista. Ativo em nível baixo. |
| 14 | Botão **Y** | Botão | Abrir Terminal. Ativo em nível baixo. |
| 15 | **Buzzer** | PWM (LEDC) | Piezo passivo, acionamento direto. **Não** é strapping pin — escolha segura. |
| 16 | NFC **IRQ** | GPIO in | Interrupção do PN532 |
| 17 | Display **CS** | SPI | Chip-select do display |
| 18 | Display **DC** | GPIO out | Data/Command do ST7796S |
| 21 | Display **RST** | GPIO out | Reset do ST7796S |
| 38 | Display **BL** (LED do painel) | PWM (LEDC) | Pino LED/backlight do painel — controle de brilho |
| 42 | Display **VCC enable** | GPIO out | Liga/corta VCC do painel via transistor **NPN**. **1 = LIGA, 0 = CORTA** (ver §5) |
| 45 | *(livre)* | — | **Strapping pin** (VDD_SPI voltage) — deixar livre / não conectar. |
| 48 | **WS2812 Data** | RMT | Linha de dados dos 3 LEDs em cadeia |

**GPIOs livres / não usados:** 45 (strapping — não usar). Os demais não listados estão livres exceto 26–37 (PSRAM).

---

## 3. Barramento SPI (compartilhado: Display + NAND)

```
ESP32-S3  GPIO7  (MOSI) ──┬── ST7796S SDA/MOSI
                          └── W25N01GV DI (IO0)
ESP32-S3  GPIO8  (SCK)  ──┬── ST7796S SCK
                          └── W25N01GV CLK
ESP32-S3  GPIO9  (MISO) ───── W25N01GV DO (IO1)        (display não usa MISO)
ESP32-S3  GPIO17 (CS)   ───── ST7796S CS
ESP32-S3  GPIO10 (CS)   ───── W25N01GV CS
```

- Dois dispositivos, **um barramento**, **CS independentes** — só um ativo por vez.
- Display roda a 40 MHz; a NAND suporta até ~104 MHz. O barramento é seguro pela seleção de CS.
- **Pull-up de ~10 kΩ em cada CS** (GPIO10 e GPIO17) para 3V3 — evita seleção espúria durante boot/reset, quando os GPIOs estão em alta impedância.
- Manter as trilhas MOSI/SCK curtas e de comprimento parecido; se possível, resistor série de 22–33 Ω em MOSI e SCK próximos ao ESP para conter ringing.
- W25N01GV: amarrar **WP#** e **HOLD#** a 3V3 (não usados no modo SPI single).

---

## 4. Barramento I²C (NFC PN532)

```
ESP32-S3  GPIO5 (SDA) ──┬── PN532 SDA ──── [4.7 kΩ] ── 3V3
ESP32-S3  GPIO6 (SCL) ──┬── PN532 SCL ──── [4.7 kΩ] ── 3V3
ESP32-S3  GPIO16      ───── PN532 IRQ
                          PN532 VCC ── 3V3
                          PN532 GND ── GND
```

- **Pull-ups obrigatórios**: 4.7 kΩ em SDA e SCL para 3V3 (se o módulo PN532 já trouxer pull-ups na placa, **não duplicar** — verificar antes).
- Confirmar que o módulo PN532 está com os jumpers/switch em modo **I²C** (muitos vêm em HSU/SPI de fábrica).
- IRQ é entrada — o firmware energiza o campo RF só quando necessário (estado de terminal). RST do PN532 pode ficar em 3V3 fixo se não for usado.

---

## 5. Display ST7796S — alimentação e controle

**Lógica (3V3):** CS=GPIO17, DC=GPIO18, RST=GPIO21, SCK=GPIO8, MOSI=GPIO7. MISO do display não é usado.

**Resumo da fiação de energia do display (confirmado pelo usuário):**
- **GPIO42** → base do transistor **NPN** → chave do **VCC do painel**.
- **GPIO38** → pino **LED/backlight** do painel (controle de brilho via PWM).

**VCC do painel — chave via GPIO42:**
- O GPIO 42 controla um **transistor NPN** que atua como *driver de gate* de uma chave high-side (P-MOSFET) que liga/desliga o **VCC do painel**.
- Polaridade lógica: **GPIO42 = 1 → painel LIGADO**; **GPIO42 = 0 → painel DESLIGADO**.
- Topologia recomendada:
  ```
  GPIO42 ──[1 kΩ]── base NPN ;  emissor NPN → GND
  coletor NPN ──┬── gate do P-MOSFET high-side
                └──[100 kΩ]── source/3V3 (pull-up do gate)
  P-MOSFET: source → 3V3 (ou rail do painel) ; dreno → VCC do display
  ```
  Com GPIO42=1 o NPN conduz, puxa o gate do P-MOSFET para baixo, e o P-MOSFET liga o VCC.
- ⚠️ O código de referência em `CONSULTA/display_hal.c` ainda usa lógica **PNP antiga** (0=liga). O hardware atual é **NPN (1=liga)** — substituído em 2026-05-10.
- Em boot: setar GPIO42=1 **antes** de iniciar o SPI e aguardar ~50 ms de estabilização.

**Backlight (BL = GPIO38):**
- Controlado por PWM (LEDC). Se o LED de backlight do painel exigir mais corrente do que um GPIO entrega com segurança (>20 mA), inserir um **transistor/MOSFET** para acionar o backlight, com o GPIO38 na base/gate. Verificar a corrente do backlight no datasheet do módulo do painel.

**Decoupling:** 100 nF + 10 µF próximos da alimentação do módulo do display.

---

## 6. Entradas digitais (botões)

6 botões momentâneos, todos **ativo-baixo** (botão entre GPIO e GND):

```
GPIO ──┬── botão ── GND
       └──[pull-up]── 3V3
```

- O ESP32-S3 tem **pull-up interno** — pode dispensar resistor externo. Para robustez (ruído/cabos longos), opcional pull-up externo de 10 kΩ.
- Debounce é feito em **firmware** (`button_hal`: ISR + timer). Opcional capacitor de 100 nF em paralelo com cada botão para debounce em hardware.
- Botões: A=11, B=12, X=13, Y=14, REC/START=3, PWR=4.
- **GPIO3 e GPIO4** são lidos pela PMU no boot (combo PWR+REC = modo recovery). GPIO4 também é a fonte de wake `ext0` do Deep Sleep — garantir que em repouso o pino fique em nível **alto** (pull-up) e o botão leve a **baixo**.

---

## 7. WS2812 (3 LEDs endereçáveis)

```
ESP32-S3 GPIO48 ──[330–470 Ω]── DIN do LED1 ── DOUT→DIN LED2 ── DOUT→DIN LED3
LEDs VCC ── 5V (ou 3V3, ver nota) ;  LEDs GND ── GND
Capacitor 1000 µF entre VCC e GND dos LEDs, próximo ao primeiro LED
```

- **Resistor série 330–470 Ω** na linha de dados, junto ao GPIO48.
- **Capacitor de bulk 1000 µF** (≥6.3 V) nos terminais de alimentação da fita/LEDs.
- **Alimentação: 3V3** (decisão confirmada pelo usuário). Os 3 LEDs são alimentados pelo próprio rail 3V3 do sistema — **sem level shifter**. Com VCC=3V3 o nível lógico "1" do ESP (3.3 V) atende o WS2812 sem margem problemática. Brilho levemente menor que a 5V, aceitável para 3 LEDs de status.
- Função: LED1 = tarefa verde pendente, LED2 = tarefa amarela, LED3 = alerta de ataque (lógica composta de escalada — ver `game_logic_decisions`).

---

## 8. Buzzer (áudio)

```
GPIO15 ──[resistor série opcional 100 Ω]── piezo ── GND
```

- **Pino escolhido: GPIO15.** Motivo: está livre, **não é strapping pin** e suporta PWM via LEDC. Substitui o GPIO45 sugerido antes — GPIO45 é strapping pin (VDD_SPI) e foi liberado.
- Buzzer **passivo** acionado por **PWM (LEDC)** direto do GPIO15 — **sem transistor** nesta versão (decisão de projeto).
- Só SFX, sem música de fundo. Mute = duty 0%.
- Se o volume ficar baixo, evoluir depois para um transistor + indutor/driver — mas **não** está no MVP.

---

## 9. Joystick analógico

Joystick dual-axis com 2 potenciômetros (um por eixo). **Sem botão SEL.**

```
Pot eixo X:  terminal1 → 3V3 ;  terminal2 → GND ;  wiper → GPIO1 (ADC1_CH0)
Pot eixo Y:  terminal1 → 3V3 ;  terminal2 → GND ;  wiper → GPIO2 (ADC1_CH1)
```

- Capacitor de **10–100 nF** de cada wiper para GND — filtra ruído do ADC.
- Usar **ADC1** (GPIO1/GPIO2 são ADC1) — ADC2 conflita com o Wi-Fi/rádio.
- Firmware faz média móvel (MA8) + deadzone 5% + calibração; mesmo assim o filtro RC em hardware ajuda.

---

## 10. Subsistema de energia

```
Li-Po 3.7V ──→ TP4056 (carga + proteção) ──→ Regulador 3V3 ──→ rail 3V3 do sistema
                    ↑
                USB 5V (carga)
```

- **TP4056**: módulo de carga Li-Po. Usar preferencialmente a versão **com proteção** (DW01 + FS8205) contra sobre-descarga/sobre-corrente.
- **Regulador 3V3**: a bateria entrega 3.0–4.2 V; o ESP32-S3 e todos os periféricos rodam em **3V3 regulado**. Opções:
  - **LDO** (ex.: AMS1117-3.3, ME6211, RT9013) — simples; escolher um que aguente o pico de corrente do ESP-S3 em TX (~500 mA de margem). O AMS1117 tem dropout alto (~1.1 V) — com a bateria abaixo de ~3.4 V o rail cai; preferir um LDO **low-dropout** (ME6211/RT9013, dropout ~0.25 V).
  - **Buck** se quiser maximizar autonomia.
- **Decoupling do ESP32-S3**: 100 nF em cada pino VDD + um bulk de 22–47 µF próximo ao módulo. Seguir as recomendações do datasheet do módulo ESP32-S3.
- **PWR / power management**: não há latch de hardware no MVP — o desligamento é via **Deep Sleep** do ESP, com wake por `ext0` no GPIO4. O botão PWR (GPIO4) só precisa de botão→GND + pull-up. (Se quiser desligamento real de hardware no futuro, adicionar um circuito de soft-latch com MOSFET.)
- Estimar a corrente de pico: ESP-S3 + backlight do display + 3 WS2812 podem somar facilmente >400 mA — dimensionar regulador e trilhas de alimentação para isso.

---

## 11. Checklist para o schematic

- [ ] ESP32-S3 N16R8 com decoupling completo (datasheet do módulo) e EN com RC de reset.
- [ ] Strapping pins: GPIO0 (boot), GPIO45/46 — **deixar livres** (GPIO45 não é mais usado pelo buzzer; buzzer migrou para GPIO15). GPIO3 também é strapping mas é uso intencional (REC).
- [ ] GPIOs 26–37 livres (PSRAM Octal).
- [ ] Pull-ups: I²C SDA/SCL (4.7k), SPI CS x2 (10k), botões (interno ou 10k).
- [ ] Barramento SPI com CS independentes; WP#/HOLD# da NAND em 3V3.
- [ ] Transistor NPN + P-MOSFET do VCC do display (GPIO42, lógica 1=liga).
- [ ] Resistor série + capacitor bulk dos WS2812; decidir alimentação 3V3.
- [ ] Filtro RC nos wibers do joystick; usar ADC1.
- [ ] TP4056 (com proteção) + regulador 3V3 low-dropout dimensionado para o pico.
- [ ] Conectores: USB (carga + dados/recovery CDC), bateria, display (FPC/header), módulo NFC, módulo joystick.
- [ ] Pontos de teste em 3V3, GND, MOSI, SCK, SDA, SCL.

---

## 12. Pendências / decisões em aberto de hardware

- **Backlight do display**: GPIO38 vai no pino LED do painel. Confirmar a corrente desse LED — define se GPIO38 aciona direto ou via MOSFET.
- **PN532**: confirmar se o módulo já tem pull-ups I²C na placa (para não duplicar) e se o modo está em I²C.
- Sem latch de power em hardware — desligamento só por Deep Sleep. Reavaliar se for requisito.

### Resolvidos
- ✅ **Buzzer**: GPIO15 (livre, não-strapping). GPIO45 liberado.
- ✅ **WS2812**: alimentação 3V3, sem level shifter.
- ✅ **GPIO42/38**: 42 = NPN do VCC do painel; 38 = pino LED/backlight. Lógica NPN (1=liga).

---

*Fontes: `hardware/PINOUT`, memória de projeto `project_hardware`, `game_logic_decisions` (RESPOSTAS.txt 2026-05-12), e código atual dos HALs em `components/hardware/`.*
