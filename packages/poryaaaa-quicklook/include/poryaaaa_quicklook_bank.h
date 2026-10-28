#ifndef PORYAAAA_QUICKLOOK_BANK_H
#define PORYAAAA_QUICKLOOK_BANK_H

#include "voicegroup/voicegroup_types.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

    typedef struct PqlBank PqlBank;

    /* Validate and copy a v1 bank. The caller may release blob immediately.
     * Returns NULL on invalid input or allocation failure. No file I/O occurs. */
    PqlBank* pql_bank_load(const uint8_t* blob, size_t size);

    /* Borrow the 128 engine-ready voices, valid until pql_bank_free(bank).
     * Keep the bank alive until every engine using it has stopped. NULL-safe. */
    ToneData* pql_bank_voices(PqlBank* bank);

    /* Release the single backing buffer and handle. NULL-safe. */
    void pql_bank_free(PqlBank* bank);

#ifdef __cplusplus
}
#endif

#endif
