---
tipo: canonical
status: vigente
area: game-design
canonical-for: [matriz-ataque-carta, matriz-carta-ataque-3x3, pilares-cia, scoring-mvp, balanceamento-mvp, tasks-3-freertos]
ultima-atualizacao: 2026-05-27
data: 2026-05-08
tags: [cybersec, arquitetura, fsm, fase2]
---

# Matriz Ataque <-> Contramedida <-> Hardware

> Tradutor canonico entre os tres pilares do projeto: a **ameaca cibernetica simulada**, a **carta NFC fisica** que a neutraliza e o **driver de HAL** que torna o feedback visivel/audivel ao jogador. Toda a FSM em `components/game_logic/` deve consultar esta matriz como fonte da verdade.

## Pilar pedagogico (CONSULTA/Artigo.pdf, Secao 3.2.1, Tabela 1)

> "Foram concebidas tres cartas de defesa, descritas a seguir e sintetizadas na Tabela 1." - Artigo, p. 9

As tres cartas materializam o conceito de **autenticacao por fator de posse** (Stallings 2017): a identidade da acao defensiva e verificada pela apresentacao do objeto fisico ao sensor PN532.

## Tabela mestre (MVP — 3 ataques)

> Trio ratificado em 2026-05-12 (RESPOSTAS.txt + sub-dialogo). **Phishing adiado** para pos-MVP.

| Ataque | Severidade | Sala mais afetada | Carta NFC | Conceito ensinado | Driver HAL critico | Pilar CIA |
|---|---|---|---|---|---|---|
| [[ataque-ddos]] | Vermelho (urgente) | [[sala-servidores]] | [[carta-balanceamento-rede]] | Load balancing / disponibilidade | [[driver-st7796s]] (animacao de chamas/sobrecarga) + [[driver-buzzer]] (alarme) | **Disponibilidade** |
| [[ataque-ransomware]] | Vermelho (critico) | [[sala-financeiro]] | [[carta-backup-emergencia]] | Disaster recovery / importancia do backup | [[driver-pn532]] (leitura UID da carta) + [[driver-st7796s]] (tela de criptografia) | **Integridade** |
| [[ataque-propagacao-lateral]] | Vermelho | Multi-sala (origem variavel) | [[carta-isolamento-rede]] | Network segmentation / contencao | [[driver-st7796s]] (animacao de propagacao entre setores) | **Integridade** |
| [[ataque-phishing]] *(pos-MVP)* | (TBD) | [[sala-recepcao]] | (TBD) | Engenharia social / autenticacao | [[driver-st7796s]] (tela de email suspeito) | **Confidencialidade** |

## Matriz CARTA x ATAQUE (efeito de aplicar carta erradamente)

> **Default proposto pelo Claude em 2026-05-12 — ratificar com equipe.** O exemplo
> de "Backup em DDoS sobrecarrega o servidor" veio do usuario; os outros pares foram
> deduzidos pelo principio de "ataque agravado quando a contramedida amplifica o vetor".

|  | DDoS | Ransomware | Propagacao Lateral |
|---|---|---|---|
| **Isolamento** | inutil (so perde tempo) | inutil (dados ja criptografados) | ✅ CORRETO |
| **Backup** | ❌ AGRAVA — sobrecarrega o servidor com trafego extra de restauracao | ✅ CORRETO | inutil |
| **Balanceamento** | ✅ CORRETO | ❌ AGRAVA — replica a criptografia para mais nodes | inutil |

> [!note] Semantica das 3 categorias
> - **✅ CORRETO** -> mitiga o ataque conforme System Deploy (3-5s). Pontuacao completa + bonus de velocidade.
> - **inutil** -> Action Lock + System Deploy completos sem efeito mecanico; jogador apenas perdeu tempo precioso dos 3 min.
> - **❌ AGRAVA** -> alem de perder o tempo, o cronometro do ataque acelera ou aplica penalidade especifica (definir constante por par em `game_config.h`).

> [!note] Pilares CIA
> Confidencialidade, Integridade, Disponibilidade - tripe classico de Stallings 2017 citado em `CONSULTA/Artigo.pdf` Secao 2.3. A FSM deve etiquetar cada evento ativo com seu pilar para fins de telemetria pedagogica na mostra academica.

## Mapeamento detalhado por ataque

### 1. DDoS -> Carta de Balanceamento de Rede

**Definicao tecnica** (`CONSULTA/Artigo.pdf` Secao 2.3): ataque de Negacao de Servico Distribuido sobrecarrega um sistema com volume excessivo de requisicoes ate torna-lo inoperante (Stallings 2017).

**Cadeia de hardware necessaria**:
- [[driver-st7796s]] precisa renderizar **animacao de chamas/sobrecarga** sobre o icone do servidor afetado. Framebuffer 480x320 RGB565 mora em **PSRAM Octal 80 MHz** (memoria `project_hardware`); a animacao consome aprox. 307 KB por frame.
- [[driver-buzzer]] dispara alarme PWM via LEDC (GPIO 45).
- [[driver-pn532]] aguarda UID da [[carta-balanceamento-rede]] na fila I2C; FSM compara com tabela de UIDs registrados em NVS.
- [[driver-ws2812]] muda o LED de status para vermelho intenso via RMT (GPIO 48).

