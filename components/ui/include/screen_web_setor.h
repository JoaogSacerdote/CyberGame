#pragma once
#include <stdbool.h>
#include "nfc_config.h"  /* carta_id_t */

/* ── Identificação de instância ──────────────────────────────────────────── *
 *
 * O jogo possui dois servidores físicos: Esquerda e Direita.
 * Cada um é uma instância lógica completamente independente — o estado de
 * uma não afeta a outra. Passe o ID correto em todas as funções.           */

typedef enum {
    WEB_SETOR_ESQUERDA = 0,
    WEB_SETOR_DIREITA,
    WEB_SETOR_COUNT,
} web_setor_id_t;

/* ── Callback NFC ────────────────────────────────────────────────────────── *
 *
 * Assinatura do listener registrado pela engine quando uma carta NFC é lida.
 * A leitura é AUTOMÁTICA: o engine faz poll do NFC sempre que esta tela está
 * aberta com um ataque ativo no servidor — nenhum botão é necessário.
 *
 * Resposta por tipo de carta:
 *   CARTA_BALANCEAMENTO → mitiga DDoS
 *   CARTA_ISOLAMENTO    → isola o Ransomware (Cenário A/B)
 *   CARTA_BACKUP        → restaura o sistema após Cenário B              */
typedef void (*web_setor_carta_cb_t)(web_setor_id_t id, carta_id_t carta);

/* ── API pública ─────────────────────────────────────────────────────────── */

void screen_web_setor_build(web_setor_id_t id);
void screen_web_setor_destroy(web_setor_id_t id);
bool screen_web_setor_is_open(web_setor_id_t id);

/* Inicia o ataque DDoS na instância especificada.
 * Afeta apenas esta instância — a outra permanece no estado base.
 * Chamado pela engine quando fsm_get_attack_active() muda para true.     */
void screen_web_setor_ddos_start(web_setor_id_t id);

/* Inicia o ataque de Ransomware na instância especificada.
 * Mostra o overlay CHIADO (opacidade cresce com o progresso) + barra.
 * Sem chamas/envelope-chama (exclusivos do DDoS) e sem UI de HDs (a troca
 * de HDs pertence à tarefa amarela, que libera a carta de Backup).
 * CARTA_ISOLAMENTO (< 50%): progresso zera, CHIADO faz fade (~2 s).
 * CARTA_ISOLAMENTO (≥ 50%): congela e pede CARTA_BACKUP.
 * CARTA_BACKUP: restaura o sistema e volta ao estado base.              */
void screen_web_setor_ransomware_start(web_setor_id_t id);

/* Chamado pela engine quando NFC detecta uma carta enquanto esta tela está
 * aberta. Roteia para o callback registrado via set_carta_cb().           */
void screen_web_setor_on_carta(web_setor_id_t id, carta_id_t carta);

/* Registra o callback NFC global (compartilhado entre instâncias).
 * Chamar uma vez na inicialização — Módulo 2+ fará o registro.           */
void screen_web_setor_set_carta_cb(web_setor_carta_cb_t cb);
