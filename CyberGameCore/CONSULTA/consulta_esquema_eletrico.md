---
tags: [cybersec, hardware, schematic, pcb, eletronica]
projeto: CyberSec / CyberGame
status: referencia-ativa
data: 2026-05-14
---

# Consulta — Esquema Elétrico e PCB do CyberGame

> Documento **autoritativo** com tudo que é preciso para desenhar o esquema
> elétrico e a PCB do console portátil **CyberSec: Network Defender**.
> Consolida o pinout, barramentos, subsistema de energia e notas de layout.
> Snapshot 2026-05-14 — reflete todas as decisões de hardware até esta data.

---

## 1. Componentes principais

| Bloco | Componente | Encapsulamento | Interface | Tensão |
|---|---|---|---|---|
| MCU | **ESP32-S3-WROOM-1-N16R8** (16 MB Flash, 8 MB PSRAM Octal) | módulo | — | 3V3 |
| Asset storage | **NAND SPI Winbond W25N01GV** (1 Gbit / 128 MB) | WSON-8 / SOIC-8 | SPI | 3V3 |
| Display | LCD TFT 4.0" 480×320, controlador **ST7796S** | módulo + FPC | SPI 4-fios | 3V3 lógica + VCC painel |
| NFC | módulo **PN532** | módulo | I²C | 3V3 |
| Input analógico | joystick analógico dual-axis (modelo PSP slim, **sem botão SEL**) | — | 2× ADC | 3V3 |
| Input digital | 4× push-button A/B/X/Y + REC/START + PWR | — | GPIO | 3V3 |
| Áudio | buzzer **passivo** (piezo) | — | PWM | 3V3 |
| LEDs | 3× **WS2812B** endereçáveis | — | RMT 1-wire | **3V3** |
| Energia | bateria Li-Po + carregador **TP4056** (c/ proteção) + regulador 3V3 LDO | módulos | — | 3.0–4.2 V → 3V3 |

⚠️ **GPIOs 26–37 são proibidos** — usados internamente pela PSRAM Octal. Não rotear nada neles.

---

## 2. Diagrama de blocos

```
                      +-------------------+
   USB-C  ──5V──────▶ │  TP4056 (carga +  │
   (carga)            │  proteção Li-Po)  │
                      +---------+---------+
   Li-Po 3.7V ───────────────────┤
                      +---------v---------+
                      │  Regulador 3V3    │  (LDO low-dropout)
                      +---------+---------+
                                │ rail 3V3
        ┌───────────────┬───────┼────────┬───────────────┬──────────────┐
        │               │       │        │               │              │
   +----v----+    +------v----+  │  +-----v-----+   +------v-----+  +-----v-----+
   │ESP32-S3 │    │ ST7796S   │  │  │ W25N01GV  │   │  PN532     │  │ 3× WS2812B│
   │WROOM-1  │    │ (display) │  │  │  (NAND)   │   │  (NFC)     │  │ (status)  │
   │N16R8    │    +-----------+  │  +-----------+   +------------+  +-----------+
   +----+----+         ▲ SPI2 ───┴──── ▲ SPI2          ▲ I²C            ▲ RMT
        │              │  (CS=17)      │ (CS=10)        │ (5/6)          │ (48)
        ├── ADC1 ◀── joystick X/Y (GPIO 1/2)
        ├── GPIO ◀── botões A/B/X/Y/REC+START/PWR
        └── PWM ──▶ buzzer (GPIO 15) , backlight (GPIO 38)
```

---

## 3. Pinout definitivo do ESP32-S3