**Estado da FSM** (em `components/game_logic/fsm_core.c`, a criar): `STATE_ATTACK_DDOS_ACTIVE` -> espera UID -> compara -> `STATE_DEFENSE_SUCCESS` ou `STATE_DEFENSE_WRONG`.

### 2. Ransomware -> Carta de Backup de Emergencia

**Definicao tecnica** (`CONSULTA/Artigo.pdf` Secao 2.3): malware que criptografa os dados da vitima e exige resgate financeiro para sua recuperacao (Kim & Solomon 2018).

**Cadeia de hardware necessaria**:
- [[driver-pn532]] e a estrela aqui: a mecanica de leitura de UID via I2C concretiza fisicamente o conceito de **autenticacao por fator de posse** (Stallings 2017, citado no Artigo Secao 2.7). A carta correta restaura o setor; a errada penaliza.
- [[driver-st7796s]] renderiza a tela "ARQUIVOS SEQUESTRADOS" com efeito de criptografia progressiva (animacao de pixels embaralhados).
- [[driver-buzzer]] toca tom de urgencia maxima.

**Conexao com pilar Integridade**: o ransomware viola integridade dos dados. O backup restaura o estado conhecido-bom anterior. **Pedagogia**: ensinar que **prevencao (backup regular)** > **reacao (pagar resgate)**.

### 3. Propagacao lateral -> Carta de Isolamento de Rede

**Definicao tecnica** (`CONSULTA/Artigo.pdf` Secao 2.3): network segmentation divide a infraestrutura em segmentos independentes, de modo que o comprometimento de um setor nao se propague (Stallings 2017).

**Cadeia de hardware necessaria**:
- [[driver-st7796s]] precisa renderizar animacao de **propagacao** entre os icones de salas no mapa top-down (`CONSULTA/Artigo.pdf` Secao 3.2 - "mapa top-down que simula uma empresa moderna dividida em setores estrategicos").
- LVGL animation engine opera sobre framebuffer PSRAM.
- Carta correta interrompe animacao de propagacao na sala adjacente.

## Tarefas concorrentes (consolidado MVP — 3 tasks)

> **Sobrescreve o "4 tasks" do artigo.** Ratificado em RESPOSTAS.txt 2026-05-12.
> Audio nao tem task propria: o GameEngine empurra SoundID na audio_queue e a Task Hardware
> consome (PWM curto e nao-bloqueante via LEDC).

| Task | Prioridade | Core afinidade | Responsabilidade |
|---|---|---|---|
| Task UI/LVGL | Alta | **Core 1 exclusivo** | dirty areas + DMA + render LVGL; nao calcula gameplay |
| Task GameEngine | Media | Core 0 | FSM, timers do expediente, scoring, matriz de carta x ataque |
| Task Hardware | Alta | Core 0 | dormida; acorda por ISR (botoes/PWR) ou event flag (NFC) ou audio_queue |

A camada [[hal-bridge]] e o **lugar canonico** para a traducao "UID lido via I2C" -> "evento de carta para a FSM".

**Gating do PN532**: GameEngine seta event flag `NFC_LISTEN_ENABLE` quando entra em
STATE_WAITING_CARD. Hardware energiza RF field; em qualquer outro estado o leitor fica desligado.

## Regras de balanceamento (defaults MVP — ajustaveis em `game_config.h`)

Ratificadas em 2026-05-12. Constantes propostas (mudaveis sem recompilar a FSM):

```c
// game_config.h — defaults propostos
#define EXPEDIENTE_DURACAO_MS       (3 * 60 * 1000)  // 3 min reais = 08h-18h
#define EVENTO_VERDE_INTERVALO_MS   (30 * 1000)
#define EVENTO_AMARELO_INTERVALO_MS (60 * 1000)
#define EVENTO_VERMELHO_INTERVALO_MS (90 * 1000)
#define ACTION_LOCK_MS              1500
#define SYSTEM_DEPLOY_MS            4000
#define VERMELHO_TIMER_MS           20000   // tempo ate destruir o setor
#define VERMELHO_AGRAVADO_MULT      0.5     // tempo restante cai pela metade se aplicar carta agrava
#define SCORE_VERDE                 10
#define SCORE_AMARELA               20
#define SCORE_VERMELHO_BASE         50
#define SCORE_VELOCIDADE_MAX        50      // bonus extra por reflexo
```

> [!todo] Restos para `A resolver.txt`
> - Quantidade final de ataques vermelhos por sessao (= vidas).
> - Penalidade exata por carta AGRAVA (subtrai tempo? sobe %? multiplicador?).
> - Conteudo do Dashboard do Analista (botao X).
> - Tela final de vitoria/derrota — visual e QR code.

## Referencias consultadas

- `CONSULTA/Artigo.pdf` Secao 2.3 (Ciberseguranca e Letramento Digital) - definicoes formais de DDoS, ransomware, network segmentation
- `CONSULTA/Artigo.pdf` Secao 2.7 (Comunicacao NFC e Autenticacao) - fundamento do fator de posse via PN532
- `CONSULTA/Artigo.pdf` Secao 3.2.1 (Cartas de Defesa NFC) - **fonte primaria** desta matriz
- `CONSULTA/Artigo.pdf` Secao 3.4 (Arquitetura de Software) - 4 tarefas FreeRTOS
- Memoria local `project_hardware` - pinout e PSRAM Octal habilitada
