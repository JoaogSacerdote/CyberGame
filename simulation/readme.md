# CyberSec: Network Defender — Simulador LVGL (PC)

Simulador de PC do jogo embarcado **CyberSec**, escrito em C usando **LVGL 9.x**  
(com camada de compatibilidade LVGL 8) renderizado via **SDL2**.  
Permite rodar e testar o jogo completo no Windows sem nenhum hardware real.

- Janela: **480 × 320 px** (equivalente ao display ILI9341 do ESP32-S3)
- Build: **CMake + MinGW GCC** via MSYS2
- Dependências externas: apenas SDL2 (já gerenciado pelo `COMPILAR.bat`)

---

## Estrutura do Projeto

```
simulation/
├── readme.md                       ← este arquivo
├── main/                           ← código-fonte canônico (espelhado do firmware)
│   └── src/
│       ├── cybersec_game.h         ← API pública do jogo
│       └── cybersec_game.c         ← Motor completo (UI, FSM, lógica)
└── lv_port_pc_eclipse/             ← projeto CMake para PC
    ├── main.c                      ← entry point: inicializa SDL + LVGL, chama cybersec_start()
    ├── cybersec_game.h             ← cópia de main/src/ (copiada pelo COMPILAR.bat)
    ├── cybersec_game.c             ← cópia de main/src/ (copiada pelo COMPILAR.bat)
    ├── lv_conf.h                   ← configuração LVGL (fontes, cores, resolução)
    ├── CMakeLists.txt              ← build system
    ├── COMPILAR.bat                ← instala submodule, copia fontes, compila
    ├── RODAR.bat                   ← inicia o executável com DLLs corretas
    ├── lvgl/                       ← submodule LVGL 9.x (clonado pelo COMPILAR.bat)
    └── bin/
        ├── main.exe                ← executável final (gerado após build)
        ├── SDL2.dll                ← copiada pelo RODAR.bat se ausente
        ├── libgcc_s_seh-1.dll      ← copiada pelo RODAR.bat se ausente
        └── libstdc++-6.dll         ← copiada pelo RODAR.bat se ausente
```

---

## Pré-requisitos

| Ferramenta | Versão mínima | Observação |
|---|---|---|
| Windows | 10 / 11 | — |
| Git | qualquer | Para clonar o submodule LVGL |
| MSYS2 (GCC + CMake + SDL2) | — | Instalado automaticamente — veja abaixo |

> Não é necessário instalar nada manualmente além do MSYS2. O `COMPILAR.bat` cuida do resto.

---

## Fluxo Completo — Passo a Passo

### Passo 1 — Instalar MSYS2 (apenas na primeira vez)

Abra o **PowerShell** ou **CMD** como usuário normal e execute:

```bat
winget install --id MSYS2.MSYS2 --accept-package-agreements --accept-source-agreements
```

