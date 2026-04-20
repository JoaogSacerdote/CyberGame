# CyberSec: Network Defender — Simulador LVGL

Simulador PC do jogo embarcado, escrito em C usando **LVGL 9.x** (com camada de compatibilidade LVGL 8).  
Resolve o **display 320×240** (ILI9341) sem nenhum hardware real.

---

## Estrutura do Projeto

```
simulation/
├── readme.md
├── main/src/
│   ├── cybersec_game.h      ← API pública do jogo
│   └── cybersec_game.c      ← Motor completo (UI, FSM, lógica)
└── lv_port_pc_eclipse/      ← Template SDL para PC (submodule lvgl)
    ├── main.c               ← Entry point — integra cybersec_start()
    ├── cybersec_game.h      ← Cópia (gerada no setup)
    ├── cybersec_game.c      ← Cópia (gerada no setup)
    ├── lv_conf.h            ← Configuração LVGL
    └── bin/main.exe         ← Executável final (gerado após build)
```

---

## Pré-requisitos

| Ferramenta | Como obter (Windows) |
|---|---|
| GCC / CMake / SDL2 | MSYS2 — veja seção abaixo |
| Git | já incluso no sistema |

---

## Como Rodar (Windows)

### Jeito mais rápido — scripts prontos

Dentro da pasta `lv_port_pc_eclipse/` há dois arquivos `.bat`:

| Script | O que faz |
|---|---|
| `RODAR.bat` | **Inicia o jogo** (duplo clique) |
| `COMPILAR.bat` | Recompila tudo do zero (apenas se alterar o código) |

> **Se o jogo já foi compilado antes, basta dar duplo clique em `RODAR.bat`.**

---

### Primeiro uso — instalar ferramentas

Se for a primeira vez na máquina, instale o MSYS2 (GCC + CMake + SDL2).  
Abra o **CMD** ou **PowerShell** e execute:

```
winget install --id MSYS2.MSYS2 --accept-package-agreements --accept-source-agreements
```

Depois:

```
C:\msys64\usr\bin\pacman.exe -S --noconfirm --needed mingw-w64-x86_64-gcc mingw-w64-x86_64-cmake mingw-w64-x86_64-SDL2 mingw-w64-x86_64-make
```

Após instalar, execute `COMPILAR.bat` uma vez e depois `RODAR.bat`.

---

## Opção B — Wokwi (validação do firmware real)

No Wokwi com ESP32-S3 + ILI9341:
1. Adicione `cybersec_game.h` e `cybersec_game.c` ao projeto ESP-IDF.
2. No `app_main()`, após inicializar LVGL:
   ```c
   cybersec_start();
   ```
3. No handler do joystick/botões (FreeRTOS task), mapeie os eventos:
   ```c
   // Exemplo para botão de scan NFC:
   cybersec_sdl_key_event(SDLK_n, true);
   cybersec_sdl_key_event(SDLK_n, false);
   ```

> **Nota Wokwi**: substitua os `SDLK_*` por suas próprias constantes de key,
> pois no ESP-IDF não há SDL. Basta criar um enum equivalente e passar os valores.

---

## Controles

| Tecla | Ação |
|---|---|
| ↑ ↓ ← → | Mover o personagem pelo escritório |
| `SPACE` / `ENTER` | Resolver tarefa (verde) ou anomalia (amarelo) na sala atual |
| `N` | Simular scan NFC — resolve RANSOMWARE (vermelho) |
| `R` | Reiniciar o jogo |

---

## Mecânicas Implementadas

### Mapa Top-Down
Quatro salas num layout 2×2 com corredor central:

```
┌─────────────┬─────────────┐
│  Recepção   │  R. Humanos │
├─────────────┼─────────────┤  ← corredor
│  Financeiro │  Servidores │
└─────────────┴─────────────┘
```

### FSM de Eventos
Três níveis de ameaça escalados por probabilidade:

```
40% → TAREFA    (verde)  — drain  2 HP/2s
30% → ANOMALIA  (amarelo)— drain  5 HP/2s  
30% → RANSOMWARE(vermelho)— drain 12 HP/2s  ← exige NFC
```

### Sistema de HP
- Cada sala tem HP 0–100.
- Eventos não resolvidos drenam HP continuamente.
- **Integridade da rede** = média de HP das 4 salas.

### Relógio do Jogo
- 3 minutos reais = expediente das **08:00 às 18:00**.
- Sobreviver até 18h = vitória.

### Mecânica NFC
Ao pressionar `[N]` numa sala com Ransomware:
- Dialog confirma a autenticação do "Cartão de Backup".
- Sala recupera **+30 HP** (vs +15 de resolução normal).
- Conceito ensinado: **autenticação multifator** e **recuperação de desastre**.

---

## Arquitetura do Código

```
cybersec_start()
│
├── create_hud()        — HUD: relógio, barra de integridade, status
├── create_map()        — 4 salas + corredores
├── create_player()     — ícone do personagem
└── start_timers()
    ├── tmr_move_cb()   — movimentação (16 ms / ~62 fps)
    ├── tmr_clock_cb()  — relógio do jogo (500 ms)
    ├── tmr_event_cb()  — spawn aleatório de eventos (7–14 s)
    └── tmr_drain_cb()  — drenagem de HP (2 s)

cybersec_sdl_key_event()
├── keys.up/down/left/right → lido por tmr_move_cb
├── SPACE/ENTER             → resolve_room()
└── N                       → resolve_room() com NFC
```

---

## Extensões Sugeridas

- [ ] Sprite de personagem (LVGL image widget com bitmap)
- [ ] Animação de "digitação" ao resolver evento
- [ ] Som via buzzer (PWM) no ESP32 — integrar `ledc_set_freq()`
- [ ] Placar persistente em NVS (ESP32) ou arquivo (PC)
- [ ] Modo multiplayer: 2 analistas, 2 joysticks
- [ ] Geração procedural de mapa por andar