#ifndef VG_ALLOC_H
#define VG_ALLOC_H

#include <stdbool.h>
#include <stddef.h>
#include <stdckdint.h>
#include <stdlib.h>

static inline bool vg_size_add(size_t a, size_t b, size_t *out)
{
    return !ckd_add(out, a, b);
}

static inline bool vg_size_mul(size_t a, size_t b, size_t *out)
{
    return !ckd_mul(out, a, b);
}

static inline void *vg_malloc_array(size_t count, size_t elemSize)
{
    size_t bytes;
    if (!vg_size_mul(count, elemSize, &bytes))
        return NULL;
    return malloc(bytes);
}

static inline void *vg_realloc_array(void *ptr, size_t count, size_t elemSize)
{
    size_t bytes;
    if (!vg_size_mul(count, elemSize, &bytes))
        return NULL;
    return realloc(ptr, bytes);
}

#endif /* VG_ALLOC_H */
