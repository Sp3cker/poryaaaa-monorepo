#include "m4a_pcm_mixer_mode.h"

#include <stdio.h>
#include <string.h>

typedef struct
{
    M4APcmMixerMode mode;
    uint8_t raw;
    const char* name;
} M4APcmMixerDescription;

static const M4APcmMixerDescription s_mixerModes[] = {
    {M4A_PCM_MIXER_IPATIX, (uint8_t)M4A_PCM_MIXER_IPATIX, "ipatix"},
    {M4A_PCM_MIXER_SAPPY, (uint8_t)M4A_PCM_MIXER_SAPPY, "sappy"},
};

const char* m4a_pcm_mixer_name(M4APcmMixerMode mode)
{
    for (size_t i = 0; i < sizeof(s_mixerModes) / sizeof(s_mixerModes[0]); ++i)
    {
        if (s_mixerModes[i].mode == mode)
            return s_mixerModes[i].name;
    }
    return NULL;
}

bool m4a_pcm_mixer_parse(const char* name, M4APcmMixerMode* mode)
{
    if (!name || !mode)
        return false;
    for (size_t i = 0; i < sizeof(s_mixerModes) / sizeof(s_mixerModes[0]); ++i)
    {
        if (strcmp(name, s_mixerModes[i].name) == 0)
        {
            *mode = s_mixerModes[i].mode;
            return true;
        }
    }
    return false;
}

bool m4a_pcm_mixer_from_raw(uint8_t raw, M4APcmMixerMode* mode)
{
    if (!mode)
        return false;
    for (size_t i = 0; i < sizeof(s_mixerModes) / sizeof(s_mixerModes[0]); ++i)
    {
        if (s_mixerModes[i].raw == raw)
        {
            *mode = s_mixerModes[i].mode;
            return true;
        }
    }
    return false;
}

void m4a_pcm_mixer_format_diagnostic(char* destination, size_t capacity, const char* value)
{
    if (!destination || capacity == 0)
        return;
    snprintf(destination, capacity, "invalid pcm_mixer value '%s'; expected ipatix or sappy", value ? value : "");
}
