#ifndef TDD_FRAMEWORK_H
#define TDD_FRAMEWORK_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define RED     "\x1b[31m"
#define GREEN   "\x1b[32m"
#define YELLOW  "\x1b[33m"
#define RESET   "\x1b[0m"

typedef struct {
    int total_tests;
    int passed_tests;
    int failed_tests;
    clock_t start_time;
} test_stats_t;

static test_stats_t g_test_stats = {0};

#define TEST_ASSERT(condition, message) do { \
    g_test_stats.total_tests++; \
    if (!(condition)) { \
        g_test_stats.failed_tests++; \
        printf(RED "✗ FAIL" RESET " %s:%d - %s\n", __FILE__, __LINE__, message); \
        return 0; \
    } else { \
        g_test_stats.passed_tests++; \
    } \
} while(0)

#define TEST_ASSERT_EQ(expected, actual) do { \
    g_test_stats.total_tests++; \
    if ((expected) != (actual)) { \
        g_test_stats.failed_tests++; \
        printf(RED "✗ FAIL" RESET " %s:%d - Expected %d, got %d\n", __FILE__, __LINE__, (int)(expected), (int)(actual)); \
        return 0; \
    } else { \
        g_test_stats.passed_tests++; \
    } \
} while(0)

#define TEST_ASSERT_STR_EQ(expected, actual) do { \
    g_test_stats.total_tests++; \
    if ((expected) == NULL && (actual) == NULL) { \
        g_test_stats.passed_tests++; \
    } else if ((expected) == NULL || (actual) == NULL) { \
        g_test_stats.failed_tests++; \
        printf(RED "✗ FAIL" RESET " %s:%d - Expected '%s', got '%s'\n", __FILE__, __LINE__, \
               (expected) ? (expected) : "NULL", (actual) ? (actual) : "NULL"); \
        return 0; \
    } else if (strcmp(expected, actual) != 0) { \
        g_test_stats.failed_tests++; \
        printf(RED "✗ FAIL" RESET " %s:%d - Expected '%s', got '%s'\n", __FILE__, __LINE__, expected, actual); \
        return 0; \
    } else { \
        g_test_stats.passed_tests++; \
    } \
} while(0)

#define RUN_TEST(test_func) do { \
    printf("\nRunning: %s\n", #test_func); \
    if (test_func()) { \
        printf(GREEN "✓ PASS" RESET " %s\n", #test_func); \
    } \
} while(0)

#define TEST_SUITE_BEGIN() do { \
    g_test_stats.start_time = clock(); \
    printf("\n" YELLOW "=== TDD Test Suite Starting ===" RESET "\n"); \
} while(0)

#define TEST_SUITE_END() do { \
    double elapsed = (double)(clock() - g_test_stats.start_time) / CLOCKS_PER_SEC; \
    printf("\n" YELLOW "=== Test Summary ===" RESET "\n"); \
    printf("Total tests: %d\n", g_test_stats.total_tests); \
    printf(GREEN "Passed: %d" RESET "\n", g_test_stats.passed_tests); \
    printf(RED "Failed: %d" RESET "\n", g_test_stats.failed_tests); \
    printf("Time elapsed: %.3f seconds\n", elapsed); \
    printf("Success rate: %.1f%%\n", \
           g_test_stats.total_tests > 0 ? \
           (100.0 * g_test_stats.passed_tests / g_test_stats.total_tests) : 0); \
} while(0)

#endif