#ifndef M4A_PCM_MIXER_MODE_H
#define M4A_PCM_MIXER_MODE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "m4a_driver.h"

#ifdef __cplusplus
extern "C"
{
#endif

    /* Return the stable product name for a valid mixer mode, or NULL. */
    const char* m4a_pcm_mixer_name(M4APcmMixerMode mode);

    /* Parse one exact, case-sensitive product name into the driver enum. */
    bool m4a_pcm_mixer_parse(const char* name, M4APcmMixerMode* mode);

    /* Decode the stable state/parameter byte without casting an invalid enum. */
    bool m4a_pcm_mixer_from_raw(uint8_t raw, M4APcmMixerMode* mode);

    /* Format the shared bounded diagnostic for an invalid product value. */
    void m4a_pcm_mixer_format_diagnostic(char* destination, size_t capacity, const char* value);

#ifdef __cplusplus
}
#endif

#endif
