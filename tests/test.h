/*
 * test.h — минимальный тестовый каркас без зависимостей.
 * Проверка не прерывает тест: выводит место ошибки и считает провалы.
 */
#ifndef TEST_H
#define TEST_H

#include <stdio.h>

extern int g_checks;
extern int g_failures;

#define CHECK(cond)                                                          \
    do {                                                                     \
        g_checks++;                                                          \
        if (!(cond)) {                                                       \
            g_failures++;                                                    \
            printf("    FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);       \
        }                                                                    \
    } while (0)

#define CHECK_EQ(a, b)                                                       \
    do {                                                                     \
        long long va_ = (long long)(a), vb_ = (long long)(b);                \
        g_checks++;                                                          \
        if (va_ != vb_) {                                                    \
            g_failures++;                                                    \
            printf("    FAIL %s:%d: %s == %s (%lld != %lld)\n", __FILE__,    \
                   __LINE__, #a, #b, va_, vb_);                              \
        }                                                                    \
    } while (0)

#define RUN(test)                                                            \
    do {                                                                     \
        printf("  %s\n", #test);                                             \
        test();                                                              \
    } while (0)

void run_ringbuf_tests(void);
void run_rx_tracker_tests(void);
void run_cli_tests(void);
void run_strutil_tests(void);

#endif /* TEST_H */
