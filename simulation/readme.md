# CyberSec: Network Defender — Simulador LVGL (PC)

Simulador de PC do jogo embarcado **CyberSec: Network Defender**, escrito em C usando **LVGL 9.x**
renderizado via **SDL2**. Permite rodar e testar o jogo completo no Windows sem nenhum hardware real.

- Janela: **480 × 320 px** (equivalente ao display ILI9341 do ESP32-S3)
- Build: **CMake + MinGW GCC** via MSYS2
- Dependências externas: apenas SDL2 (já incluída no MSYS2)

---

## Estrutura do Projeto

```
simulation/
├── readme.md                       ← este arquivo
├── img/                            ← screenshots de referência (UI mockups)
└── lv_port_pc_eclipse/             ← projeto CMake para PC
    ├── main.c                      ← entry point: inicializa SDL + LVGL, chama cybersec_start()
    ├── cybersec_game.h             ← API pública do jogo
    ├── cybersec_game.c             ← motor completo (UI, FSM, lógica, ~1500 linhas)
    ├── lv_conf.h                   ← configuração LVGL (fontes, cores, resolução)
    ├── CMakeLists.txt              ← build system
    ├── COMPILAR.bat                ← compila o projeto (git submodule + cmake)
    ├── RODAR.bat                   ← inicia o executável com DLLs corretas
    ├── lvgl/                       ← biblioteca LVGL 9.3 (clonada pelo COMPILAR.bat)
    └── bin/
        └── main.exe                ← executável final (gerado após build)
```

---

## Pré-requisitos

| Ferramenta | Versão mínima | Observação |
|---|---|---|
| Windows | 10 / 11 | — |
| Git | qualquer | Para clonar o submodule LVGL |
| MSYS2 (GCC + CMake + SDL2) | — | Instalar antes de compilar |

### Instalar MSYS2 (apenas na primeira vez)

```bat
winget install --id MSYS2.MSYS2 --accept-package-agreements --accept-source-agreements
```

### Instalar GCC, CMake e SDL2 (apenas na primeira vez)

```bat
C:\msys64\usr\bin\pacman.exe -S --noconfirm --needed ^
  mingw-w64-x86_64-gcc ^
  mingw-w64-x86_64-cmake ^
  mingw-w64-x86_64-SDL2 ^
  mingw-w64-x86_64-make
```

---

## Como Compilar e Rodar

Navegue até `simulation/lv_port_pc_eclipse/` e:

### Primeira vez (ou após alterar o código)

Dê **duplo clique** em `COMPILAR.bat`. O script:

1. Verifica GCC/CMake em `C:\msys64\mingw64\bin\`
2. Clona o submodule LVGL via `git submodule update --init --recursive`
3. Executa `cmake -B build -S . -G "MinGW Makefiles"`
4. Compila com `cmake --build build --parallel 4`
5. Gera `bin/main.exe`

### Rodar

Dê **duplo clique** em `RODAR.bat`. A janela **480×320** abre imediatamente.

### Resumo rápido

```
1ª vez:  COMPILAR.bat  →  RODAR.bat
Demais:  RODAR.bat
Após editar código:  COMPILAR.bat  →  RODAR.bat
```

---

## Fluxo do Jogo

```
Tela Inicial
    │  [A] para iniciar
    ▼
Tutorial (8 passos)
    │  [A] para avançar cada diálogo
    ▼
Jogo (08:00 → 18:00)
    │  Resolver incidentes, mover entre salas
    ▼
Vitória (18:00 com integridade > 0)
ou Derrota (integridade chegou a 0)
    │  [R] para reiniciar
    ▼
  Tela Inicial
```

---

## Controles

| Tecla | Ação |
|---|---|
| `↑` `↓` `←` `→` | Mover o personagem pelo escritório |
| `A` ou `SPACE` | Confirmar diálogo de tutorial / Abrir e resolver incidente verde ou amarelo |
| `N` | Scan NFC — abre e resolve incidente vermelho (Ransomware) |
| `R` | Reiniciar o jogo a qualquer momento |

### Resolver um incidente

1. Mova o personagem **até ficar próximo** da bolha colorida no mapa (raio ~50 px).
2. Pressione `A` (incidentes verdes ou amarelos) ou `N` (ransomware vermelho).
3. Um **diálogo de detalhe** abre com informações da vulnerabilidade.
4. Pressione `A` ou `N` novamente para confirmar a resolução.

---

## Mecânicas do Jogo

### Mapa — Dois Andares

O jogo possui dois ambientes com layout pixel-art:

```
┌─────────────────────────────────────┐
│         Recepção (Sala 1)           │  ── porta direita ──►  Escritório
│  (prateleiras, tapete, sofá, rack)  │
└─────────────────────────────────────┘

