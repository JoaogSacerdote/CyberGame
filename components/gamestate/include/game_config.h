#pragma once

/*
 * game_config.h — Constantes derivadas da calibração.
 * Não edite aqui: use calibracao.h para ajustar os valores do jogo.
 */

#include "calibracao.h"

/* ── Duração do expediente ──────────────────────────────────────────────────
 * CAL_VELOCIDADE_TEMPO 0→5min  50→3min  100→50s  */
#ifdef CYBERSIM
  #define EXPEDIENTE_DURACAO_MS     (30 * 60 * 1000)
#else
  #define EXPEDIENTE_DURACAO_MS     (300000 - (uint32_t)(CAL_VELOCIDADE_TEMPO) * 2500u)
#endif

#define HORA_INICIO_JOGO_MIN        (8  * 60)
#define HORA_FIM_JOGO_MIN           (18 * 60)

/* ── Vidas ──────────────────────────────────────────────────────────────────*/
#define VIDAS_INICIAIS              CAL_VIDAS_INICIAIS

/* ── Spawn de tarefas ───────────────────────────────────────────────────────
 * MIN = tempo mínimo de expediente antes de a tarefa poder aparecer.
 * RAND = janela aleatória adicional (aparece em MIN+random(0..RAND)).
 * GAP = intervalo mínimo entre dois spawns de tipos diferentes.          */
#ifdef CYBERSIM
  #define TAREFA_VERDE_MIN_MS       (30 * 1000)
  #define TAREFA_VERDE_RAND_MS      (30 * 1000)
  #define TAREFA_AMARELA_MIN_MS     (2 * 60 * 1000)
  #define TAREFA_AMARELA_RAND_MS    (60 * 1000)
  #define TAREFA_VERMELHO_MIN_MS    (5 * 60 * 1000)
  #define TAREFA_SPAWN_GAP_MS       (20 * 1000)
#else
  /* CAL_TAREFA_VERDE_INICIO:   0→2s  50→6s  100→10s */
  #define TAREFA_VERDE_MIN_MS       ((uint32_t)(2 + (CAL_TAREFA_VERDE_INICIO) * 8  / 100) * 1000u)
  /* Janela aleatória = mesma duração do mínimo */
  #define TAREFA_VERDE_RAND_MS      TAREFA_VERDE_MIN_MS

  /* CAL_TAREFA_AMARELA_INICIO: 0→10s  50→20s  100→30s */
  #define TAREFA_AMARELA_MIN_MS     ((uint32_t)(10 + (CAL_TAREFA_AMARELA_INICIO) * 20 / 100) * 1000u)
  #define TAREFA_AMARELA_RAND_MS    TAREFA_AMARELA_MIN_MS

  /* CAL_TAREFA_VERMELHO_INICIO: 0→20s  50→40s  100→60s */
  #define TAREFA_VERMELHO_MIN_MS    ((uint32_t)(20 + (CAL_TAREFA_VERMELHO_INICIO) * 40 / 100) * 1000u)

  #define TAREFA_SPAWN_GAP_MS       (10 * 1000u)
#endif

/* ── Ataques vermelhos ──────────────────────────────────────────────────────
 * CAL_DIFICULDADE_ATAQUES 0=fácil  50=padrão  100=brutal               */
#ifdef CYBERSIM
  #define EVENTO_VERMELHO_INTERVALO_MS  (3 * 60 * 1000)
  #define EVENTO_VERMELHO_PRIMEIRO_MS   (30 * 1000)
  #define VERMELHO_TIMER_MS             (60 * 1000)
#else
  /* Intervalo entre ataques: 0→60s  50→35s  100→10s */
  #define EVENTO_VERMELHO_INTERVALO_MS  ((uint32_t)(10 + (100 - CAL_DIFICULDADE_ATAQUES) * 50 / 100) * 1000u)
  /* Delay após o gate de 40s abrir antes do 1º ataque */
  #define EVENTO_VERMELHO_PRIMEIRO_MS   1000u
  /* Tempo do jogador para mitigar: 0→40s  50→22s  100→5s */
  #define VERMELHO_TIMER_MS             ((uint32_t)(5 + (100 - CAL_DIFICULDADE_ATAQUES) * 35 / 100) * 1000u)
#endif

#define VERMELHO_AGRAVADO_MULT_PCT      50
#define NFC_LEITURA_COOLDOWN_MS         1000u
#define EVENTO_COOLDOWN_GLOBAL_MS       (5 * 1000u)
#define MAX_VERMELHOS_SIMULTANEOS       1

/* ── Pontuação ──────────────────────────────────────────────────────────────*/
#define SCORE_VERDE                     10
#define SCORE_AMARELA                   20
#define SCORE_VERMELHO_BASE             50
#define SCORE_VELOCIDADE_MAX            50

/* ── NFC / Deploy ───────────────────────────────────────────────────────────*/
#define ACTION_LOCK_MS                  1500u
#define SYSTEM_DEPLOY_MS                4000u

/* ── UI ─────────────────────────────────────────────────────────────────────*/
#define TIMEOUT_TELA_FINAL_MS           (30 * 1000u)
#define MENU_NAV_DEBOUNCE_MS            300u

/* ── Ticks ──────────────────────────────────────────────────────────────────*/
#define ENGINE_TICK_PERIOD_MS           100u

/* ── Jogador ────────────────────────────────────────────────────────────────
 * Sprite fixo 32×48 (4 linhas × 3 frames). Não altere W/H aqui.          */
#define PLAYER_FRAME_W                  32
#define PLAYER_FRAME_H                  48

/* Velocidade de movimento: CAL_VELOCIDADE_JOGADOR 0→max=1  50→max=3  100→max=6 */
#define PLAYER_STEP_MIN_PX              (1u + (unsigned)(CAL_VELOCIDADE_JOGADOR) / 67u)
#define PLAYER_STEP_MAX_PX              (1u + (unsigned)(CAL_VELOCIDADE_JOGADOR) * 5u / 100u)

/* Deadzone do joystick: CAL_ZONA_MORTA_JOYSTICK 0→0  50→30  100→60 */
#define JOYSTICK_DEFLEXAO_MIN           50u
#define PLAYER_JOY_DEADZONE             ((unsigned)(CAL_ZONA_MORTA_JOYSTICK) * 60u / 100u)

/* Boost eixo X: CAL_BOOST_EIXO_X 0→×1,0  50→×1,5  100→×2,0 */
#define JOY_X_BOOST_PCT                 (100u + (unsigned)(CAL_BOOST_EIXO_X))

/* Animação walk: CAL_VELOCIDADE_ANIMACAO 0→200ms  50→125ms  100→50ms */
#define PLAYER_WALK_PERIOD_MS           (200u - (unsigned)(CAL_VELOCIDADE_ANIMACAO) * 150u / 100u)

#define SPAWN_DOOR_MARGIN_PX            16u

/* ── Diálogo (typewriter) ───────────────────────────────────────────────────*/
#define DIALOG_TYPE_PERIOD_MS           30u
