---
tipo: canonical
status: vigente
area: cross-cutting
canonical-for: [fsm-macro, fsm-gameplay-substate, arquitetura-componentes, mapeamento-ataque-carta-hal, tasks-freertos, topologia-salas, boot-lifecycle, pipeline-assets, ws2812-logica, z-ordering-lvgl]
ultima-atualizacao: 2026-05-27
data: 2026-05-12
gerado-por: Claude (sessao 2026-05-12)
fonte: Artigo_extracted.txt + matriz-reacao-ataques.md + RESPOSTAS.txt + project_status_hals + project_hardware
tags: [cybersec, arquitetura, diagramas, mermaid]
---

# Diagramas do Projeto CyberSec: Network Defender

> Visao 360 do projeto em Mermaid. Cada secao e um diagrama autocontido.
> Atualizado em 2026-05-12 com as decisoes finais de [[../CONSULTA/RESPOSTAS.txt|RESPOSTAS.txt]].
> Restos em [[../CONSULTA/A resolver.txt|A resolver.txt]].

## 1. FSM macro do jogo (consolidado MVP)

> Maquina de estados finitos de alto nivel. Tutorial dirigido **removido**
> (aprendizagem organica via NPC recepcionista). Calibracao NFC **removida**
> (UIDs hardcoded em `nfc_config.h`). Pause via **START (GPIO 15)**.

```mermaid
stateDiagram-v2
    [*] --> BOOT: power on
    BOOT --> RECOVERY: PWR+REC pressionados
    BOOT --> SPLASH: boot normal
    RECOVERY --> [*]: USB CDC (ja implementado)

    SPLASH --> MENU_PRINCIPAL: timeout 2s ou A
    MENU_PRINCIPAL --> GAMEPLAY: "Iniciar Expediente"
    MENU_PRINCIPAL --> RANKING_VIEW: "Ranking"
    MENU_PRINCIPAL --> CREDITOS: "Sobre"

    GAMEPLAY --> PAUSE: botao START (GPIO 15)
    PAUSE --> GAMEPLAY: B
    PAUSE --> MENU_PRINCIPAL: "Sair (perde sessao)"

    GAMEPLAY --> FIM_EXPEDIENTE: relogio 18:00 (hard stop) OU vidas=0
    FIM_EXPEDIENTE --> TELA_RESULTADO: animacao
    TELA_RESULTADO --> CADASTRO_NICK: "salvar score" (A)
    TELA_RESULTADO --> RANKING_VIEW: "anonimo" (B)
    CADASTRO_NICK --> RANKING_VIEW: nick confirmado
    RANKING_VIEW --> MENU_PRINCIPAL: B ou timeout 30s

    CREDITOS --> MENU_PRINCIPAL: B
```

## 2. Loop de GAMEPLAY (sub-FSM) — consolidado MVP

> Estados internos do GAMEPLAY. Terminal aberto via **botao Y** quando proximo
> de equipamento interagivel. Toda tarefa exige terminal aberto. B fecha
> terminal (e aborta deploy se em curso).

```mermaid
stateDiagram-v2
    [*] --> EXPLORANDO

    EXPLORANDO --> DIALOGO_NPC: A perto de NPC
    DIALOGO_NPC --> EXPLORANDO: B ou texto acabou

    EXPLORANDO --> TERMINAL_ABERTO: Y perto de equipamento
    TERMINAL_ABERTO --> EXPLORANDO: B (fecha terminal)

    TERMINAL_ABERTO --> TAREFA_VERDE: terminal tem tarefa verde
    TERMINAL_ABERTO --> TAREFA_AMARELA: terminal tem tarefa amarela
    TERMINAL_ABERTO --> STATE_WAITING_CARD: terminal sob ataque vermelho

    TAREFA_VERDE --> ACTION_LOCK_OK: jogador acerta
    TAREFA_VERDE --> ACTION_LOCK_FAIL: timeout / errou
    TAREFA_AMARELA --> ACTION_LOCK_OK: QTE Simon Says completo
    TAREFA_AMARELA --> ACTION_LOCK_FAIL: QTE falho

    STATE_WAITING_CARD --> ACTION_LOCK: carta lida (qualquer UID)
    STATE_WAITING_CARD --> TERMINAL_ABERTO: B aborta
    ACTION_LOCK --> SYSTEM_DEPLOY: lock 1-2s expira
    SYSTEM_DEPLOY --> DEPLOY_OK: barra 100% E carta CORRETA
    SYSTEM_DEPLOY --> DEPLOY_FAIL: B durante deploy (zera progresso)
    SYSTEM_DEPLOY --> DEPLOY_AGRAVA: barra 100% E carta AGRAVA

    DEPLOY_OK --> TERMINAL_ABERTO: ataque mitigado +pontos
    DEPLOY_FAIL --> TERMINAL_ABERTO: ataque continua
    DEPLOY_AGRAVA --> TERMINAL_ABERTO: cronometro ataque acelera

    ACTION_LOCK_OK --> TERMINAL_ABERTO: +score
    ACTION_LOCK_FAIL --> TERMINAL_ABERTO: -score
```

