#pragma once
#include <stdlib.h>
#include <stdint.h>

static inline uint32_t esp_random(void) {
    return (uint32_t)rand();
}

static inline void esp_fill_random(void *buf, size_t len) {
    uint8_t *p = (uint8_t *)buf;
    for (size_t i = 0; i < len; i++) {
        p[i] = (uint8_t)rand();
    }
}
