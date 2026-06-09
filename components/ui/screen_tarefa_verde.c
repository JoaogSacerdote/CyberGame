#include "screen_tarefa_verde.h"
#include "ui_internal.h"

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "esp_log.h"
#include "esp_random.h"
#include "lvgl.h"
#include "asset_loader.h"
#include "asset_ids.h"
#include "button_hal.h"
#include "joystick_hal.h"
#include "gamestate.h"

static const char *TAG = "TAREFA_VD";

/* ── Word lists ──────────────────────────────────────────────────────────── */

static const char *USUARIOS_INSEGUROS[] = { "admin", "usuario", "root", "guest" };
static const char *USUARIOS_SEGUROS[]   = { "adm.joao_2025", "ti.sec.01", "op_rede_br2", "adm_sys_99x" };
static const char *SENHAS_INSEGURAS[]   = { "123456", "senha123", "qwerty", "empresa01" };
static const char *SENHAS_SEGURAS[]     = { "C!ber@25#Seg", "M@q_TI!9x2", "S3gur@_Net#25", "P@ss_TI!x2" };

#define N_INS_U  (sizeof(USUARIOS_INSEGUROS) / sizeof(USUARIOS_INSEGUROS[0]))
#define N_SEG_U  (sizeof(USUARIOS_SEGUROS)   / sizeof(USUARIOS_SEGUROS[0]))
#define N_INS_S  (sizeof(SENHAS_INSEGURAS)   / sizeof(SENHAS_INSEGURAS[0]))
#define N_SEG_S  (sizeof(SENHAS_SEGURAS)     / sizeof(SENHAS_SEGURAS[0]))

/* ── Estados ─────────────────────────────────────────────────────────────── */

typedef enum {
    STEP_USUARIO = 0,  /* escolhendo usuario */
    STEP_SENHA,        /* escolhendo senha */
    STEP_CONFIRMAR,    /* confirmando selecao */
    STEP_SUCESSO,      /* credenciais aceitas — mensagem verde antes de sair */
    STEP_CONCLUIDA,    /* tarefa ja concluida — visualizacao ou re-edicao */
} step_t;

static step_t  s_step      = STEP_USUARIO;
static uint8_t s_cursor    = 0;
static uint8_t s_sel_user  = 0xFF;
static uint8_t s_sel_pass  = 0xFF;
static bool    s_conf_error = false;

static const char *s_users[4];
static bool        s_user_secure[4];
static const char *s_passs[4];
static bool        s_pass_secure[4];

/* ── Widgets ─────────────────────────────────────────────────────────────── */

static lv_obj_t   *s_overlay     = NULL;
static lv_obj_t   *s_opcoes      = NULL;   /* CREDENCIAL_OPCOES */
static lv_obj_t   *s_lbl_selecao = NULL;   /* "Selecione o Usuario / Senha" */
static lv_obj_t   *s_lbl_usuario = NULL;   /* CAMPO_USUARIO */
static lv_obj_t   *s_lbl_senha   = NULL;   /* CAMPO_SENHA */
static lv_obj_t   *s_choice[4]   = {NULL};
static lv_obj_t   *s_conf_panel  = NULL;   /* CAMPO_ESCOLHA_CONFIRMAR/CORRIGIR */
static lv_obj_t   *s_lbl_conf    = NULL;   /* texto dentro do painel */
static lv_timer_t *s_timer       = NULL;

static tarefa_vd_cb_t s_done_cb = NULL;

typedef enum { A_FUNDO = 0, A_OPCOES, A_COUNT } slot_t;
static loaded_asset_t s_assets[A_COUNT];

static button_state_t s_a_cache = BTN_RELEASED;
static button_state_t s_b_cache = BTN_RELEASED;

/* ── Posicoes (fonte: INTERACOES.txt + POSICAO.txt, pivot bottom-center) ─── *
 *  draw_x = pivot_x - w/2   draw_y = pivot_y - h                            */

/* CAMPO_SELECAO_USUARIO bc(173,49) 250x20  CAMPO_SELECAO_SENHA bc(173,125) 250x20 */
#define SEL_X    48
#define SEL_Y_U  29
#define SEL_Y_P 105
#define SEL_W   250
#define SEL_H    20

