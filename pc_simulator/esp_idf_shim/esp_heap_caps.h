#pragma once
#include <stdlib.h>
#include <stdint.h>

#define MALLOC_CAP_SPIRAM    (1u << 3)
#define MALLOC_CAP_DMA       (1u << 1)
#define MALLOC_CAP_8BIT      (1u << 10)
#define MALLOC_CAP_INTERNAL  (1u << 11)
#define MALLOC_CAP_DEFAULT   MALLOC_CAP_8BIT

static inline void *heap_caps_malloc(size_t size, uint32_t caps) {
    (void)caps;
    return malloc(size);
}

static inline void heap_caps_free(void *ptr) {
    free(ptr);
}

static inline void *heap_caps_realloc(void *ptr, size_t size, uint32_t caps) {
    (void)caps;
    return realloc(ptr, size);
}

static inline void *heap_caps_calloc(size_t n, size_t size, uint32_t caps) {
    (void)caps;
    return calloc(n, size);
}
