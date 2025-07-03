#include "test_framework.h"

TestStats g_test_stats = {0, 0, 0};

void test_init(void) {
    g_test_stats.total_tests = 0;
    g_test_stats.passed_tests = 0;
    g_test_stats.failed_tests = 0;
    printf("Starting test suite...\n");
}

void test_summary(void) {
    printf("\n=== Test Summary ===\n");
    printf("Total tests: %d\n", g_test_stats.total_tests);
    printf("Passed: %d\n", g_test_stats.passed_tests);
    printf("Failed: %d\n", g_test_stats.failed_tests);
    printf("Success rate: %.2f%%\n", 
           g_test_stats.total_tests > 0 ? 
           (float)g_test_stats.passed_tests / g_test_stats.total_tests * 100 : 0);
    
    if (g_test_stats.failed_tests > 0) {
        printf("Some tests failed!\n");
    } else {
        printf("All tests passed!\n");
    }
}