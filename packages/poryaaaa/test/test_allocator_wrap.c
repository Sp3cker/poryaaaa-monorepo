#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

uint64_t poryaaaa_test_malloc_calls;
uint64_t poryaaaa_test_calloc_calls;
uint64_t poryaaaa_test_realloc_calls;
uint64_t poryaaaa_test_free_calls;

void* poryaaaa_test_malloc(size_t size)
{
    poryaaaa_test_malloc_calls++;
    return malloc(size);
}

void* poryaaaa_test_calloc(size_t count, size_t size)
{
    poryaaaa_test_calloc_calls++;
    return calloc(count, size);
}

void* poryaaaa_test_realloc(void* pointer, size_t size)
{
    poryaaaa_test_realloc_calls++;
    return realloc(pointer, size);
}

void poryaaaa_test_free(void* pointer)
{
    poryaaaa_test_free_calls++;
    free(pointer);
}