| GPIO | Função | Bloco | Observações |
|------|--------|-------|-------------|
| 1  | Joystick eixo X | ADC1_CH0 | entrada analógica |
| 2  | Joystick eixo Y | ADC1_CH1 | entrada analógica |
| 3  | **REC + START** (dual-use) | botão | boot: PMU lê como REC (combo PWR+REC = recovery); pós-boot: BTN_START (pause). Ativo-baixo. **Strapping pin** (uso intencional) |
| 4  | **PWR** | botão | power/sleep; fonte de wake `ext0` do Deep Sleep. Ativo-baixo |
| 5  | NFC **SDA** | I²C | precisa pull-up (§5) |
| 6  | NFC **SCL** | I²C | precisa pull-up (§5) |
| 7  | **SPI MOSI** | SPI2 | **compartilhado** Display + NAND |
| 8  | **SPI SCK** | SPI2 | **compartilhado** Display + NAND |
| 9  | **MISO** | SPI2 | usado pela NAND (leitura); display é write-only |
| 10 | NAND **CS** | SPI2 | chip-select da NAND |
| 11 | Botão **A** | botão | Confirmar / Interagir. Ativo-baixo |
| 12 | Botão **B** | botão | Cancelar / Voltar. Ativo-baixo |
| 13 | Botão **X** | botão | Dashboard do Analista. Ativo-baixo |
| 14 | Botão **Y** | botão | Abrir Terminal. Ativo-baixo |
| 15 | **Buzzer** | PWM (LEDC) | piezo passivo, acionamento direto. **Não** é strapping pin |
| 16 | NFC **IRQ** | GPIO in | interrupção do PN532 |
| 17 | Display **CS** | SPI2 | chip-select do display |
| 18 | Display **DC** | GPIO out | Data/Command do ST7796S |
| 21 | Display **RST** | GPIO out | reset do ST7796S |
| 38 | Display **BL** (LED do painel) | PWM (3C) | pino LED/backlight do painel — controle de brilho |
| 42 | Display **VCC enable** | GPIO out | liga/corta VCC do painel via transistor **NPN**. **1 = LIGA, 0 = CORTA** (§4) |
| 48 | **WS2812 Data** | RMT | linha de dados dos 3 LEDs em cadeia |

**GPIOs livres / reservas:** 15 ocupado agora (buzzer). **45 é strapping pin (VDD_SPI) — deixar livre.** GPIOs não listados estão livres, exceto **26–37 (PSRAM Octal)**.

**Outros strapping pins do ESP32-S3** a observar no layout: GPIO0 (boot), GPIO3 (uso intencional = REC), GPIO45, GPIO46. Nenhum deles deve ser puxado para um nível indevido no boot.

---

## 4. Barramento SPI (compartilhado: Display + NAND)

```
ESP32-S3 GPIO7  (MOSI) ──┬── ST7796S SDA/MOSI
                         └── W25N01GV DI (IO0)
ESP32-S3 GPIO8  (SCK)  ──┬── ST7796S SCK
                         └── W25N01GV CLK
ESP32-S3 GPIO9  (MISO) ───── W25N01GV DO (IO1)        (display não usa MISO)
ESP32-S3 GPIO17 (CS)   ───── ST7796S CS
ESP32-S3 GPIO10 (CS)   ───── W25N01GV CS
```

- **Um barramento (SPI2_HOST), dois dispositivos, CS independentes** — só um ativo por vez.
- **Clocks independentes por device:** Display = **40 MHz**, NAND W25N01GV = **50 MHz** (subido em duas etapas a partir de 10 MHz após estabilização — `selftest` passa nessa frequência; o chip aguenta até 104 MHz; o limite é a integridade de sinal da PCB).
- **Pull-up de ~10 kΩ em cada CS** (GPIO10 e GPIO17) para 3V3 — evita seleção espúria durante boot/reset (GPIOs em alta impedância).
- Trilhas MOSI/SCK curtas e de comprimento parecido; se possível **resistor série de 22–33 Ω** em MOSI e SCK próximos ao ESP para conter ringing.
- W25N01GV: amarrar **WP#** e **HOLD#** a 3V3 (não usados no modo SPI single).
- Decoupling 100 nF próximo de cada VCC (display e NAND).

---

## 5. Barramento I²C (NFC PN532)

```
ESP32-S3 GPIO5 (SDA) ──┬── PN532 SDA ──── [4.7 kΩ] ── 3V3
ESP32-S3 GPIO6 (SCL) ──┬── PN532 SCL ──── [4.7 kΩ] ── 3V3
ESP32-S3 GPIO16      ───── PN532 IRQ
                         PN532 VCC ── 3V3
                         PN532 GND ── GND
```

- **Pull-ups obrigatórios** 4.7 kΩ em SDA e SCL para 3V3. ⚠️ Se o módulo PN532 já trouxer pull-ups na placa, **não duplicar** — verificar antes.
- Conferir que o módulo PN532 está em modo **I²C** (muitos vêm em HSU/SPI de fábrica — jumpers/switch).
- IRQ é entrada. RST do PN532 pode ficar fixo em 3V3 se não for usado.
- I²C roda a 100 kHz.

---

## 6. Display ST7796S — alimentação e controle

**Lógica (3V3):** CS=GPIO17, DC=GPIO18, RST=GPIO21, SCK=GPIO8, MOSI=GPIO7. MISO do display não é usado.

**Resumo da fiação de energia (confirmado pelo usuário):**
- **GPIO42** → base do transistor **NPN** → chave do **VCC do painel**.
- **GPIO38** → pino **LED/backlight** do painel (controle de brilho via PWM LEDC).

