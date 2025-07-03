#ifndef TEST_FRAMEWORK_H
#define TEST_FRAMEWORK_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TEST_PASS 0
#define TEST_FAIL 1

typedef struct {
    int total_tests;
    int passed_tests;
    int failed_tests;
} TestStats;

extern TestStats g_test_stats;

#define TEST_ASSERT(condition, message) \
    do { \
        g_test_stats.total_tests++; \
        if (condition) { \
            g_test_stats.passed_tests++; \
            printf("PASS: %s\n", message); \
        } else { \
            g_test_stats.failed_tests++; \
            printf("FAIL: %s\n", message); \
        } \
    } while(0)

#define TEST_ASSERT_EQUAL(expected, actual, message) \
    do { \
        g_test_stats.total_tests++; \
        if ((expected) == (actual)) { \
            g_test_stats.passed_tests++; \
            printf("PASS: %s\n", message); \
        } else { \
            g_test_stats.failed_tests++; \
            printf("FAIL: %s (expected %ld, got %ld)\n", message, (long)(expected), (long)(actual)); \
        } \
    } while(0)

#define TEST_ASSERT_PTR_EQUAL(expected, actual, message) \
    do { \
        g_test_stats.total_tests++; \
        if ((expected) == (actual)) { \
            g_test_stats.passed_tests++; \
            printf("PASS: %s\n", message); \
        } else { \
            g_test_stats.failed_tests++; \
            printf("FAIL: %s (expected %p, got %p)\n", message, (void*)(expected), (void*)(actual)); \
        } \
    } while(0)

#define TEST_ASSERT_STRING_EQUAL(expected, actual, message) \
    do { \
        g_test_stats.total_tests++; \
        if (strcmp((expected), (actual)) == 0) { \
            g_test_stats.passed_tests++; \
            printf("PASS: %s\n", message); \
        } else { \
            g_test_stats.failed_tests++; \
            printf("FAIL: %s (expected '%s', got '%s')\n", message, expected, actual); \
        } \
    } while(0)

#define TEST_ASSERT_NOT_NULL(ptr, message) \
    do { \
        g_test_stats.total_tests++; \
        if ((ptr) != NULL) { \
            g_test_stats.passed_tests++; \
            printf("PASS: %s\n", message); \
        } else { \
            g_test_stats.failed_tests++; \
            printf("FAIL: %s (pointer is NULL)\n", message); \
        } \
    } while(0)

#define TEST_ASSERT_NULL(ptr, message) \
    do { \
        g_test_stats.total_tests++; \
        if ((ptr) == NULL) { \
            g_test_stats.passed_tests++; \
            printf("PASS: %s\n", message); \
        } else { \
            g_test_stats.failed_tests++; \
            printf("FAIL: %s (pointer is not NULL)\n", message); \
        } \
    } while(0)

#define RUN_TEST(test_func) \
    do { \
        printf("\n=== Running %s ===\n", #test_func); \
        test_func(); \
    } while(0)

void test_init(void);
void test_summary(void);

#endif