/* CAMPO_USUARIO bc(254,81) 250x18    CAMPO_SENHA bc(252,155) 250x18 */
#define FLD_USER_X  129
#define FLD_USER_Y   63
#define FLD_PASS_X  127
#define FLD_PASS_Y  137
#define FLD_W       250
#define FLD_H        18

/* CAMPO_ESCOLHA_1..4 — derivado dos boxes reais de CREDENCIAL_OPCOES (pixel analysis).
 * 0=esq-cima  1=esq-baixo  2=dir-cima  3=dir-baixo */
static const int16_t CHOICE_X[4] = {  91,  91, 297, 297 };
static const int16_t CHOICE_Y[4] = { 183, 244, 183, 244 };
#define CHOICE_W  90
#define CHOICE_H  42

/* CAMPO_ESCOLHA_CONFIRMAR/CORRIGIR bc(239,292) 308x115 */
#define CONF_X   85
#define CONF_Y  177
#define CONF_W  308
#define CONF_H  115

/* CREDENCIAL_OPCOES 308x115 — posicionado pelo pixel data real da imagem.
 * draw(85, 177): boxes internos ficam em screen (91,183),(91,244),(297,183),(297,244).
 * O POSICAO.txt indica bc(244,115) que coloca a imagem no topo da tela (erro de origem).
 * Anchor equivale ao mesmo bc(239,292) do CAMPO_ESCOLHA_CONFIRMAR/CORRIGIR. */
#define OPCOES_CENTER_X  239
#define OPCOES_BOTTOM_Y  292

/* ── Forward declarations ────────────────────────────────────────────────── */

static void pick_words(void);

/* ── Helpers ─────────────────────────────────────────────────────────────── */

