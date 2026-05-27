#pragma once

/* Versao do firmware CyberGame.
 *
 * Semver simples (MAJOR.MINOR.PATCH). Incrementar manualmente em commits
 * que mudem comportamento observavel pelo jogador / hardware:
 *   - MAJOR: breaking change (ex.: troca de plataforma, novo MCU)
 *   - MINOR: feature nova (ex.: novo ataque, nova tela)
 *   - PATCH: bugfix sem mudanca de feature
 *
 * Build indicator detalhado vem do git via app_get_description() (IDF
 * embute commit hash quando ESP_APP_PROJECT_VER nao esta hardcoded). */

#define CYBERGAME_VERSION_MAJOR    0
#define CYBERGAME_VERSION_MINOR    1
#define CYBERGAME_VERSION_PATCH    0

#define CYBERGAME_VERSION_STR      "0.1.0"
