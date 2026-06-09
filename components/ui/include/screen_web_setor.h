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
 * Assinatura do listener registrado pela engine quando uma carta NFC é lida
 * com a tela aberta (BTN_Y pressionado → NFC ativo).
 *
 * Módulo 1 : listener estruturado, lógica pendente.
 * Módulo 2+: implementar resposta por tipo de carta:
 *   CARTA_BALANCEAMENTO → mitiga DDoS
 *   CARTA_ISOLAMENTO    → isola segmento de rede
 *   CARTA_BACKUP        → não se aplica nesta tela (ignorar ou negar)    */
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
 * Mostra o overlay CHIADO (opacidade cresce com o progresso), quebra
 * 1–4 slots aleatórios na BAIA e abre a UI de gestão de HDs.
 * CARTA_ISOLAMENTO (< 50%): fade e restauração automática.
 * CARTA_ISOLAMENTO (≥ 50%): congela e pede CARTA_BACKUP.
 * CARTA_BACKUP: sucesso se todos os slots da BAIA = HD_BOM.            */
void screen_web_setor_ransomware_start(web_setor_id_t id);

/* Chamado pela engine quando NFC detecta uma carta enquanto esta tela está
 * aberta. Roteia para o callback registrado via set_carta_cb().           */
void screen_web_setor_on_carta(web_setor_id_t id, carta_id_t carta);

/* Registra o callback NFC global (compartilhado entre instâncias).
 * Chamar uma vez na inicialização — Módulo 2+ fará o registro.           */
void screen_web_setor_set_carta_cb(web_setor_carta_cb_t cb);
