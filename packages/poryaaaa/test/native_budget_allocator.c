#include "native_budget_allocator.h"

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

/* This translation unit is compiled normally, not with engine allocator macros.
 * A fixed external ledger leaves libc alignment and payload layout untouched. */
static struct
{
    void* pointer;
    size_t size;
} ledger[16384];
static NativeBudget budget;
static size_t failAt;

/* Tracking bugs invalidate the failure sweep instead of masquerading as loader failures. */
static void ledger_error(const char* message)
{
    fprintf(stderr, "allocation test infrastructure: %s\n", message);
    abort();
}

/* Locate existing ownership or an empty slot without allocating inside the allocator. */
static size_t ledger_find(void* pointer)
{
    for (size_t i = 0; i < sizeof(ledger) / sizeof(ledger[0]); i++)
        if (ledger[i].pointer == pointer)
            return i;
    ledger_error(pointer ? "free/realloc of an untracked pointer" : "ledger capacity exhausted");
    return 0;
}

/* Fail one deterministic engine request while leaving later cleanup and retries usable. */
static int reject_request(void)
{
    budget.requests++;
    if (failAt && budget.requests == failAt)
    {
        budget.injectedFailures++;
        errno = ENOMEM;
        return 1;
    }
    return 0;
}

/* Account for real libc payloads without adding headers or changing alignment. */
static void* record_allocation(void* pointer, size_t size)
{
    if (pointer)
    {
        size_t i = ledger_find(NULL);
        ledger[i].pointer = pointer;
        ledger[i].size = size;
        budget.liveAllocations++;
        budget.liveBytes += size;
    }
    return pointer;
}

/* Start another measurement without discarding allocations owned by the open project. */
void native_budget_begin(size_t failAtRequest)
{
    budget.requests = 0;
    budget.injectedFailures = 0;
    failAt = failAtRequest;
}

/* Capture ownership and traffic together for failure-boundary assertions. */
NativeBudget native_budget_snapshot(void)
{
    return budget;
}

/* Intercept engine requests only; fixture and adapter allocation remains ordinary libc. */
void* poryaaaa_budget_malloc(size_t size)
{
    return reject_request() ? NULL : record_allocation(malloc(size), size);
}

/* Preserve calloc overflow and zero-initialization semantics under fault injection. */
void* poryaaaa_budget_calloc(size_t count, size_t size)
{
    if (reject_request())
        return NULL;
    if (size && count > SIZE_MAX / size)
    {
        errno = ENOMEM;
        return NULL;
    }
    return record_allocation(calloc(count, size), count * size);
}

/* Reclaim exactly one tracked allocation, detecting ownership mistakes immediately. */
void poryaaaa_budget_free(void* pointer)
{
    if (!pointer)
        return;
    size_t i = ledger_find(pointer);
    budget.liveAllocations--;
    budget.liveBytes -= ledger[i].size;
    ledger[i].pointer = NULL;
    ledger[i].size = 0;
    free(pointer);
}

/* Failed growth retains the old allocation; successful growth replaces its ledger entry. */
void* poryaaaa_budget_realloc(void* pointer, size_t size)
{
    if (reject_request())
        return NULL; /* The original allocation and its contents remain owned. */
    if (!pointer)
        return record_allocation(realloc(NULL, size), size);
    if (!size)
    {
        poryaaaa_budget_free(pointer);
        return NULL;
    }
    size_t i = ledger_find(pointer);
    void* replacement = realloc(pointer, size);
    if (!replacement)
        return NULL;
    budget.liveBytes -= ledger[i].size;
    budget.liveBytes += size;
    ledger[i].pointer = replacement;
    ledger[i].size = size;
    return replacement;
}