## 3. Arquitetura de componentes / HALs

> Mapa de dependencias atual (apos commit `db39883`). Setas indicam "depende
> de" / "usa". Componentes em **negrito** ainda nao existem.

```mermaid
graph TD
    main[main.c] --> pmu
    main --> button_hal
    main --> joystick_hal
    main --> nfc_hal
    main --> display_hal
    main --> storage_hal
    main --> ui_debug

    display_hal --> hal_bridge
    hal_bridge --> LVGL["LVGL 9.3<br/>via lv_timer"]
    LVGL --> ui_debug

    storage_hal --> asset_store
    asset_store -.->|Etapa 2: write API<br/>ainda nao existe| asset_store

    pmu -.->|deep sleep<br/>ext0 wake| Hardware

    nfc_hal --> hal_common
    button_hal --> hal_common
    hal_common -->|isr_service_install_once| Hardware

    main --> recovery
    recovery -->|USB CDC<br/>PING/PONG| Hardware

    button_hal -.->|input_queue<br/>(futura)| game_logic
    nfc_hal -.->|nfc_queue<br/>(futura)| game_logic
    joystick_hal -.->|polling<br/>(futura)| game_logic
    asset_store -.->|read API<br/>(futura)| game_logic
    game_logic -.->|ui_cmd_queue<br/>(futura)| hal_bridge
    game_logic -.->|audio_cmd_queue<br/>(futura)| feedback_hal

    game_logic[**game_logic**<br/>EM IMPLEMENTACAO<br/>FSM + tasks + queues<br/>+ nfc_config.h hardcoded]:::wip
    feedback_hal[**feedback_hal**<br/>buzzer + 3x WS2812<br/>EM IMPLEMENTACAO]:::wip

    classDef wip fill:#fff2cc,stroke:#996600,stroke-width:2px,color:#663300
```

## 4. Mapeamento Ataque -> Carta NFC -> HAL

> Versao em grafo da tabela mestre em [[matriz-reacao-ataques]]. Sub-grafos
> agrupam por **pilar CIA** (Confidencialidade / Integridade / Disponibilidade).

```mermaid
graph LR
    subgraph Disponibilidade
        DDoS["DDoS<br/>(Vermelho)"]
        Balanc["Carta: Balanceamento<br/>de Rede"]
        DDoS -->|neutraliza com| Balanc
    end

    subgraph Integridade_R[Integridade - Ransomware]
        Ransom["Ransomware<br/>(Vermelho)"]
        Backup["Carta: Backup<br/>de Emergencia"]
        Ransom -->|neutraliza com| Backup
    end

    subgraph Integridade_P[Integridade - Propagacao]
        Prop["Propagacao Lateral<br/>(Amarelo)"]
        Isol["Carta: Isolamento<br/>de Rede"]
        Prop -->|neutraliza com| Isol
    end

    subgraph Confidencialidade_F[Confidencialidade futura]
        Phish["Phishing<br/>(extensao futura)"]
        FutCard["?"]
        Phish -.->|sem carta definida| FutCard
    end

    Balanc --> PN532[driver-pn532]
    Backup --> PN532
    Isol --> PN532
    PN532 --> Display[driver-st7796s]
    PN532 --> Buzzer[driver-buzzer<br/>GPIO 45]
    Display --> WS2812[driver-ws2812<br/>GPIO 48]
```

## 5. Tasks FreeRTOS + IPC (consolidado MVP — 3 tasks)

> **Atualizado 2026-05-12.** 3 tasks (nao 4 como o artigo) com **core affinity
> obrigatoria**. Audio NAO tem task propria — o GameEngine empurra SoundID na
> audio_queue e a Task Hardware consome (PWM/LEDC nao-bloqueante).

