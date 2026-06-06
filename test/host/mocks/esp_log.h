#pragma once

#include <stdio.h>

#define ESP_LOGE(tag, ...) do { fprintf(stderr, "[E][%s] ", (tag)); fprintf(stderr, __VA_ARGS__); fputc('\n', stderr); } while (0)
#define ESP_LOGW(tag, ...) do { fprintf(stderr, "[W][%s] ", (tag)); fprintf(stderr, __VA_ARGS__); fputc('\n', stderr); } while (0)
#define ESP_LOGI(tag, ...) do { fprintf(stdout, "[I][%s] ", (tag)); fprintf(stdout, __VA_ARGS__); fputc('\n', stdout); } while (0)
#define ESP_LOGD(tag, ...) ((void)(tag))
#define ESP_LOGV(tag, ...) ((void)(tag))
