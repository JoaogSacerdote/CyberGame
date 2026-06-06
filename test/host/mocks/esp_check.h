#pragma once

#include "esp_err.h"
#include "esp_log.h"

#define ESP_RETURN_ON_ERROR(x, tag, ...) do { \
    esp_err_t _err_rc = (x); \
    if (_err_rc != ESP_OK) { \
        ESP_LOGE((tag), __VA_ARGS__); \
        return _err_rc; \
    } \
} while (0)

#define ESP_RETURN_ON_FALSE(cond, err_code, tag, ...) do { \
    if (!(cond)) { \
        ESP_LOGE((tag), __VA_ARGS__); \
        return (err_code); \
    } \
} while (0)
