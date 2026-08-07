#include "m4a_reverb.h"

/* The compatibility reverb helpers are static inline in the flat header.
 * m4a_engine.c is intentionally the sole engine compilation unit in both
 * current targets, while standalone consumers can compile this file safely. */
