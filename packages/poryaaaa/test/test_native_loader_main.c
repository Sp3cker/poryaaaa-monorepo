#include "test_assert.h"

int tests_run;
int tests_passed;

int test_native_sample_labels(void);
void test_native_fallbacks(void);

/* Run native loader suites and report one process-wide result. */
int main(void)
{
    int fixtureStatus = test_native_sample_labels();
    test_native_fallbacks();
    printf("%d/%d tests passed\n", tests_passed, tests_run);
    return fixtureStatus || tests_passed != tests_run ? 1 : 0;
}