static void no_scroll(lv_obj_t *o)
{
    lv_obj_remove_flag(o, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(o, LV_DIR_NONE);
}

static void refresh_choices(void)
{
    bool is_pass = (s_step == STEP_SENHA);
    for (uint8_t i = 0; i < 4; i++) {
        bool sel     = is_pass ? (s_sel_pass == i) : (s_sel_user == i);
        bool hovered = (s_cursor == i);

        if (sel && hovered) {
            lv_obj_set_style_bg_color(s_choice[i], lv_color_hex(0x00AA33), LV_PART_MAIN);
            lv_obj_set_style_bg_opa (s_choice[i], LV_OPA_40, LV_PART_MAIN);
        } else if (sel) {
            lv_obj_set_style_bg_color(s_choice[i], lv_color_hex(0x00CC44), LV_PART_MAIN);
            lv_obj_set_style_bg_opa (s_choice[i], LV_OPA_30, LV_PART_MAIN);
        } else if (hovered) {
            lv_obj_set_style_bg_color(s_choice[i], lv_color_hex(0xFFCC00), LV_PART_MAIN);
            lv_obj_set_style_bg_opa (s_choice[i], LV_OPA_30, LV_PART_MAIN);
        } else {
            lv_obj_set_style_bg_opa(s_choice[i], LV_OPA_0, LV_PART_MAIN);
        }

        lv_obj_set_style_border_width(s_choice[i],
            (hovered || sel) ? 2 : 0, LV_PART_MAIN);
        lv_obj_set_style_border_color(s_choice[i],
            sel ? lv_color_hex(0x00AA33) : lv_color_hex(0xFFCC00), LV_PART_MAIN);

        lv_obj_t *lbl = lv_obj_get_child(s_choice[i], 0);
        if (lbl) lv_label_set_text(lbl, is_pass ? s_passs[i] : s_users[i]);
    }
}

/* Atualiza CAMPO_USUARIO e CAMPO_SENHA: mostra selecao atual, ou credencial
 * salva (se houver), ou "admin" como padrao inicial do sistema.            */
static void update_field_labels(void)
{
    const char *saved_usr = NULL, *saved_pw = NULL;
    gamestate_verde_selecao_get(&saved_usr, &saved_pw);

    if (s_lbl_usuario) {
        lv_label_set_text(s_lbl_usuario,
            s_sel_user < 4 ? s_users[s_sel_user] :
            saved_usr       ? saved_usr : "admin");
    }
    if (s_lbl_senha) {
        lv_label_set_text(s_lbl_senha,
            s_sel_pass < 4 ? s_passs[s_sel_pass] :
            saved_pw        ? saved_pw : "admin");
    }
}

static void update_conf_panel(void)
{
    if (!s_lbl_conf) return;

    if (s_step == STEP_SUCESSO) {
        lv_label_set_text(s_lbl_conf,
            "Credenciais seguras!\n\n"
            "[A] Atualizar    [B] Sair");
        lv_obj_set_style_text_color(s_lbl_conf, lv_color_hex(0x00CC44), LV_PART_MAIN);

    } else if (s_step == STEP_CONCLUIDA) {
        const char *usr = NULL, *pw = NULL;
        gamestate_verde_selecao_get(&usr, &pw);
        char buf[180];
        snprintf(buf, sizeof(buf),
                 "Usuario: %s\nSenha: %s\n\n[A] Alterar    [B] Sair",
                 usr ? usr : "admin", pw ? pw : "admin");
        lv_label_set_text(s_lbl_conf, buf);
        lv_obj_set_style_text_color(s_lbl_conf, lv_color_hex(0xCCCCCC), LV_PART_MAIN);

    } else if (s_step == STEP_CONFIRMAR) {
        if (s_conf_error) {
            lv_label_set_text(s_lbl_conf,
                "Usuario ou Senha nao atendem\naos requisitos de seguranca.\n\n"
                "[A/B] Tentar novamente");
            lv_obj_set_style_text_color(s_lbl_conf, lv_color_hex(0xFF4444), LV_PART_MAIN);
        } else {
            lv_label_set_text(s_lbl_conf,
                "Credenciais validas!\n\n"
                "[A] Confirmar    [B] Corrigir");
            lv_obj_set_style_text_color(s_lbl_conf, lv_color_hex(0xEEEEEE), LV_PART_MAIN);
        }
    }
}

static void show_step(void)
{
    bool show_sel  = (s_step == STEP_USUARIO || s_step == STEP_SENHA);
    bool show_conf = (s_step == STEP_CONFIRMAR || s_step == STEP_SUCESSO || s_step == STEP_CONCLUIDA);

    /* CREDENCIAL_OPCOES: visivel so durante selecao (nunca com painel confirmar) */
    if (s_opcoes) {
        if (show_sel) lv_obj_remove_flag(s_opcoes, LV_OBJ_FLAG_HIDDEN);
        else          lv_obj_add_flag   (s_opcoes, LV_OBJ_FLAG_HIDDEN);
    }

    /* CAMPO_SELECAO — texto de instrucao, so na selecao */
    if (s_lbl_selecao) {
        if (show_sel) {
            lv_obj_remove_flag(s_lbl_selecao, LV_OBJ_FLAG_HIDDEN);
            lv_obj_set_pos(s_lbl_selecao, SEL_X,
                s_step == STEP_USUARIO ? SEL_Y_U : SEL_Y_P);
            lv_label_set_text(s_lbl_selecao,
                s_step == STEP_USUARIO ? "Selecione o Usuario" : "Selecione a Senha");
        } else {
            lv_obj_add_flag(s_lbl_selecao, LV_OBJ_FLAG_HIDDEN);
        }
    }

    /* CAMPO_USUARIO e CAMPO_SENHA — sempre visiveis (credenciais ficam nos campos
     * mesmo durante confirmacao; o painel fica na area inferior sem sobreposicao) */
    if (s_lbl_usuario) lv_obj_remove_flag(s_lbl_usuario, LV_OBJ_FLAG_HIDDEN);
    if (s_lbl_senha)   lv_obj_remove_flag(s_lbl_senha,   LV_OBJ_FLAG_HIDDEN);

    /* Opcoes de escolha — apenas em selecao */
    for (uint8_t i = 0; i < 4; i++) {
        if (!s_choice[i]) continue;
        if (show_sel) lv_obj_remove_flag(s_choice[i], LV_OBJ_FLAG_HIDDEN);
        else          lv_obj_add_flag   (s_choice[i], LV_OBJ_FLAG_HIDDEN);
    }

    /* Painel confirmar/corrigir — mutuamente exclusivo com CREDENCIAL_OPCOES */
    if (s_conf_panel) {
        if (show_conf) lv_obj_remove_flag(s_conf_panel, LV_OBJ_FLAG_HIDDEN);
        else           lv_obj_add_flag   (s_conf_panel, LV_OBJ_FLAG_HIDDEN);
    }

    if (show_conf)  update_conf_panel();
    if (show_sel)   refresh_choices();
}

/* ── Tick ────────────────────────────────────────────────────────────────── */

static void tarefa_vd_tick(lv_timer_t *t)
{
    (void)t;
    if (!s_overlay) return;

    const joystick_data_t j = joystick_hal_get_state();
    const bool left  = (j.x < -50);
    const bool right = (j.x >  50);
    const bool up    = (j.y < -50);
    const bool down  = (j.y >  50);

    static bool prev_left = false, prev_right = false;
    static bool prev_up   = false, prev_down  = false;
    const bool new_left  = left  && !prev_left;
    const bool new_right = right && !prev_right;
    const bool new_up    = up    && !prev_up;
    const bool new_down  = down  && !prev_down;
    prev_left = left; prev_right = right;
    prev_up   = up;   prev_down  = down;

    const bool a_edge = ui_btn_edge(BTN_A, &s_a_cache);
    const bool b_edge = ui_btn_edge(BTN_B, &s_b_cache);

    switch (s_step) {

    /* ---- Credenciais aceitas: A salva e conclui, B descarta e sai ---- */
    case STEP_SUCESSO: {
        if (a_edge) {
            gamestate_salvar_verde_selecao(s_users[s_sel_user], s_passs[s_sel_pass]);
            gamestate_concluir_verde();
            ESP_LOGI(TAG, "credenciais salvas: usuario='%s' senha='%s'",
                     s_users[s_sel_user], s_passs[s_sel_pass]);
            tarefa_vd_cb_t cb = s_done_cb;
            screen_tarefa_verde_destroy();
            if (cb) cb(TAREFA_VD_CONCLUIDA);
        } else if (b_edge) {
            tarefa_vd_cb_t cb = s_done_cb;
            screen_tarefa_verde_destroy();
            if (cb) cb(TAREFA_VD_CANCELADA);
        }
        return;
    }

    /* ---- Tarefa ja concluida ---- */
    case STEP_CONCLUIDA:
        if (a_edge) {
            /* Re-editar: gera novas opcoes e reinicia selecao */
            pick_words();
            s_step     = STEP_USUARIO;
            s_cursor   = 0;
            s_sel_user = 0xFF;
            s_sel_pass = 0xFF;
            s_conf_error = false;
            update_field_labels();
            show_step();
        } else if (b_edge) {
            tarefa_vd_cb_t cb = s_done_cb;
            screen_tarefa_verde_destroy();
            if (cb) cb(TAREFA_VD_CANCELADA);
        }
        return;

    /* ---- Confirmacao de credenciais ---- */
    case STEP_CONFIRMAR:
        if (s_conf_error) {
            /* Aguardando reconhecimento do erro */
            if (a_edge || b_edge) {
                s_conf_error = false;
                s_step     = STEP_USUARIO;
                s_cursor   = 0;
                s_sel_user = 0xFF;
                s_sel_pass = 0xFF;
                update_field_labels();
                show_step();
            }
            return;
        }
        if (a_edge) {
            const bool ok = (s_sel_user < 4) && (s_sel_pass < 4) &&
                             s_user_secure[s_sel_user] && s_pass_secure[s_sel_pass];
            if (ok) {
                ESP_LOGI(TAG, "credenciais validas: %s / %s",
                         s_users[s_sel_user], s_passs[s_sel_pass]);
                s_step = STEP_SUCESSO;
                show_step();
            } else {
                ESP_LOGW(TAG, "credenciais inseguras");
                s_conf_error = true;
                update_conf_panel();
            }
        } else if (b_edge) {
            /* Corrigir: volta ao inicio da selecao */
            s_step     = STEP_USUARIO;
            s_cursor   = 0;
            s_sel_user = 0xFF;
            s_sel_pass = 0xFF;
            s_conf_error = false;
            update_field_labels();
            show_step();
        }
        return;

    /* ---- Selecao de usuario / senha ---- */
    case STEP_USUARIO:
    case STEP_SENHA:
        /* Navega no grid 2x2: 0=esq-cima 1=esq-baixo 2=dir-cima 3=dir-baixo */
        if (new_left  && s_cursor >= 2) { s_cursor -= 2; refresh_choices(); }
        if (new_right && s_cursor <  2) { s_cursor += 2; refresh_choices(); }
        if (new_up    && (s_cursor == 1 || s_cursor == 3)) { s_cursor--; refresh_choices(); }
        if (new_down  && (s_cursor == 0 || s_cursor == 2)) { s_cursor++; refresh_choices(); }

        if (b_edge) {
            ESP_LOGI(TAG, "B — saindo da tarefa verde");
            tarefa_vd_cb_t cb = s_done_cb;
            screen_tarefa_verde_destroy();
            if (cb) cb(TAREFA_VD_CANCELADA);
            return;
        }

        if (a_edge) {
            if (s_step == STEP_USUARIO) {
                s_sel_user = s_cursor;
                ESP_LOGI(TAG, "usuario: %s (%s)", s_users[s_sel_user],
                         s_user_secure[s_sel_user] ? "seguro" : "inseguro");
                if (s_lbl_usuario) lv_label_set_text(s_lbl_usuario, s_users[s_sel_user]);
                s_step   = STEP_SENHA;
                s_cursor = 0;
                show_step();
            } else {
                s_sel_pass = s_cursor;
                ESP_LOGI(TAG, "senha: %s (%s)", s_passs[s_sel_pass],
                         s_pass_secure[s_sel_pass] ? "segura" : "insegura");
                if (s_lbl_senha) lv_label_set_text(s_lbl_senha, s_passs[s_sel_pass]);
                s_step       = STEP_CONFIRMAR;
                s_conf_error = false;
                show_step();
            }
        }
        break;
    }
}

/* ── Selecao aleatoria de palavras ───────────────────────────────────────── */

static uint8_t rnd_pick(uint8_t max) { return (uint8_t)(esp_random() % max); }

static void pick_words(void)
{
    uint8_t iu0 = rnd_pick(N_INS_U);
    uint8_t iu1 = (iu0 + 1 + rnd_pick(N_INS_U - 1)) % N_INS_U;
    uint8_t su0 = rnd_pick(N_SEG_U);
    uint8_t su1 = (su0 + 1 + rnd_pick(N_SEG_U - 1)) % N_SEG_U;

    uint8_t pos[4] = {0, 1, 2, 3};
    for (uint8_t i = 3; i > 0; i--) {
        uint8_t j = rnd_pick(i + 1);
        uint8_t tmp = pos[i]; pos[i] = pos[j]; pos[j] = tmp;
    }
    s_users[pos[0]] = USUARIOS_INSEGUROS[iu0]; s_user_secure[pos[0]] = false;
    s_users[pos[1]] = USUARIOS_INSEGUROS[iu1]; s_user_secure[pos[1]] = false;
    s_users[pos[2]] = USUARIOS_SEGUROS[su0];   s_user_secure[pos[2]] = true;
    s_users[pos[3]] = USUARIOS_SEGUROS[su1];   s_user_secure[pos[3]] = true;

    uint8_t is0 = rnd_pick(N_INS_S);
    uint8_t is1 = (is0 + 1 + rnd_pick(N_INS_S - 1)) % N_INS_S;
    uint8_t ss0 = rnd_pick(N_SEG_S);
    uint8_t ss1 = (ss0 + 1 + rnd_pick(N_SEG_S - 1)) % N_SEG_S;

    for (uint8_t i = 3; i > 0; i--) {
        uint8_t j = rnd_pick(i + 1);
        uint8_t tmp = pos[i]; pos[i] = pos[j]; pos[j] = tmp;
    }
    s_passs[pos[0]] = SENHAS_INSEGURAS[is0]; s_pass_secure[pos[0]] = false;
    s_passs[pos[1]] = SENHAS_INSEGURAS[is1]; s_pass_secure[pos[1]] = false;
    s_passs[pos[2]] = SENHAS_SEGURAS[ss0];   s_pass_secure[pos[2]] = true;
    s_passs[pos[3]] = SENHAS_SEGURAS[ss1];   s_pass_secure[pos[3]] = true;
}

/* ── Build / Destroy ─────────────────────────────────────────────────────── */

void screen_tarefa_verde_build(tarefa_vd_cb_t done_cb)
{
    if (s_overlay) return;

    const esp_err_t e0 = asset_loader_load(ASSET_TYPE_SPRITE, ASSET_TAREFA_VD_FUNDO,  &s_assets[A_FUNDO]);
    const esp_err_t e1 = asset_loader_load(ASSET_TYPE_SPRITE, ASSET_TAREFA_VD_OPCOES, &s_assets[A_OPCOES]);
    if (e0 != ESP_OK || e1 != ESP_OK) {
        ESP_LOGE(TAG, "assets tarefa verde indisponiveis");
        asset_loader_free(&s_assets[A_FUNDO]);
        asset_loader_free(&s_assets[A_OPCOES]);
        return;
    }

    s_done_cb = done_cb;

    const bool ja_concluida = (gamestate_verde_estado() == TAREFA_CONCLUIDA);

    /* Sempre inicializa palavras (choices ficam ocultas se ja_concluida) */
    pick_words();

    if (!ja_concluida) {
        s_step     = STEP_USUARIO;
        s_cursor   = 0;
        s_sel_user = 0xFF;
        s_sel_pass = 0xFF;
        s_conf_error = false;
    } else {
        s_step = STEP_CONCLUIDA;
    }

    /* ── Overlay full-screen ── */
    s_overlay = lv_obj_create(lv_screen_active());
    lv_obj_set_size(s_overlay, 480, 320);
    lv_obj_set_pos(s_overlay, 0, 0);
    lv_obj_set_style_bg_opa(s_overlay, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_overlay, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_border_width(s_overlay, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(s_overlay, 0, LV_PART_MAIN);
    no_scroll(s_overlay);

    /* ── CREDENCIAL_GERAL — fundo completo (480x320) ── */
    lv_obj_t *bg = lv_image_create(s_overlay);
    lv_image_set_src(bg, &s_assets[A_FUNDO].dsc);
    lv_obj_set_pos(bg, 0, 0);
    no_scroll(bg);

    /* ── CREDENCIAL_OPCOES — posicao calculada em runtime pelas dims reais ── */
    s_opcoes = lv_image_create(s_overlay);
    lv_image_set_src(s_opcoes, &s_assets[A_OPCOES].dsc);
    {
        const int ow = (int)s_assets[A_OPCOES].dsc.header.w;
        const int oh = (int)s_assets[A_OPCOES].dsc.header.h;
        lv_obj_set_pos(s_opcoes, OPCOES_CENTER_X - ow / 2, OPCOES_BOTTOM_Y - oh);
    }
    no_scroll(s_opcoes);

    /* ── CAMPO_SELECAO_USUARIO/SENHA — texto simples sem fundo ── */
    s_lbl_selecao = lv_label_create(s_overlay);
    lv_label_set_text(s_lbl_selecao, "Selecione o Usuario");
    lv_obj_set_pos(s_lbl_selecao, SEL_X, SEL_Y_U);
    lv_obj_set_style_text_color(s_lbl_selecao, lv_color_white(), LV_PART_MAIN);
    no_scroll(s_lbl_selecao);

    /* ── CAMPO_USUARIO — credencial de usuario atual ── */
    s_lbl_usuario = lv_label_create(s_overlay);
    lv_obj_set_pos(s_lbl_usuario, FLD_USER_X, FLD_USER_Y);
    lv_obj_set_size(s_lbl_usuario, FLD_W, FLD_H);
    lv_obj_set_style_text_color(s_lbl_usuario, lv_color_hex(0x111111), LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_lbl_usuario, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s_lbl_usuario, LV_OPA_COVER, LV_PART_MAIN);
    lv_label_set_long_mode(s_lbl_usuario, LV_LABEL_LONG_CLIP);
    no_scroll(s_lbl_usuario);

    /* ── CAMPO_SENHA — credencial de senha atual ── */
    s_lbl_senha = lv_label_create(s_overlay);
    lv_obj_set_pos(s_lbl_senha, FLD_PASS_X, FLD_PASS_Y);
    lv_obj_set_size(s_lbl_senha, FLD_W, FLD_H);
    lv_obj_set_style_text_color(s_lbl_senha, lv_color_hex(0x111111), LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_lbl_senha, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s_lbl_senha, LV_OPA_COVER, LV_PART_MAIN);
    lv_label_set_long_mode(s_lbl_senha, LV_LABEL_LONG_CLIP);
    no_scroll(s_lbl_senha);

    /* Preenche campos com valor salvo ou padrao "admin" */
    update_field_labels();

    /* ── CAMPO_ESCOLHA_1..4 — fundo transparente, texto preto ── */
    for (uint8_t i = 0; i < 4; i++) {
        s_choice[i] = lv_obj_create(s_overlay);
        lv_obj_set_size(s_choice[i], CHOICE_W, CHOICE_H);
        lv_obj_set_pos(s_choice[i], CHOICE_X[i], CHOICE_Y[i]);
        lv_obj_set_style_bg_opa(s_choice[i], LV_OPA_0, LV_PART_MAIN);
        lv_obj_set_style_border_width(s_choice[i], 0, LV_PART_MAIN);
        lv_obj_set_style_pad_all(s_choice[i], 2, LV_PART_MAIN);
        no_scroll(s_choice[i]);

        lv_obj_t *lbl = lv_label_create(s_choice[i]);
        lv_label_set_text(lbl, s_users[i]);
        lv_label_set_long_mode(lbl, LV_LABEL_LONG_CLIP);
        lv_obj_set_width(lbl, CHOICE_W - 4);
        lv_obj_set_style_text_color(lbl, lv_color_hex(0x111111), LV_PART_MAIN);
        lv_obj_center(lbl);
        no_scroll(lbl);
    }

    /* ── CAMPO_ESCOLHA_CONFIRMAR/CORRIGIR — painel mutuamente exclusivo com CREDENCIAL_OPCOES ── */
    s_conf_panel = lv_obj_create(s_overlay);
    lv_obj_set_size(s_conf_panel, CONF_W, CONF_H);
    lv_obj_set_pos(s_conf_panel, CONF_X, CONF_Y);
    lv_obj_set_style_bg_color(s_conf_panel, lv_color_hex(0x1a3a4a), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s_conf_panel, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_color(s_conf_panel, lv_color_hex(0x2a6070), LV_PART_MAIN);
    lv_obj_set_style_border_width(s_conf_panel, 1, LV_PART_MAIN);
    lv_obj_set_style_pad_all(s_conf_panel, 10, LV_PART_MAIN);
    no_scroll(s_conf_panel);

    s_lbl_conf = lv_label_create(s_conf_panel);
    lv_label_set_text(s_lbl_conf, "");
    lv_label_set_long_mode(s_lbl_conf, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(s_lbl_conf, CONF_W - 24);
    lv_obj_set_style_text_color(s_lbl_conf, lv_color_hex(0xEEEEEE), LV_PART_MAIN);
    lv_obj_set_style_text_align(s_lbl_conf, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_align(s_lbl_conf, LV_ALIGN_TOP_MID, 0, 0);
    no_scroll(s_lbl_conf);

    s_a_cache = button_hal_peek(BTN_A);
    s_b_cache = button_hal_peek(BTN_B);

    show_step();

    s_timer = lv_timer_create(tarefa_vd_tick, UI_TICK_MS, NULL);
    ESP_LOGI(TAG, "tarefa verde aberta (ja_concluida=%d)", ja_concluida);
}

bool screen_tarefa_verde_is_open(void) { return s_overlay != NULL; }

void screen_tarefa_verde_destroy(void)
{
    if (s_timer)   { lv_timer_delete(s_timer); s_timer = NULL; }
    if (s_overlay) { lv_obj_delete(s_overlay); s_overlay = NULL; }
    s_opcoes = s_lbl_selecao = s_lbl_usuario = s_lbl_senha = NULL;
    s_conf_panel = s_lbl_conf = NULL;
    for (uint8_t i = 0; i < 4; i++) s_choice[i] = NULL;
    asset_loader_free(&s_assets[A_FUNDO]);
    asset_loader_free(&s_assets[A_OPCOES]);
    s_done_cb = NULL;
    ESP_LOGI(TAG, "tarefa verde fechada");
}
