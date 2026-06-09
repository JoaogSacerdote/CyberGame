/* room_layout_sd.c — Carrega layouts de sala do SD card em vez de tabelas
 * compiladas em flash. Cada sala eh um arquivo JSON:
 *   /sd/assets/layout/<nome>.json
 *
 * Lazy-load: primeira chamada a room_layout_find() para uma sala desconhecida
 * le e faz parse do JSON; chamadas seguintes retornam do cache estatico.
 *
 * Sem alocacao dinamica apos o primeiro load — todos os buffers sao estaticos
 * (MAX_ROOMS salas x MAX_ENTITIES entidades por sala).
 */
#include "room_layout.h"

#include "cJSON.h"
#include "esp_log.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static const char *TAG = "ROOM_LAYOUT_SD";

#ifndef LAYOUT_SD_DIR
#define LAYOUT_SD_DIR   "/sd/assets/layout"
#endif
#define MAX_ROOMS        4
#define MAX_ENTITIES    64
#define MAX_NAME_LEN    24

/* ── armazenamento estatico ──────────────────────────────────────── */

static char              s_room_name[MAX_ROOMS][MAX_NAME_LEN];
static char              s_ent_name [MAX_ROOMS][MAX_ENTITIES][MAX_NAME_LEN];
static room_entity_def_t s_ent_buf  [MAX_ROOMS][MAX_ENTITIES];
static room_layout_t     s_rooms    [MAX_ROOMS];
static size_t            s_room_count = 0;

/* ── helpers de conversao ────────────────────────────────────────── */

static entity_type_t type_from_str(const char *s)
{
    if (!s)                          return ENTITY_TYPE_FURNITURE;
    if (strcmp(s, "player")    == 0) return ENTITY_TYPE_PLAYER;
    if (strcmp(s, "npc")       == 0) return ENTITY_TYPE_NPC;
    if (strcmp(s, "furniture") == 0) return ENTITY_TYPE_FURNITURE;
    if (strcmp(s, "prop")      == 0) return ENTITY_TYPE_PROP;
    if (strcmp(s, "trigger")   == 0) return ENTITY_TYPE_TRIGGER;
    return ENTITY_TYPE_FURNITURE;
}

static uint32_t flag_from_str(const char *s)
{
    if (!s)                               return 0;
    if (strcmp(s, "solid")        == 0)   return ENTITY_FLAG_SOLID;
    if (strcmp(s, "movable")      == 0)   return ENTITY_FLAG_MOVABLE;
    if (strcmp(s, "carryable")    == 0)   return ENTITY_FLAG_CARRYABLE;
    if (strcmp(s, "interactable") == 0)   return ENTITY_FLAG_INTERACTABLE;
    if (strcmp(s, "trigger")      == 0)   return ENTITY_FLAG_TRIGGER;
    if (strcmp(s, "visible")      == 0)   return ENTITY_FLAG_VISIBLE;
    if (strcmp(s, "ysorted")      == 0)   return ENTITY_FLAG_YSORTED;
    return 0;
}

/* ── leitura de arquivo ──────────────────────────────────────────── */

/* Le um arquivo inteiro para um buffer alocado via malloc.
 * Retorna NULL em falha. Caller deve free() o resultado. */
static char *read_file_alloc(const char *path)
{
    FILE *f = fopen(path, "r");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    rewind(f);
    char *buf = malloc((size_t)sz + 1);
    if (buf) {
        fread(buf, 1, (size_t)sz, f);
        buf[sz] = '\0';
    }
    fclose(f);
    return buf;
}

/* ── loader principal ────────────────────────────────────────────── */

