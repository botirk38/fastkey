#include "test_framework.h"

// Test function declarations
void run_redis_store_tests(void);
void run_resp_tests(void);
void run_command_tests(void);
void run_stream_tests(void);
void run_integration_tests(void);

int main(void) {
    test_init();
    
    // Run all test suites
    run_redis_store_tests();
    run_resp_tests();
    run_command_tests();
    run_stream_tests();
    run_integration_tests();
    
    // Print summary
    test_summary();
    
    return (g_test_stats.failed_tests > 0) ? 1 : 0;
}