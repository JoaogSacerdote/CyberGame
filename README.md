# CyberGame — Console Educativo de Cibersegurança

> Console portátil de jogo educativo que ensina conceitos de segurança digital
> (senhas, backup, ransomware, DDoS) por meio de mecânicas de RPG top-down 2D
> em pixel art. Hardware próprio baseado em ESP32-S3.

**Versão:** 0.1.0 · **Plataforma:** ESP32-S3 + ESP-IDF 6.0 + LVGL 9.x

---

## Sobre o projeto

O CyberGame é um console portátil construído em hardware próprio. O jogador
assume o papel de um profissional de TI em uma empresa e precisa reagir a
ameaças digitais (ransomware, DDoS, propagação de malware) aplicando as
contramedidas corretas — backup, isolamento de rede, gestão de senhas — dentro
do tempo de um "expediente". O objetivo é ensinar os pilares de segurança da
informação de forma prática e lúdica.

---

## Hardware

| Componente | Descrição |
|---|---|
| **MCU** | ESP32-S3 (N16R8) com 8 MB PSRAM Octal @ 40 MHz |
| **Display** | LCD SPI ST7796, 480×320 px, backlight via transistor (GPIO 42) |
| **Entrada** | Joystick analógico (ADC) + 4 botões (A / B / X / Y) |
| **Armazenamento** | Cartão microSD via SPI (assets e layouts de sala) |
| **LEDs** | 3× WS2812 RGB (indicadores de tarefa) |
| **NFC** | Leitor PN532 via I²C (cartas físicas liberam ações no jogo) |

O pinout completo está em [`hardware/PINOUT.md`](hardware/PINOUT.md). Esquemas
elétricos e arquivos de PCB ficam em [`hardware/`](hardware/).

---

## Arquitetura do firmware

```
main/                 boot, banner e inicialização da engine
components/
  engine/             loop principal, init dos HALs, task LVGL
  hardware/           HALs: display, botões, joystick, PMU, SD, NFC
  hal_bridge/         ponte hardware → LVGL (HALs não conhecem LVGL)
  assets/             carregador de assets (cache load-once) e diálogos
  asset_store/        camada de arquivos sobre o storage
  entity/             pool de entidades, movimento, Y-sort, debug overlay
  ui/                 telas: splash, menu, recepção, empresa, HUD, pause
  fsm/                máquina de estados do jogo
  gamestate/          estado persistente (progresso, tarefas)
  recovery/           modo de recuperação via USB CDC
```

**Convenções principais:**
- HALs (`components/hardware/*.h`) nunca incluem LVGL — a integração fica em `hal_bridge/`.
- Atualizações de UI passam por *diff-gate* e usam `lv_timer_create` (não tasks FreeRTOS).
- Sistema de entidades com pivô *bottom-center*; ordenação por Y-sort.

---

## Compilando e gravando

Requer [ESP-IDF 6.0](https://docs.espressif.com/projects/esp-idf/) instalado.

```powershell
# Ativar o ambiente do ESP-IDF (PowerShell)
. "C:\esp\v6.0\esp-idf\export.ps1"

# Build + gravação + monitor serial
idf.py build flash monitor
```

> Se o build falhar com erros de paths/MinGW, apague a pasta `build/` e
> recompile.

---

## Pipeline de assets

Os sprites são desenhados como PNGs individuais e convertidos em blobs RGB565
prontos para o ESP32, gravados no cartão microSD. Os scripts ficam em
[`tools/`](tools/):

```powershell
# Gera os IDs de asset a partir do registry
python tools/gen_asset_ids.py

# Converte sprites + layouts para sdcard/assets/
python tools/build_sd_assets.py

# Copiar o conteúdo de sdcard/assets/ para o cartão microSD
```

A fonte de verdade dos assets é [`assets/asset_registry.json`](assets/asset_registry.json)
(nome → ID → arquivo). Os layouts de sala (posições das entidades) ficam em
`assets/layout/`.

---

## Estrutura do repositório

```
main/            ponto de entrada do firmware
components/      componentes do firmware (engine, HALs, UI, etc.)
assets/          sprites, diálogos, layouts e registry de assets
tools/           scripts do pipeline de assets e utilitários
hardware/        pinout, esquemas elétricos e arquivos de PCB
pc_simulator/    simulador das telas em PC (LVGL + SDL)
simulation/      simulação em PC (LVGL + SDL2)
test/            testes
```

---

## Simulador em PC

As telas de UI podem ser testadas no computador sem o hardware. Veja
[`pc_simulator/README.md`](pc_simulator/README.md) e
[`simulation/`](simulation/) para detalhes de compilação com LVGL + SDL.

---

## Status

Em desenvolvimento ativo (versão 0.1.0). Já funcionando: HALs de display,
botões, joystick, PMU e SD; engine com loop LVGL; pipeline de assets; sistema
de entidades com Y-sort; salas de recepção e empresa; FSM de estados.

---

## Autoria

Projeto desenvolvido por:

| Autor | E-mail | GitHub |
|---|---|---|
| **João Gabriel** | `joaog.souza2002@gmail.com` | [@JoaogSacerdote](https://github.com/JoaogSacerdote) |
| **Klayveer Silva** | `silvaklayveer@gmail.com` | [@Klayveer](https://github.com/Klayveer) |
| **Caio Godoy** | `caiogodoy05@gmail.com` | [@caiocodes](https://github.com/caiocodes) |
