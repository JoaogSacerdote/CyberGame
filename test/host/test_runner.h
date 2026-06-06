#pragma once

#include <stdio.h>

static int g_test_checks   = 0;
static int g_test_failures = 0;

#define TEST_ASSERT(cond) do { \
    g_test_checks++; \
    if (!(cond)) { \
        fprintf(stderr, "  FAIL: %s  (%s:%d)\n", #cond, __FILE__, __LINE__); \
        g_test_failures++; \
    } \
} while (0)

#define TEST_EQ_INT(actual, expected) do { \
    g_test_checks++; \
    long long _a = (long long)(actual); \
    long long _e = (long long)(expected); \
    if (_a != _e) { \
        fprintf(stderr, "  FAIL: %s == %s  (got %lld, want %lld)  (%s:%d)\n", \
                #actual, #expected, _a, _e, __FILE__, __LINE__); \
        g_test_failures++; \
    } \
} while (0)

#define TEST_RUN(fn) do { \
    fprintf(stdout, "RUN  %s\n", #fn); \
    fn(); \
} while (0)

#define TEST_SUMMARY() do { \
    fprintf(stdout, "----\n%d checks, %d failure(s)\n", g_test_checks, g_test_failures); \
    return g_test_failures == 0 ? 0 : 1; \
} while (0)
