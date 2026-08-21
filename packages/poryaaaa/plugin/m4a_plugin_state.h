#ifndef M4A_PLUGIN_STATE_H
#define M4A_PLUGIN_STATE_H

#include <stdbool.h>
#include <stdint.h>

#include <clap/clap.h>

#include "m4a/m4a_driver.h"

enum
{
    M4A_PLUGIN_STATE_TRACK_COUNT = 16,
    M4A_PLUGIN_STATE_PROJECT_ROOT_CAPACITY = 512,
    M4A_PLUGIN_STATE_VOICEGROUP_CAPACITY = 256,
    M4A_PLUGIN_STATE_RECORDER_PATH_CAPACITY = 512,
};

typedef struct
{
    char projectRoot[M4A_PLUGIN_STATE_PROJECT_ROOT_CAPACITY];
    char voicegroupName[M4A_PLUGIN_STATE_VOICEGROUP_CAPACITY];
    uint8_t volume;
    uint8_t reverbAmount;
    M4APcmMixerMode mixerMode;
    uint8_t programs[M4A_PLUGIN_STATE_TRACK_COUNT];
    bool recorderArmed;
    char recorderPath[M4A_PLUGIN_STATE_RECORDER_PATH_CAPACITY];
} M4APluginStateData;

/* Encode one padding-free version-3 transaction to the CLAP stream. */
bool m4a_plugin_state_write(const clap_ostream_t* stream, const M4APluginStateData* state);

/* Decode v3, v2, or unversioned state completely before the caller mutates runtime state. */
bool m4a_plugin_state_read(const clap_istream_t* stream, M4APluginStateData* state);

#endif
