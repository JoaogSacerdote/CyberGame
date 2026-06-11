#pragma once

/* =============================================================================
 *   calibracao.h  —  Calibração Global do CyberGame
 *
 *   COMO USAR:
 *     1. Altere os números abaixo (sempre entre 0 e 100).
 *     2. Salve o arquivo.
 *     3. Recompile:  python idf.py build flash monitor
 *
 *   ESCALA:
 *     0 = mínimo / mais devagar / mais fácil / apagado
 *    50 = padrão recomendado  ← use como ponto de partida
 *   100 = máximo / mais rápido / mais difícil / brilho total
 *
 *   As fórmulas que convertem esses valores estão em game_config.h e
 *   display_hal.h — você não precisa mexer neles.
 * =============================================================================*/


/* ============================================================================
 *  JOGADOR
 *  Controle do personagem no mapa.
 * ============================================================================*/

/* Velocidade de movimento no mapa
 *    0 = se arrasta (quase não anda)
 *   50 = padrão — boa velocidade para explorar o escritório
 *  100 = corre — difícil de controlar com precisão
 * Dica: se está difícil entrar nas áreas certas, diminua. */
#define CAL_VELOCIDADE_JOGADOR        100

/* Zona morta do joystick
 * Define quanto você precisa inclinar o joystick até o personagem se mover.
 *    0 = qualquer toque já anda (sensível demais, o personagem "deriva")
 *   50 = padrão — ignora inclinações leves e travamento do eixo central
 *  100 = precisa inclinar forte para mover
 * Dica: se o personagem anda sozinho sem tocar no joystick, aumente este valor. */
#define CAL_ZONA_MORTA_JOYSTICK       50

/* Compensação de alcance do eixo X (esquerda/direita)
 * Módulos thumbstick comuns têm o potenciômetro X com range físico menor
 * que o Y, fazendo esquerda/direita parecer mais lento que cima/baixo.
 * Este valor amplia o sinal X antes de calcular a velocidade.
 *    0 = sem compensação (use se X e Y já parecem iguais)
 *   50 = padrão — multiplica o alcance X por 1,5×
 *  100 = multiplica por 2,0× (use se X for muito mais lento que Y)
 * Dica: aumente até cima/baixo e esquerda/direita parecerem iguais. */
#define CAL_BOOST_EIXO_X              50

/* Velocidade da animação de caminhada (ciclo dos passos)
 *    0 = passos muito lentos (pés arrastando)
 *   50 = padrão
 *  100 = passos rápidos (correndo na animação)
 * Obs: não afeta a velocidade real, só o visual dos pés. */
#define CAL_VELOCIDADE_ANIMACAO       60


/* ============================================================================
 *  TEMPO DE JOGO
 *  Quanto tempo real o expediente (08h→18h) dura.
 * ============================================================================*/

/* Velocidade do relógio interno
 *    0 = ~5 minutos reais = um dia de trabalho (para testar com calma)
 *   50 = ~3 minutos reais  ← PADRÃO — partida desafiadora
 *  100 = ~50 segundos reais (frenético)
 * Dica: enquanto desenvolve, use 20–30 para ter tempo de explorar sem pressão. */
#define CAL_VELOCIDADE_TEMPO          30


/* ============================================================================
 *  DIFICULDADE — TAREFAS (verde e amarela)
 *  Quando cada tipo de tarefa aparece após o início do expediente.
 * ============================================================================*/

/* Aparecimento da tarefa VERDE (troca de senha)
 *    0 = aparece em ~2 segundos de expediente
 *   50 = aparece em ~5 segundos  ← PADRÃO
 *  100 = aparece em ~10 segundos */
#define CAL_TAREFA_VERDE_INICIO       50

/* Aparecimento da tarefa AMARELA (servidores)
 *    0 = aparece em ~10 segundos de expediente
 *   50 = aparece em ~20 segundos  ← PADRÃO
 *  100 = aparece em ~30 segundos */
#define CAL_TAREFA_AMARELA_INICIO     50

/* Quando os ataques VERMELHOS começam a acontecer
 *    0 = ataques surgem ~20 segundos após iniciar o expediente
 *   50 = surgem ~40 segundos após iniciar  ← PADRÃO
 *  100 = surgem ~60 segundos após iniciar (mais tempo para se preparar) */
#define CAL_TAREFA_VERMELHO_INICIO    50


/* ============================================================================
 *  DIFICULDADE — ATAQUES VERMELHOS
 *  Quão rápidos e frequentes são os ataques de ransomware / DDoS.
 * ============================================================================*/

/* Intensidade dos ataques vermelhos
 *    0 = fácil  — 40 segundos para mitigar, 60 segundos entre ataques
 *   50 = padrão — ~22 segundos para mitigar, ~35 segundos entre ataques
 *  100 = brutal —  5 segundos para mitigar, 10 segundos entre ataques
 * Atenção: acima de 80 o jogo fica quase impossível. */
#define CAL_DIFICULDADE_ATAQUES       50

/* Número de vidas  (valor direto: 1, 2 ou 3 — não é escala 0-100)
 *   1 = sem erros permitidos
 *   2 = um erro tolerado
 *   3 = padrão  ← RECOMENDADO */
#define CAL_VIDAS_INICIAIS            2


/* ============================================================================
 *  TELA (display ST7796)
 *  Calibração de cor do painel físico deste console.
 *  Cada lote de tela pode ser ligeiramente diferente.
 * ============================================================================*/

/* Correção do canal vermelho
 * O painel ST7796 tem LEDs vermelhos com eficiência menor que verde/azul —
 * o vermelho aparece mais escuro do que deveria ser.
 *    0 = sem correção (vermelho fica claramente desbotado)
 *   50 = correção padrão para este painel  ← RECOMENDADO
 *  100 = correção máxima (vermelho muito intenso, pode saturar)
 * Dica: olhe a tela de game over (vermelho) e a tela de vitória (verde)
 *       e ajuste até as cores parecerem equilibradas. */
#define CAL_BOOST_COR_VERMELHA        45


/* ============================================================================
 *  LEDs WS2812
 *  Brilho dos 3 LEDs RGB na lateral do console.
 *  LED 1 = tarefa verde  |  LED 2 = tarefa amarela  |  LED 3 = alerta vermelho
 * ============================================================================*/

/* Brilho geral dos LEDs
 *    0 = LEDs completamente apagados
 *   50 = brilho moderado (agradável em ambientes com pouca luz)
 *   75 = padrão recomendado  ← ponto de equilíbrio visual
 *  100 = brilho máximo (muito intenso perto dos olhos)
 * Dica: em sala escura, use 40–60 para não ofuscar. */
#define CAL_BRILHO_LED                100
