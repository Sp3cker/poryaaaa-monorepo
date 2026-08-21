#ifndef M4A_PARAMS_H
#define M4A_PARAMS_H

#include <stdint.h>
#include <stdbool.h>
#include <clap/clap.h>
#include "m4a/m4a_driver.h"

typedef struct M4APluginData M4APluginData;

enum
{
    M4A_PLUGIN_TRACK_COUNT = 16,
};

typedef enum
{
    M4A_PARAM_PROGRAM_BASE = 0,
    M4A_PARAM_PCM_MIXER = M4A_PARAM_PROGRAM_BASE + M4A_PLUGIN_TRACK_COUNT,
    M4A_PARAM_COUNT = M4A_PARAM_PCM_MIXER + 1,
} M4AParamId;

enum
{
    M4A_PCM_MIXER_PARAM_MIN = M4A_PCM_MIXER_IPATIX,
    M4A_PCM_MIXER_PARAM_MAX = M4A_PCM_MIXER_SAPPY,
};

#ifdef __cplusplus
extern "C"
{
#endif

    bool m4a_params_get_pcm_mixer(const M4APluginData* data, M4APcmMixerMode* mode);
    void m4a_params_set_pcm_mixer(M4APluginData* data, M4APcmMixerMode mode);
    bool m4a_params_request_gui_pcm_mixer(M4APluginData* data, M4APcmMixerMode mode);
    void m4a_params_mark_pcm_mixer_pending(M4APluginData* data);
    bool m4a_params_consume_pcm_mixer(M4APluginData* data);

    void m4a_params_init(M4APluginData* data);
    void m4a_params_set_program(M4APluginData* data, int trackIndex, uint8_t program);
    void m4a_params_sync_to_driver(M4APluginData* data);
    void m4a_params_process_event(M4APluginData* data, const clap_event_param_value_t* ev);
    bool m4a_params_state_save(M4APluginData* data, const clap_ostream_t* stream);
    void m4a_params_state_load(M4APluginData* data, const clap_istream_t* stream);
    const clap_plugin_params_t* m4a_params_extension(void);

#ifdef __cplusplus
}
#endif

#endif /* M4A_PARAMS_H */
