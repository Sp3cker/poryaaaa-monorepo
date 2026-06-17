#ifndef VG_VOICE_MACRO_H
#define VG_VOICE_MACRO_H

#include "voicegroup_types.h"

#include <ctype.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

typedef enum
{
    VG_MACRO_DIRECTSOUND,
    VG_MACRO_SQUARE_1,
    VG_MACRO_SQUARE_2,
    VG_MACRO_PROG_WAVE,
    VG_MACRO_NOISE,
    VG_MACRO_KEYSPLIT,
    VG_MACRO_KEYSPLIT_ALL,
    VG_MACRO_CRY,
} VoicegroupMacroKind;

typedef struct
{
    const char* keyword;
    uint8_t typeCode;
    VoicegroupMacroKind kind;
} VoicegroupMacro;

/*
 * Match order matters: more specific variants (_no_resample, _alt)
 * MUST come before their shorter base forms so the prefix match
 * doesn't fire early.
 */
static const VoicegroupMacro vg_voice_macros[] = {
    {"voice_directsound_no_resample", VOICE_DIRECTSOUND_NO_RESAMPLE, VG_MACRO_DIRECTSOUND},
    {"voice_directsound_alt", VOICE_DIRECTSOUND_ALT, VG_MACRO_DIRECTSOUND},
    {"voice_directsound", VOICE_DIRECTSOUND, VG_MACRO_DIRECTSOUND},
    {"voice_square_1_alt", VOICE_SQUARE_1_ALT, VG_MACRO_SQUARE_1},
    {"voice_square_1", VOICE_SQUARE_1, VG_MACRO_SQUARE_1},
    {"voice_square_2_alt", VOICE_SQUARE_2_ALT, VG_MACRO_SQUARE_2},
    {"voice_square_2", VOICE_SQUARE_2, VG_MACRO_SQUARE_2},
    {"voice_programmable_wave_alt", VOICE_PROGRAMMABLE_WAVE_ALT, VG_MACRO_PROG_WAVE},
    {"voice_programmable_wave", VOICE_PROGRAMMABLE_WAVE, VG_MACRO_PROG_WAVE},
    {"voice_noise_alt", VOICE_NOISE_ALT, VG_MACRO_NOISE},
    {"voice_noise", VOICE_NOISE, VG_MACRO_NOISE},
    {"voice_keysplit_all", VOICE_KEYSPLIT_ALL, VG_MACRO_KEYSPLIT_ALL},
    {"voice_keysplit", VOICE_KEYSPLIT, VG_MACRO_KEYSPLIT},
    {"cry_reverse", VOICE_CRY_REVERSE, VG_MACRO_CRY},
    {"cry", VOICE_CRY, VG_MACRO_CRY},
    {NULL, 0, 0},
};

static inline bool vg_voice_macro_match(const char* line, const VoicegroupMacro** outMacro, const char** outArgs)
{
    for (const VoicegroupMacro* macro = vg_voice_macros; macro->keyword; macro++)
    {
        size_t len = strlen(macro->keyword);
        if (strncmp(line, macro->keyword, len) != 0 || !isspace((unsigned char)line[len]))
            continue;
        const char* args = line + len + 1;
        while (*args && isspace((unsigned char)*args))
            args++;
        *outMacro = macro;
        *outArgs = args;
        return true;
    }
    return false;
}

#endif /* VG_VOICE_MACRO_H */
