#ifndef VG_ALLOC_H
#define VG_ALLOC_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

static inline bool vg_size_add(size_t a, size_t b, size_t* out)
{
    if (a > SIZE_MAX - b)
        return false;
    *out = a + b;
    return true;
}

static inline bool vg_size_mul(size_t a, size_t b, size_t* out)
{
    if (a != 0 && b > SIZE_MAX / a)
        return false;
    *out = a * b;
    return true;
}

static inline void* vg_malloc_array(size_t count, size_t elemSize)
{
    size_t bytes;
    if (!vg_size_mul(count, elemSize, &bytes))
        return NULL;
    return malloc(bytes);
}

static inline void* vg_realloc_array(void* ptr, size_t count, size_t elemSize)
{
    size_t bytes;
    if (!vg_size_mul(count, elemSize, &bytes))
        return NULL;
    return realloc(ptr, bytes);
}

#endif /* VG_ALLOC_H */
