#pragma once
#include "esp_err.h"
#include "esp_log.h"

#define ESP_RETURN_ON_ERROR(x, tag, fmt, ...) do { \
    esp_err_t _err_ = (x); \
    if (_err_ != ESP_OK) { \
        ESP_LOGE(tag, fmt, ##__VA_ARGS__); \
        return _err_; \
    } \
} while (0)

#define ESP_RETURN_ON_FALSE(a, err_code, tag, fmt, ...) do { \
    if (!(a)) { \
        ESP_LOGE(tag, fmt, ##__VA_ARGS__); \
        return err_code; \
    } \
} while (0)

#define ESP_GOTO_ON_ERROR(x, label, tag, fmt, ...) do { \
    esp_err_t _err_ = (x); \
    if (_err_ != ESP_OK) { \
        ESP_LOGE(tag, fmt, ##__VA_ARGS__); \
        goto label; \
    } \
} while (0)

#define ESP_GOTO_ON_FALSE(a, err_code, label, tag, fmt, ...) do { \
    if (!(a)) { \
        ESP_LOGE(tag, fmt, ##__VA_ARGS__); \
        goto label; \
    } \
} while (0)

#define ESP_ERROR_CHECK(x) do { \
    esp_err_t _err_ = (x); \
    if (_err_ != ESP_OK) { \
        ESP_LOGE("CHECK", "Error %d: %s", _err_, esp_err_to_name(_err_)); \
        abort(); \
    } \
} while (0)
