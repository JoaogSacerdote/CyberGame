#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t storage_hal_init(void);

/* Le JEDEC ID do chip. manuf vai conter o byte de fabricante (0xEF Winbond),
 * device os 2 bytes do device ID (0xAA21 para W25N01GV). */
esp_err_t storage_hal_read_jedec_id(uint8_t *manuf, uint16_t *device);

#ifdef __cplusplus
}
#endif