┌─────────────────────────────────────┐
│         Escritório (Sala 2)         │  ◄── porta esquerda ── Recepção
│  (mesas, PCs, impressoras, plantas) │
└─────────────────────────────────────┘
```

**Transição entre salas:** caminhe até a **borda lateral** (indicador `► 2` ou `◄ 1`).  
O personagem aparece na porta oposta da outra sala.

---

### Incidentes (Bolhas Coloridas)

A cada 9–16 segundos, um incidente surge em ponto fixo de uma sala aleatória:

| Ícone | Cor | Tipo | Drenagem | Resolução | Pontos |
|---|---|---|---|---|---|
| `?` | Verde | Tarefa de prevenção | 1 HP/2s | `A` / `SPACE` | +15 |
| `!` | Amarelo | Anomalia suspeita | 4 HP/2s | `A` / `SPACE` | +15 |
| `X` | Vermelho | Ransomware ativo | 10 HP/2s | `N` (NFC scan) | +30 |

Máximo de **4 incidentes simultâneos** em toda a empresa.

---

### Sistema de Integridade da Rede

- Exibida no HUD como barra colorida e percentual.
- Calculada a partir dos HP dos incidentes ativos:
  - Sem incidentes ativos → **100%**
  - Com incidentes → proporcional ao HP médio restante
- Cor da barra: verde (>60%), amarelo (>30%), vermelho (≤30%).
- **Integridade = 0% → Derrota imediata.**

---

### Relógio do Jogo

- **3 minutos reais** = expediente completo das **08:00 às 18:00** (10h de jogo).
- O relógio aparece no canto superior esquerdo do HUD.
- Chegar às **18:00** com integridade > 0 = **Vitória**.

---

### Tutorial

Ao iniciar, o NPC guia apresenta **8 diálogos** explicando:

1. Boas-vindas e contexto do jogo
2. Controles de movimento
3. Incidentes verdes (prevenção)
4. Incidentes amarelos (anomalias)
5. Incidentes vermelhos (ransomware)
6. Como interagir com incidentes
7. Consequências de ignorar incidentes
8. Dica sobre explorar as duas salas

Pressione `A` para avançar cada passo.

---

## Arquitetura do Código

```
main.c
├── hal_init(480, 320)               — cria display SDL 480×320, registra driver LVGL
├── SDL_AddEventWatch(sdl_key_watch) — hook de teclado SDL → cybersec_sdl_key_event()
└── loop: lv_timer_handler() + SDL_Delay(5)

cybersec_start()                     — inicializa estado global, cria tela inicial
    │  [A]
    ▼
create_hud()                         — barra HUD: relógio, REDE: [barra], status
build_room1() / build_room2()        — layout pixel-art com lv_obj (mk_rect calls)
create_player()                      — personagem: head (skin+hair) + body (camisa)
show_tutorial_step()                 — overlay com portrait NPC + texto + indicador [A] N/8
    │  após step 8
    ▼
start_timers()
    ├── tmr_move_cb   (16 ms)   — move personagem, detecta transição de sala
    ├── tmr_clock_cb  (500 ms)  — avança relógio do jogo, checa vitória às 18:00
    ├── tmr_event_cb  (9–16 s)  — sorteia sala + tipo de incidente, cria bolha colorida
    └── tmr_drain_cb  (2 s)     — drena HP dos incidentes ativos, checa derrota

cybersec_sdl_key_event(key, pressed)
    ├── UP/DOWN/LEFT/RIGHT  → g_keys.* lidas por tmr_move_cb
    ├── A / SPACE           → avança tutorial ou abre/resolve dialog de incidente
    ├── N                   → abre/resolve dialog de ransomware (NFC)
    └── R                   → do_restart() — limpa tudo e reinicia
```

---

## Integração com Firmware ESP32 (Wokwi / Hardware Real)

O motor do jogo (`cybersec_game.c`) **não depende de SDL**, apenas de LVGL.  
Para rodar no ESP32-S3 com ILI9341:

1. Adicione `cybersec_game.h` e `cybersec_game.c` ao componente `main/` do projeto ESP-IDF.
2. No `app_main()`, após inicializar o driver LVGL:
   ```c
   cybersec_start();
   ```
3. No handler de botões/joystick (FreeRTOS task), chame:
   ```c
   cybersec_sdl_key_event(SDLK_UP, true);    // botão pressionado
   cybersec_sdl_key_event(SDLK_UP, false);   // botão solto
   ```

> Os valores `SDLK_*` são `#define` numéricos no topo de `cybersec_game.c`.  
> No ESP-IDF, mapeie os GPIOs dos botões para esses mesmos valores.

---

## Extensões Sugeridas

- [ ] Tileset real com `lv_image` (bitmap pixel-art exportado como C array)
- [ ] Animação do personagem ao caminhar (`lv_anim` no frame do sprite)
- [ ] Som via buzzer PWM no ESP32 ao detectar ransomware (`ledc_set_freq`)
- [ ] Placar persistente em NVS (ESP32) ou arquivo local (PC)
- [ ] Terceira sala — Sala de Servidores — com eventos críticos de maior frequência
- [ ] Barra de stamina do analista (uso excessivo de NFC cansa o personagem)