Aguarde a instalação terminar (instala em `C:\msys64\`).

---

### Passo 2 — Instalar GCC, CMake e SDL2 (apenas na primeira vez)

Ainda no PowerShell/CMD, execute:

```bat
C:\msys64\usr\bin\pacman.exe -S --noconfirm --needed mingw-w64-x86_64-gcc mingw-w64-x86_64-cmake mingw-w64-x86_64-SDL2 mingw-w64-x86_64-make
```

Aguarde o pacman baixar e instalar os pacotes (~300 MB).

---

### Passo 3 — Compilar o jogo (apenas na primeira vez ou após alterar o código)

Navegue até a pasta `simulation/lv_port_pc_eclipse/` e dê **duplo clique** em:

```
COMPILAR.bat
```

O script faz automaticamente:

1. Verifica se o GCC/CMake estão em `C:\msys64\mingw64\bin\`
2. Inicializa o submodule LVGL via `git submodule update --init --recursive`
3. Copia `cybersec_game.h` e `cybersec_game.c` de `main/src/` para a pasta local
4. Executa `cmake -B build -S . -G "MinGW Makefiles"`
5. Executa `cmake --build build --parallel 4`
6. Gera `bin/main.exe`

Uma janela CMD ficará aberta mostrando o progresso. Ao final exibe:
```
Compilacao concluida com sucesso!
Execute RODAR.bat para iniciar o jogo.
```

---

### Passo 4 — Rodar o jogo

Dê **duplo clique** em:

```
RODAR.bat
```

O script faz automaticamente:

1. Verifica se `bin\main.exe` existe (caso contrário orienta executar `COMPILAR.bat`)
2. Copia `SDL2.dll`, `libgcc_s_seh-1.dll` e `libstdc++-6.dll` para `bin\` se ausentes
3. Define `PATH=C:\msys64\mingw64\bin;%PATH%` para resolução de DLLs em runtime
4. Inicia `bin\main.exe` com diretório de trabalho em `bin\`

A janela do jogo (480×320) abre imediatamente.

---

### Resumo rápido

```
1ª vez na máquina:
  winget install MSYS2  →  pacman instala GCC/CMake/SDL2  →  COMPILAR.bat  →  RODAR.bat

Próximas vezes (sem alterar código):
  RODAR.bat  (duplo clique — pronto)

Após alterar cybersec_game.c / main.c:
  COMPILAR.bat  →  RODAR.bat
```

---

## Controles

| Tecla | Ação |
|---|---|
| `↑` `↓` `←` `→` | Mover o personagem pelo escritório |
| `SPACE` ou `ENTER` | Resolver evento da sala atual (TAREFA ou ANOMALIA) |
| `N` | Scan NFC — resolve RANSOMWARE e recupera +30 HP |
| `R` | Reiniciar o jogo do zero |

---

## Mecânicas Implementadas

### Mapa Top-Down (480 × 320)

Quatro salas num layout 2×2 com corredor central:

```
┌──────────────────┬──────────────────┐
│    Recepção      │   R. Humanos     │
│                  │                  │
├──────────────────┼──────────────────┤  ← corredor (jogador transita aqui)
│    Financeiro    │   Servidores     │
│                  │                  │
└──────────────────┴──────────────────┘
```

O jogador aparece no corredor central ao iniciar. Cada sala é uma área interativa —
entrar na sala e pressionar `SPACE` resolve o evento ativo.

---

### FSM de Eventos

A cada 7–14 segundos, uma sala aleatória recebe um evento:

```
40% → TAREFA     (verde)   — drenagem  2 HP/2s  — resolve com SPACE/ENTER
30% → ANOMALIA   (amarelo) — drenagem  5 HP/2s  — resolve com SPACE/ENTER
30% → RANSOMWARE (vermelho)— drenagem 12 HP/2s  — requer NFC (tecla N)
```

Sem resolução, o HP da sala cai até 0 e penaliza a integridade da rede.

---

### Sistema de HP

- Cada sala tem HP de 0 a 100 (inicia em 100).
- Resolver um evento recupera **+15 HP** (SPACE) ou **+30 HP** (NFC).
- **Integridade da rede** exibida no HUD = média aritmética das 4 salas.
- HP de qualquer sala chegando a 0 não encerra o jogo, mas penaliza a integridade.

---

### Relógio do Jogo

- **3 minutos reais** = expediente completo das **08:00 às 18:00** (10h de jogo).
- Sobreviver até 18:00 com integridade > 0 = **vitória**.
- Integridade chegando a 0 antes das 18:00 = **derrota**.

---

### Mecânica NFC

Ao pressionar `N` numa sala com RANSOMWARE ativo:
- Um dialog confirma a autenticação do "Cartão de Backup" (MFA).
- A sala recupera **+30 HP** (o dobro da resolução normal).
- Conceito ensinado: **autenticação multifator** e **recuperação de desastre**.

---

## Arquitetura do Código

```
main.c
├── hal_init(480, 320)              — cria display SDL 480×320, registra driver LVGL
├── SDL_AddEventWatch(sdl_key_watch)— hook de teclado SDL → cybersec_sdl_key_event()
└── loop: lv_timer_handler() + SDL_Delay(5)

cybersec_start()
├── create_hud()        — HUD superior: relógio, barra de integridade da rede, label de status
├── create_map()        — 4 salas (lv_obj) com label de nome e barra de HP individuais
├── create_player()     — ícone "@" do personagem (lv_label posicionado por pixel)
└── start_timers()
    ├── tmr_move_cb()   — 16 ms (~62 fps): lê keys.up/down/left/right, move player
    ├── tmr_clock_cb()  — 500 ms: avança relógio do jogo, verifica condição de vitória
    ├── tmr_event_cb()  — 7–14 s: sorteia sala e tipo de evento (FSM), atualiza cor da sala
    └── tmr_drain_cb()  — 2 s: drena HP das salas com evento ativo

cybersec_sdl_key_event(key, pressed)
├── SDLK_UP/DOWN/LEFT/RIGHT → seta flags keys.* lidas por tmr_move_cb
├── SDLK_SPACE / SDLK_RETURN → chama resolve_room() na sala atual
├── SDLK_n                   → chama resolve_room() com flag NFC=true
└── SDLK_r                   → reinicia o jogo (recria toda a UI)
```

---

## Integração com Firmware ESP32 (Wokwi / Hardware Real)

O motor do jogo (`cybersec_game.c`) é **portável**: não depende de SDL, apenas de LVGL.  
Para rodar no ESP32-S3 com ILI9341:

1. Adicione `cybersec_game.h` e `cybersec_game.c` ao componente `main/` do projeto ESP-IDF.
2. No `app_main()`, após inicializar o driver LVGL:
   ```c
   cybersec_start();
   ```
3. No handler de botões/joystick (FreeRTOS task), chame:
   ```c
   cybersec_sdl_key_event(SDLK_UP, true);   // botão pressionado
   cybersec_sdl_key_event(SDLK_UP, false);  // botão solto
   ```

> **Atenção**: no ESP-IDF não existe SDL. Defina um enum local com os mesmos valores  
> de `SDLK_*` usados no jogo, ou inclua somente as constantes necessárias como `#define`.

---

## Extensões Sugeridas

- [ ] Sprite de personagem (LVGL `lv_image` com bitmap em C array)
- [ ] Animação de "digitação" ao resolver evento (`lv_anim`)
- [ ] Som via buzzer PWM no ESP32 (`ledc_set_freq()`)
- [ ] Placar persistente em NVS (ESP32) ou arquivo local (PC)
- [ ] Modo multiplayer: 2 analistas, 2 joysticks
- [ ] Geração procedural de mapa por andar