```mermaid
graph TD
    subgraph ISRs[ISRs - sem afinidade]
        BTN_ISR["Button ISR<br/>GPIO 11-14, 3*<br/>*GPIO 3 dual-use REC/START"]
        NFC_IRQ["NFC IRQ<br/>GPIO 16"]
        PWR_ISR["PWR Btn ISR<br/>GPIO 4"]
    end

    subgraph Core1[CORE 1 EXCLUSIVO]
        LVGLTask["Task UI/LVGL<br/>prio: ALTA<br/>dirty areas + DMA<br/>Z-ordering 5 camadas"]
    end

    subgraph Core0[CORE 0]
        GameTask["Task GameEngine<br/>prio: MEDIA<br/>FSM + timers expediente<br/>matriz carta x ataque<br/>scoring"]
        HWTask["Task Hardware<br/>prio: ALTA<br/>dormida; acorda em ISR<br/>ou event flag NFC<br/>ou audio_queue"]
    end

    subgraph Queues
        input_q[input_queue<br/>enum ButtonEvent]
        nfc_q[nfc_queue<br/>uint32_t UID]
        ui_cmd_q[ui_command_queue<br/>struct UI_Cmd]
        audio_q[audio_queue<br/>enum SoundID]
    end

    subgraph EventFlags
        nfc_flag["NFC_LISTEN_ENABLE<br/>(event group bit)"]
    end

    BTN_ISR -->|xQueueSendFromISR| input_q
    PWR_ISR -->|xQueueSendFromISR| input_q
    NFC_IRQ -->|notify task| HWTask

    input_q --> GameTask
    HWTask -->|UID lido| nfc_q
    nfc_q --> GameTask

    GameTask -->|mutex LVGL| ui_cmd_q
    GameTask --> audio_q
    GameTask -->|seta/limpa| nfc_flag

    nfc_flag -->|"on entry STATE_WAITING_CARD<br/>energiza PN532;<br/>caso contrario corta RF"| HWTask

    ui_cmd_q --> LVGLTask
    audio_q --> HWTask

    LVGLTask -->|SPI flush| ST7796[Display ST7796S]
    HWTask -->|LEDC PWM| BuzzerHW[Buzzer GPIO 45<br/>piezo direto]
    HWTask -->|RMT| WS2812HW[3x WS2812 GPIO 48]
    HWTask -->|I2C| PN532HW[PN532 GPIO 5/6/16]
```

> [!note] Mutex LVGL e cross-core
> Apesar do LVGL rodar em Core 1 exclusivo, a Task GameEngine (Core 0) precisa publicar
> comandos de UI. **O mutex LVGL e a unica forma autorizada** de mexer em objetos LVGL
> de outra task. Toda escrita em arvore LVGL fora do Core 1 deve segurar o mutex (regra
> reforcada em `feedback_lvgl_diff_gating`).

## 6. Topologia de salas (proposta)

> Decisao 3.1 e 3.2 do DUVIDAS.TXT. Esta e uma **proposta minha** baseada nos
> setores citados em [[matriz-reacao-ataques]] (servidores, financeiro,
> recepcao) — confirma com o time antes de produzir os assets.

```mermaid
graph LR
    S1["Sala 1<br/>Recepcao<br/>(JA EXISTE)"]
    S2["Sala 2<br/>Escritorios<br/>(JA EXISTE)"]
    S3["Sala 3<br/>Servidores<br/>(falta arte)"]
    S4["Sala 4<br/>Financeiro<br/>(falta arte)"]
    S5["Sala 5<br/>RH/Reuniao<br/>(opcional)"]
    Corredor["Corredor<br/>central"]

    S1 <-->|porta direita| Corredor
    Corredor <-->|porta cima| S2
    Corredor <-->|porta direita| S3
    Corredor <-->|porta baixo| S4
    Corredor <-.->|opcional| S5
```

> [!todo] Decisoes pendentes
> - Numero exato de salas — `S5` e opcional?
> - Topologia: corredor central (proposto aqui), grade 2x2, mapa do predio
>   selecionavel?
> - Cada sala associada a 1 pilar CIA? (Servidores = Disponibilidade,
>   Financeiro = Integridade, Recepcao = Confidencialidade...)

## 7. Lifecycle de boot / power

> Como o sistema decide entre boot normal e recovery mode. Ja implementado
> ate o ponto NORMAL. Recovery completo (commit `9b4a767`).

```mermaid
flowchart TD
    Power[Power on / Wake] --> ReadBtns{PMU le<br/>combo<br/>PWR + REC}
    ReadBtns -->|so PWR| Normal[Boot NORMAL]
    ReadBtns -->|PWR+REC| Recovery[Boot RECOVERY<br/>USB CDC<br/>PING/PONG]
    Recovery --> WaitHost[Aguarda comandos<br/>do PC]
    Normal --> InitHALs[Init HALs<br/>pmu, btn, joy, nfc, disp]
    InitHALs --> SplashScreen[SPLASH]
    SplashScreen --> Menu[MENU_PRINCIPAL]

    Menu -.->|inatividade<br/>30s| AttractMode["attract mode<br/>(idea abstrata 18.6<br/>do DUVIDAS)"]
    AttractMode -.->|qualquer input| Menu

    Menu --> Sleep[DEEP_SLEEP<br/>ext0 wake em PWR]
    Sleep -->|PWR pressionado| Power
```