**VCC do painel — chave NPN via GPIO42:**
- Polaridade lógica: **GPIO42 = 1 → painel LIGADO**; **GPIO42 = 0 → painel DESLIGADO**.
- Topologia recomendada (NPN como driver de gate de uma chave high-side P-MOSFET):
  ```
  GPIO42 ──[1 kΩ]── base NPN ;  emissor NPN → GND
  coletor NPN ──┬── gate do P-MOSFET high-side
                └──[100 kΩ]── source/3V3 (pull-up do gate)
  P-MOSFET: source → 3V3 (ou rail do painel) ; dreno → VCC do display
  ```
  GPIO42=1 → NPN conduz → puxa o gate do P-MOSFET para baixo → P-MOSFET liga o VCC.
- Em boot: setar GPIO42=1 **antes** de iniciar o SPI e aguardar ~50 ms de estabilização.

**Backlight (BL = GPIO38):** controlado por PWM (LEDC). Se o LED de backlight do módulo exigir mais corrente do que um GPIO entrega com segurança (>20 mA), inserir **transistor/MOSFET** acionado pelo GPIO38. Verificar a corrente do backlight no datasheet do módulo do painel.

**Decoupling:** 100 nF + 10 µF próximos da alimentação do módulo do display.

---

## 7. Entradas digitais (botões)

6 botões momentâneos, todos **ativo-baixo** (botão entre GPIO e GND):

```
GPIO ──┬── botão ── GND
       └──[pull-up 10 kΩ opcional]── 3V3
```

- O ESP32-S3 tem **pull-up interno** — pode dispensar resistor externo. Para robustez (ruído/cabos longos), pull-up externo de 10 kΩ é recomendado.
- Debounce feito em **firmware** (ISR + timer, 50 ms). Capacitor de 100 nF em paralelo com cada botão é opcional (debounce em hardware).
- Mapeamento: A=11, B=12, X=13, Y=14, REC/START=3, PWR=4.
- **GPIO3 e GPIO4** são lidos pela PMU no boot (combo PWR+REC = modo recovery). GPIO4 é a fonte de wake `ext0` do Deep Sleep — garantir que em repouso o pino fique **alto** (pull-up) e o botão leve a **baixo**.

---

## 8. WS2812B (3 LEDs endereçáveis de status)

```
ESP32-S3 GPIO48 ──[330–470 Ω]── DIN LED1 ── DOUT→DIN LED2 ── DOUT→DIN LED3
LEDs VCC ── 3V3 ;  LEDs GND ── GND
Capacitor 1000 µF (≥6.3 V) entre VCC e GND dos LEDs, junto ao 1º LED
```

- **Alimentação: 3V3** (decisão de projeto). Sem level shifter — com VCC=3V3 o "1" lógico do ESP (3.3 V) atende o WS2812B. Brilho levemente menor que a 5V, aceitável para 3 LEDs de status.
- **Resistor série 330–470 Ω** na linha de dados, junto ao GPIO48.
- **Capacitor de bulk 1000 µF** nos terminais de alimentação dos LEDs.
- Função: LED1 = tarefa verde pendente, LED2 = tarefa amarela, LED3 = alerta de ataque (lógica de escalada composta).

---

## 9. Buzzer (áudio)

```
GPIO15 ──[resistor série opcional 100 Ω]── piezo ── GND
```

- Buzzer **passivo** acionado por **PWM (LEDC)** direto do GPIO15 — **sem transistor** nesta versão.
- **GPIO15 foi escolhido** por estar livre, **não ser strapping pin** e suportar LEDC. (Substituiu o GPIO45 da sugestão inicial — GPIO45 é strapping pin VDD_SPI.)
- Só SFX, sem música de fundo. Mute = duty 0%.
- Se o volume ficar baixo, evoluir depois para transistor + driver — fora do escopo atual.

---

## 10. Joystick analógico

Joystick dual-axis com 2 potenciômetros (um por eixo). **Sem botão SEL.**

```
Pot eixo X:  terminal1 → 3V3 ;  terminal2 → GND ;  wiper → GPIO1 (ADC1_CH0)
Pot eixo Y:  terminal1 → 3V3 ;  terminal2 → GND ;  wiper → GPIO2 (ADC1_CH1)
```

- Capacitor de **10–100 nF** de cada wiper para GND — filtra ruído do ADC.
- Usar **ADC1** (GPIO1/GPIO2 são ADC1) — ADC2 conflita com o rádio.
- Firmware faz média móvel (MA8) + deadzone 5% + calibração de centro no boot.

---

## 11. Subsistema de energia

```
USB-C 5V ──▶ TP4056 (carga + proteção Li-Po) ──▶ Li-Po 3.7V ──▶ Regulador 3V3 ──▶ rail 3V3
```

