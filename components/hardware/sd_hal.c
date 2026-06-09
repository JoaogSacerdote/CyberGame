#include "sd_hal.h"

#include <dirent.h>
#include <stdio.h>
#include <string.h>

#include "board_pins.h"
#include "driver/gpio.h"
#include "driver/sdspi_host.h"
#include "driver/spi_common.h"
#include "sdmmc_cmd.h"
#include "esp_vfs_fat.h"
#include "esp_log.h"

static const char *TAG = "SD_HAL";

/* Mesmo host SPI do display (board_pins.h documenta o compartilhamento). */
#define SD_SPI_HOST   SPI2_HOST

static sdmmc_card_t *s_card = NULL;

esp_err_t sd_hal_init(void)
{
    if (s_card != NULL) {
        return ESP_OK;   /* idempotente */
    }

    /* Pull-ups internos nas linhas do SD. SD-sobre-SPI espera pull-up em
     * MISO e CS; se o modulo nao tiver os seus, a init pode dar timeout
     * (0x107). Os internos do ESP (~45k) sao fracos mas costumam destravar.
     * NOTA: o display compartilha MOSI/SCK e funciona, entao se o SD nao
     * monta o suspeito sao as linhas EXCLUSIVAS do cartao: MISO(18) e CS(47). */
    gpio_set_pull_mode(BOARD_PIN_SD_MISO, GPIO_PULLUP_ONLY);
    gpio_set_pull_mode(BOARD_PIN_SD_CS,   GPIO_PULLUP_ONLY);

    const esp_vfs_fat_mount_config_t mount_cfg = {
        .format_if_mount_failed = false,   /* nunca formata sozinho */
        .max_files            = 5,
        .allocation_unit_size = 16 * 1024,
    };

    /* O barramento SPI2 ja foi inicializado pelo display_hal — aqui so
     * anexamos o cartao como mais um device (CS proprio). NAO chamar
     * spi_bus_initialize de novo. */
    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    host.slot = SD_SPI_HOST;
    host.max_freq_khz = 50000;  /* testado estavel a 50 MHz neste cartao */

    sdspi_device_config_t slot_cfg = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_cfg.gpio_cs = BOARD_PIN_SD_CS;
    slot_cfg.host_id = SD_SPI_HOST;

    const esp_err_t err =
        esp_vfs_fat_sdspi_mount(SD_MOUNT_POINT, &host, &slot_cfg, &mount_cfg, &s_card);

    if (err != ESP_OK) {
        if (err == ESP_FAIL) {
            ESP_LOGE(TAG, "Cartao detectado mas sem FAT. Formate em FAT32.");
        } else {
            ESP_LOGE(TAG, "Falha ao montar o microSD (%s). Cheque: cartao inserido, "
                          "MISO=GPIO%d, SD_CS=GPIO%d, barramento SPI2 ativo.",
                     esp_err_to_name(err), BOARD_PIN_SD_MISO, BOARD_PIN_SD_CS);
        }
        s_card = NULL;
        return err;
    }

    ESP_LOGI(TAG, "microSD montado em %s:", SD_MOUNT_POINT);
    sdmmc_card_print_info(stdout, s_card);
    return ESP_OK;
}

bool sd_hal_is_mounted(void)
{
    return s_card != NULL;
}

static void list_dir(const char *path)
{
    DIR *dir = opendir(path);
    if (dir == NULL) {
        ESP_LOGW(TAG, "opendir(%s) falhou — pasta nao existe?", path);
        return;
    }
    ESP_LOGI(TAG, "Conteudo de %s:", path);
    int count = 0;
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        ESP_LOGI(TAG, "  %s%s", entry->d_name,
                 (entry->d_type == DT_DIR) ? "/" : "");
        count++;
    }
    closedir(dir);
    ESP_LOGI(TAG, "%d entrada(s) em %s.", count, path);
}

esp_err_t sd_hal_list_root(void)
{
    if (s_card == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    list_dir(SD_MOUNT_POINT);
    /* Lista tambem a pasta que o jogo procura — mostra os nomes exatos. */
    list_dir(SD_MOUNT_POINT "/assets");
    return ESP_OK;
}

esp_err_t sd_hal_selftest(void)
{
    if (s_card == NULL) {
        ESP_LOGE(TAG, "selftest: cartao nao montado");
        return ESP_ERR_INVALID_STATE;
    }

    const char *path = SD_MOUNT_POINT "/_selftest.txt";
    const char *msg  = "CyberGame SD selftest OK";

    /* 1. escreve */
    FILE *f = fopen(path, "w");
    if (f == NULL) {
        ESP_LOGE(TAG, "selftest: nao criou %s (cartao protegido contra escrita?)", path);
        return ESP_FAIL;
    }
    const int n = fprintf(f, "%s\n", msg);
    fclose(f);
    if (n < 0) {
        ESP_LOGE(TAG, "selftest: fprintf falhou");
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "selftest: escreveu %d bytes em %s", n, path);

    /* 2. le de volta */
    f = fopen(path, "r");
    if (f == NULL) {
        ESP_LOGE(TAG, "selftest: nao reabriu %s pra leitura", path);
        return ESP_FAIL;
    }
    char buf[64] = {0};
    char *r = fgets(buf, sizeof(buf), f);
    fclose(f);
    if (r == NULL) {
        ESP_LOGE(TAG, "selftest: leitura falhou");
        return ESP_FAIL;
    }
    buf[strcspn(buf, "\r\n")] = '\0';   /* tira o \n */

    /* 3. compara */
    if (strcmp(buf, msg) != 0) {
        ESP_LOGE(TAG, "selftest: conteudo DIFERENTE: leu '%s', esperava '%s'", buf, msg);
        return ESP_ERR_INVALID_RESPONSE;
    }

    ESP_LOGI(TAG, "selftest: leu de volta OK: '%s'", buf);
    ESP_LOGI(TAG, "=== SD selftest PASSOU (escrita + leitura OK) ===");
    return ESP_OK;
}
