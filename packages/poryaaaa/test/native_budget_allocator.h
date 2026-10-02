#ifndef NATIVE_BUDGET_ALLOCATOR_H
#define NATIVE_BUDGET_ALLOCATOR_H

#include <stddef.h>

typedef struct
{
    size_t requests;
    size_t injectedFailures;
    size_t liveAllocations;
    size_t liveBytes;
} NativeBudget;

/* Reset request accounting, never ownership. Zero disables failure injection. */
void native_budget_begin(size_t failAtRequest);
NativeBudget native_budget_snapshot(void);
void* poryaaaa_budget_malloc(size_t size);
void* poryaaaa_budget_calloc(size_t count, size_t size);
void* poryaaaa_budget_realloc(void* pointer, size_t size);
void poryaaaa_budget_free(void* pointer);

#endif
