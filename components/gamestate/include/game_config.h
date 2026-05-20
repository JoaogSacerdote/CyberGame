#pragma once

/* Constantes parametrizaveis do gameplay. Pode ajustar sem mexer na FSM. */

/* === Tempos do expediente === */
#define EXPEDIENTE_DURACAO_MS         (3 * 60 * 1000)   /* 3 min reais = 08h-18h no jogo */
#define HORA_INICIO_JOGO_MIN          (8 * 60)          /* expediente comeca 08:00 */
#define HORA_FIM_JOGO_MIN             (18 * 60)         /* expediente termina 18:00 (hard stop) */

/* === Vidas === */
#define VIDAS_INICIAIS                3

/* === Intervalos de spawn (Etapa C+) === */
#define EVENTO_VERDE_INTERVALO_MS     (30 * 1000)
#define EVENTO_AMARELA_INTERVALO_MS   (60 * 1000)
#define EVENTO_VERMELHO_INTERVALO_MS  (90 * 1000)
#define EVENTO_COOLDOWN_GLOBAL_MS     (5 * 1000)

/* === Mecanica de carta NFC (Etapa C) === */
#define ACTION_LOCK_MS                1500
#define SYSTEM_DEPLOY_MS              4000
#define VERMELHO_TIMER_MS             20000             /* tempo ate destruir o setor */
#define VERMELHO_AGRAVADO_MULT_PCT    50                /* carta AGRAVA acelera o timer em 50% */
#define NFC_LEITURA_COOLDOWN_MS       1000              /* nao re-le mesmo UID dentro deste prazo */

/* === Pontuacao === */
#define SCORE_VERDE                   10
#define SCORE_AMARELA                 20
#define SCORE_VERMELHO_BASE           50
#define SCORE_VELOCIDADE_MAX          50                /* bonus por reflexo no vermelho */

/* === Concorrencia (Teoria do Fluxo) === */
#define MAX_VERMELHOS_SIMULTANEOS     1

/* === UI === */
#define TIMEOUT_TELA_FINAL_MS         (30 * 1000)       /* idle na tela final -> volta pra splash */
#define MENU_NAV_DEBOUNCE_MS          300               /* joystick discreto no menu */
#define JOYSTICK_DEFLEXAO_MIN         50                /* joystick e int8_t -100..+100; 50 = ~meia deflexao */

/* === Ticks === */
#define ENGINE_TICK_PERIOD_MS         100               /* ticks da FSM */

/* === Player (sprite e movimento) ===
 * Todos os personagens-player do MVP usam o mesmo sprite-sheet 32x48 (4
 * linhas DOWN/LEFT/RIGHT/UP x 3 frames). Se aparecer um personagem com
 * sheet diferente, parametrizar no screen_room. */
#define PLAYER_FRAME_W                32
#define PLAYER_FRAME_H                48

/* Animacao walk: ~8 fps (4 frames a cada 500 ms). */
#define PLAYER_WALK_PERIOD_MS         125

/* Curva de velocidade do joystick (px/tick em funcao da deflexao). */
#define PLAYER_JOY_DEADZONE           30                /* |eixo| <= 30 -> parado */
#define PLAYER_STEP_MIN_PX            2                 /* px/tick com deflexao minima */
#define PLAYER_STEP_MAX_PX            6                 /* px/tick com deflexao maxima */

/* Margem de seguranca alem de PLAYER_FRAME_W quando o spawn nasce ao lado
 * de um gatilho de porta — evita loop de troca de sala no 1o tick. */
#define SPAWN_DOOR_MARGIN_PX          16

/* === Dialogo (typewriter) === */
#define DIALOG_TYPE_PERIOD_MS         30                /* ~33 cps */