static bool load_room_from_sd(const char *room_name)
{
    if (s_room_count >= MAX_ROOMS) {
        ESP_LOGW(TAG, "MAX_ROOMS=%d atingido, '%s' ignorada", MAX_ROOMS, room_name);
        return false;
    }

    char path[80];
    snprintf(path, sizeof(path), LAYOUT_SD_DIR "/%s.json", room_name);

    char *buf = read_file_alloc(path);
    if (!buf) {
        ESP_LOGW(TAG, "sem layout para '%s' (%s)", room_name, path);
        return false;
    }

    cJSON *root = cJSON_Parse(buf);
    free(buf);
    if (!root) {
        ESP_LOGE(TAG, "JSON invalido em %s", path);
        return false;
    }

    const size_t ri = s_room_count;

    strncpy(s_room_name[ri], room_name, MAX_NAME_LEN - 1);
    s_room_name[ri][MAX_NAME_LEN - 1] = '\0';

    cJSON *jbg = cJSON_GetObjectItemCaseSensitive(root, "bg_asset_id");
    s_rooms[ri].room        = s_room_name[ri];
    s_rooms[ri].bg_asset_id = (uint16_t)(jbg && cJSON_IsNumber(jbg) ? jbg->valueint : 0);
    s_rooms[ri].entities    = s_ent_buf[ri];
    s_rooms[ri].count       = 0;

    cJSON *jarr = cJSON_GetObjectItemCaseSensitive(root, "entities");
    int n = cJSON_GetArraySize(jarr);
    if (n > MAX_ENTITIES) {
        ESP_LOGW(TAG, "sala '%s': %d entidades, truncando a %d", room_name, n, MAX_ENTITIES);
        n = MAX_ENTITIES;
    }

    for (int i = 0; i < n; i++) {
        cJSON *ej = cJSON_GetArrayItem(jarr, i);
        room_entity_def_t *d = &s_ent_buf[ri][i];
        memset(d, 0, sizeof(*d));

        /* name — copiado para buffer estatico */
        cJSON *jn = cJSON_GetObjectItemCaseSensitive(ej, "name");
        const char *nstr = (jn && cJSON_IsString(jn)) ? jn->valuestring : "";
        strncpy(s_ent_name[ri][i], nstr, MAX_NAME_LEN - 1);
        s_ent_name[ri][i][MAX_NAME_LEN - 1] = '\0';
        d->name = s_ent_name[ri][i];

        /* asset */
        cJSON *jat = cJSON_GetObjectItemCaseSensitive(ej, "asset_type");
        cJSON *jai = cJSON_GetObjectItemCaseSensitive(ej, "asset_id");
        d->asset_type = (uint8_t)(jat && cJSON_IsNumber(jat) ? jat->valueint : 0);
        d->asset_id   = (uint16_t)(jai && cJSON_IsNumber(jai) ? jai->valueint : 0);

        /* type */
        cJSON *jtype = cJSON_GetObjectItemCaseSensitive(ej, "type");
        d->type = type_from_str(jtype ? cJSON_GetStringValue(jtype) : NULL);

        /* flags */
        cJSON *jflags = cJSON_GetObjectItemCaseSensitive(ej, "flags");
        uint32_t fbits = 0;
        cJSON *jf;
        cJSON_ArrayForEach(jf, jflags) {
            fbits |= flag_from_str(cJSON_GetStringValue(jf));
        }
        d->flags = fbits;

        /* posicao */
        cJSON *jx = cJSON_GetObjectItemCaseSensitive(ej, "x");
        cJSON *jy = cJSON_GetObjectItemCaseSensitive(ej, "y");
        d->x = (int16_t)(jx && cJSON_IsNumber(jx) ? jx->valueint : 0);
        d->y = (int16_t)(jy && cJSON_IsNumber(jy) ? jy->valueint : 0);

        /* colisao */
        cJSON *jcw = cJSON_GetObjectItemCaseSensitive(ej, "coll_w");
        cJSON *jch = cJSON_GetObjectItemCaseSensitive(ej, "coll_h");
        cJSON *jcx = cJSON_GetObjectItemCaseSensitive(ej, "coll_offset_x");
        cJSON *jcy = cJSON_GetObjectItemCaseSensitive(ej, "coll_offset_y");
        d->coll_w        = (int16_t)(jcw && cJSON_IsNumber(jcw) ? jcw->valueint : 0);
        d->coll_h        = (int16_t)(jch && cJSON_IsNumber(jch) ? jch->valueint : 0);
        d->coll_offset_x = (int16_t)(jcx && cJSON_IsNumber(jcx) ? jcx->valueint : 0);
        d->coll_offset_y = (int16_t)(jcy && cJSON_IsNumber(jcy) ? jcy->valueint : 0);

        /* sort_offset_y e from_image */
        cJSON *jso = cJSON_GetObjectItemCaseSensitive(ej, "sort_offset_y");
        cJSON *jfi = cJSON_GetObjectItemCaseSensitive(ej, "from_image");
        d->sort_offset_y = (int16_t)(jso && cJSON_IsNumber(jso) ? jso->valueint : 0);
        d->from_image    = cJSON_IsTrue(jfi);

        s_rooms[ri].count++;
    }

    cJSON_Delete(root);
    s_room_count++;
    ESP_LOGI(TAG, "sala '%s': %u entidades carregadas do SD",
             room_name, (unsigned)s_rooms[ri].count);
    return true;
}

/* ── API publica ─────────────────────────────────────────────────── */

const room_layout_t *room_layout_find(const char *room)
{
    if (!room) return NULL;

    /* cache hit */
    for (size_t i = 0; i < s_room_count; i++) {
        if (strcmp(s_rooms[i].room, room) == 0) return &s_rooms[i];
    }

    /* cache miss: carrega do SD */
    if (load_room_from_sd(room)) {
        return &s_rooms[s_room_count - 1];
    }
    return NULL;
}