## 8. Pipeline de assets (proposto)

> Como a arte (Aseprite) vira bytes na NAND e e exibida na tela. Depende da
> Etapa 2 do `asset_store` (write API ainda nao existe).

```mermaid
flowchart LR
    Aseprite[Aseprite<br/>.aseprite] -->|export PNG| PNG[Sprites PNG]
    PNG -->|lvgl-image-converter<br/>OU<br/>script proprio| Bin["Asset binario<br/>RGB565 / indexed"]
    Bin -->|recovery USB CDC<br/>PUT category id| asset_store_write["asset_store<br/>write API<br/>(Etapa 2)"]
    asset_store_write --> NAND[(NAND W25N01GV<br/>128 MB)]
    NAND --> asset_store_read[asset_store<br/>read API<br/>JA EXISTE]
    asset_store_read --> game_logic
    game_logic -->|lv_img_set_src| LVGL
    LVGL --> ST7796[Display ST7796S]
```

## 9. Logica de acionamento dos 3 WS2812

> Decidida em RESPOSTAS.txt — NOTA DE CONSOLIDACAO. **3 LEDs no mesmo barramento
> RMT (GPIO 48)**. LEDs 1 e 2 sao indicadores passivos de tarefas pendentes;
> LED 3 e exclusivo do ataque e arrasta os outros conforme o ataque escala.

```mermaid
graph TD
    GameEngine[Task GameEngine<br/>tick a cada 100ms]

    GameEngine -->|"se ha tarefa verde<br/>pendente?"| L1Logic
    GameEngine -->|"se ha tarefa amarela<br/>pendente?"| L2Logic
    GameEngine -->|"ataque vermelho ativo?<br/>qual % do timer?"| L3Logic

    L1Logic["LED 1 (verde)<br/>aceso solido se sim,<br/>apagado se nao"]
    L2Logic["LED 2 (amarelo)<br/>aceso solido se sim,<br/>apagado se nao<br/><br/>OVERRIDE: se ataque > 33%,<br/>vira vermelho (escalada)"]
    L3Logic["LED 3 (vermelho)<br/>0-33%: solido<br/>33-66%: solido + LED 2 vermelho<br/>>66%: 3 LEDs piscam sincronos<br/>>90%: 3 LEDs caoticos"]

    L1Logic --> RMT[Buffer RMT 24 bits]
    L2Logic --> RMT
    L3Logic --> RMT
    RMT -->|GPIO 48| Strip[3x WS2812]
```

## 10. Z-Ordering (5 camadas LVGL)

> Decidido em NOTA DE CONSOLIDACAO. Sem buffers separados — apenas ordem na
> arvore parent/child. HUD/Terminal forcado via `lv_obj_move_foreground()`.

```mermaid
graph TD
    Z4["Camada 4 — UI/HUD/Terminal<br/>(lv_obj_move_foreground)<br/>relogio, vidas, dashboard,<br/>tela de terminal"]:::z4
    Z3["Camada 3 — Foreground/Teto<br/>(sem colisao)<br/>molduras superiores, vigas"]:::z3
    Z2["Camada 2 — Entidades dinamicas<br/>(colisao dinamica vs Z1)<br/>personagem, NPCs, alertas"]:::z2
    Z1["Camada 1 — Midground/Obstaculos<br/>(colisao fisica)<br/>mesas, paredes, servidores"]:::z1
    Z0["Camada 0 — Background/Chao<br/>(sem colisao)<br/>texturas, carpetes"]:::z0

    Z4 -.->|sobrepoe| Z3
    Z3 -.->|sobrepoe| Z2
    Z2 -.->|sobrepoe| Z1
    Z1 -.->|sobrepoe| Z0

    classDef z0 fill:#e8e8e8,stroke:#999
    classDef z1 fill:#c8d8e8,stroke:#369
    classDef z2 fill:#f8d8c8,stroke:#963
    classDef z3 fill:#d8c8e8,stroke:#639
    classDef z4 fill:#f8f8c8,stroke:#990,stroke-width:3px
```

## Referencias cruzadas

- [[matriz-reacao-ataques]] — fonte primaria do mapeamento ataque/carta/HAL
- [[../CONSULTA/DUVIDAS.TXT|DUVIDAS.TXT]] — duvidas que afetam estes diagramas
- [[../CONSULTA/Artigo.pdf]] — fonte pedagogica e tecnica primaria
- Memoria `project_status_hals` (snapshot 2026-05-11) — o que ja esta commitado
- Memoria `project_hardware` — pinout definitivo