- **TP4056**: módulo de carga Li-Po — usar a versão **com proteção** (DW01 + FS8205) contra sobre-descarga/sobre-corrente.
- **Regulador 3V3**: a bateria entrega 3.0–4.2 V; ESP32-S3 e todos os periféricos rodam em **3V3 regulado**.
  - Preferir um **LDO low-dropout** (ME6211, RT9013 — dropout ~0.25 V) em vez do AMS1117 (dropout ~1.1 V — com a bateria abaixo de ~3.4 V o rail cai).
  - Dimensionar para o **pico de corrente**: ESP-S3 em TX + backlight do display + 3 WS2812 podem somar >400 mA.
- **Decoupling do ESP32-S3**: 100 nF em cada pino VDD + bulk de 22–47 µF próximo ao módulo. Seguir o datasheet do módulo WROOM-1.
- **EN do ESP32-S3**: RC de reset (10 kΩ pull-up + 1 µF para GND) + botão de reset opcional.
- **Power management**: não há latch de hardware no MVP — desligamento via **Deep Sleep** do ESP, wake por `ext0` no GPIO4. O botão PWR só precisa de botão→GND + pull-up. (Para desligamento real de hardware no futuro: circuito de soft-latch com MOSFET.)

---

## 12. Notas específicas de PCB / layout

- **GPIOs 26–37 livres** — PSRAM Octal os usa internamente. Não rotear.
- **Strapping pins** (GPIO0/3/45/46): conferir estados de boot. GPIO45 livre; GPIO3 = REC (intencional); GPIO15 (buzzer) **não** é strapping — escolha segura.
- **Plano de terra** sólido sob o barramento SPI e a PSRAM/flash.
- **Decoupling** próximo de cada CI: ESP32-S3, NAND, display, PN532.
- **SPI**: trilhas MOSI/SCK/MISO curtas e pareadas; resistores série 22–33 Ω perto do ESP.
- **WS2812**: capacitor 1000 µF junto ao 1º LED; resistor série 330–470 Ω.
- **Joystick/ADC**: filtro RC nos wipers; manter trilhas analógicas longe de SPI/PWM.
- **Conectores**: USB-C (carga + dados/recovery CDC), bateria Li-Po (JST), display (FPC ou header), módulo NFC, módulo joystick.
- **Pontos de teste**: 3V3, GND, MOSI, SCK, MISO, SDA, SCL, EN.

---

## 13. Checklist do esquema

- [ ] ESP32-S3-WROOM-1-N16R8 com decoupling completo + RC no EN.
- [ ] Strapping pins (0/3/45/46) em estado de boot correto; GPIO45 livre.
- [ ] GPIOs 26–37 livres (PSRAM Octal).
- [ ] Barramento SPI2 com CS independentes (10 + 17); pull-up 10 kΩ em cada CS; WP#/HOLD# da NAND em 3V3.
- [ ] Barramento I²C com pull-ups 4.7 kΩ (ou confirmados no módulo PN532); módulo em modo I²C.
- [ ] Transistor NPN + P-MOSFET do VCC do display (GPIO42, lógica 1=liga).
- [ ] Backlight: GPIO38 direto ou via MOSFET conforme corrente do painel.
- [ ] WS2812B: alimentação 3V3, resistor série + capacitor bulk 1000 µF.
- [ ] Filtro RC nos wipers do joystick; ADC1.
- [ ] Botões ativo-baixo (pull-up interno ou 10 kΩ externo).
- [ ] Buzzer no GPIO15.
- [ ] TP4056 (com proteção) + regulador 3V3 low-dropout dimensionado para o pico.
- [ ] Conectores e pontos de teste.

---

## 14. Pendências / decisões a ratificar

- **Corrente do backlight do display**: confirmar no datasheet do módulo — define se GPIO38 aciona direto ou via MOSFET.
- **PN532**: confirmar se o módulo já tem pull-ups I²C na placa (não duplicar) e se está em modo I²C.
- **NAND SPI a 50 MHz**: validado em HW com `selftest` (multi-region 5/5 OK, bad block scan limpo). Se evoluir a PCB ou trocar de NAND, revalidar com o mesmo comando antes de subir além.
- **Sem latch de power em hardware** — desligamento só por Deep Sleep. Reavaliar se virar requisito.

---

*Fontes: `hardware/PINOUT`, `hardware/ESQUEMA_ELETRICO.md`, código atual dos HALs em `components/hardware/` (incl. `storage_hal.c` com SPI a 50 MHz), e as decisões de hardware consolidadas no RESPOSTAS.txt.*
