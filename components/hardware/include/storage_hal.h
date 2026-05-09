#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Geometria do W25N01GV */
#define STORAGE_PAGE_SIZE        2048u    /* bytes uteis por pagina */
#define STORAGE_PAGES_PER_BLOCK  64u
#define STORAGE_BLOCK_COUNT      1024u
#define STORAGE_BLOCK_SIZE       (STORAGE_PAGE_SIZE * STORAGE_PAGES_PER_BLOCK)  /* 128 KB */
#define STORAGE_TOTAL_PAGES      (STORAGE_BLOCK_COUNT * STORAGE_PAGES_PER_BLOCK)

esp_err_t storage_hal_init(void);

/* Le JEDEC ID do chip. manuf vai conter o byte de fabricante (0xEF Winbond),
 * device os 2 bytes do device ID (0xAA21 para W25N01GV). */
esp_err_t storage_hal_read_jedec_id(uint8_t *manuf, uint16_t *device);

/* Operacoes em pagina (2048 bytes). page e o indice global (0..65535). */
esp_err_t storage_hal_read_page(uint32_t page, uint8_t *buf);
esp_err_t storage_hal_write_page(uint32_t page, const uint8_t *buf);

/* Apaga um bloco (128 KB = 64 paginas). Bloco precisa ser apagado antes de
 * qualquer escrita — NAND so consegue mudar bits 1->0 em escrita normal. */
esp_err_t storage_hal_erase_block(uint32_t block);

/* Le o marcador de bad block (primeiro byte da spare area da pagina 0 do bloco).
 * Retorna true se o bloco esta marcado como ruim (de fabrica ou pelo sistema). */
bool      storage_hal_is_block_bad(uint32_t block);

/* POST destrutivo: erase + write + read + compare em bloco reservado.
 * Roda em modo RECOVERY para nao tocar em assets reais do jogo. */
esp_err_t storage_hal_test_write_cycle(void);

#ifdef __cplusplus
}
#endif
