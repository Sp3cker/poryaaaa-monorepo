#ifndef PORYAAAA_TEST_ALLOCATOR_WRAP_H
#define PORYAAAA_TEST_ALLOCATOR_WRAP_H

#include <stddef.h>
#include <stdint.h>

#if defined(__APPLE__) || defined(__ELF__) || defined(_MSC_VER)

#    ifdef __cplusplus
extern "C"
{
#    endif

    extern uint64_t poryaaaa_test_malloc_calls;
    extern uint64_t poryaaaa_test_calloc_calls;
    extern uint64_t poryaaaa_test_realloc_calls;
    extern uint64_t poryaaaa_test_free_calls;

    void* poryaaaa_test_malloc(size_t size);
    void* poryaaaa_test_calloc(size_t count, size_t size);
    void* poryaaaa_test_realloc(void* pointer, size_t size);
    void poryaaaa_test_free(void* pointer);

#    ifdef __cplusplus
}
#    endif

/* This header is force-included only for driver translation units.  The
 * wrapper implementation is compiled separately without -include/FI. */
#    define malloc poryaaaa_test_malloc
#    define calloc poryaaaa_test_calloc
#    define realloc poryaaaa_test_realloc
#    define free poryaaaa_test_free

#endif

#endif
