#include "m4a_params.h"

#include <math.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "m4a_plugin.h"
#include "m4a/m4a_pcm_mixer_mode.h"

enum
{
    PCM_MIXER_MODE_SHIFT = 0,
    PCM_MIXER_AUDIO_PENDING_SHIFT = 1,
    PCM_MIXER_GUI_PENDING_SHIFT = 2,
    PCM_MIXER_GUI_GENERATION_SHIFT = 3,
};

_Static_assert(M4A_PARAM_PROGRAM_BASE == 0, "program parameter IDs are stable");
_Static_assert(M4A_PARAM_PCM_MIXER == 16, "PCM mixer parameter ID is stable");

#define PCM_MIXER_MODE_MASK (1u << PCM_MIXER_MODE_SHIFT)
#define PCM_MIXER_AUDIO_PENDING_MASK (1u << PCM_MIXER_AUDIO_PENDING_SHIFT)
#define PCM_MIXER_GUI_PENDING_MASK (1u << PCM_MIXER_GUI_PENDING_SHIFT)
#define PCM_MIXER_GUI_GENERATION_INCREMENT (1u << PCM_MIXER_GUI_GENERATION_SHIFT)
#define PCM_MIXER_GUI_GENERATION_MASK (UINT_MAX << PCM_MIXER_GUI_GENERATION_SHIFT)

static uint8_t clamp_u8_param(double value, uint8_t minValue, uint8_t maxValue)
{
    int ivalue = (int)(value + (value >= 0.0 ? 0.5 : -0.5));
    if (ivalue < minValue)
        ivalue = minValue;
    if (ivalue > maxValue)
        ivalue = maxValue;
    return (uint8_t)ivalue;
}

static bool is_program_param(clap_id param_id, int* trackIndex)
{
    if (param_id < M4A_PARAM_PROGRAM_BASE || param_id >= M4A_PARAM_PROGRAM_BASE + M4A_PLUGIN_TRACK_COUNT)
        return false;
    if (trackIndex)
        *trackIndex = (int)(param_id - M4A_PARAM_PROGRAM_BASE);
    return true;
}

static unsigned pcm_mixer_mode_bits(M4APcmMixerMode mode)
{
    return ((unsigned)mode << PCM_MIXER_MODE_SHIFT) & PCM_MIXER_MODE_MASK;
}

static bool pcm_mixer_mode_from_frontend_state(unsigned state, M4APcmMixerMode* mode)
{
    const unsigned rawMode = (state & PCM_MIXER_MODE_MASK) >> PCM_MIXER_MODE_SHIFT;
    return m4a_pcm_mixer_from_raw((uint8_t)rawMode, mode);
}

static bool pcm_mixer_mode_from_value(double value, M4APcmMixerMode* mode)
{
    if (!mode || !isfinite(value))
        return false;
    if (value == (double)M4A_PCM_MIXER_IPATIX)
    {
        *mode = M4A_PCM_MIXER_IPATIX;
        return true;
    }
    if (value == (double)M4A_PCM_MIXER_SAPPY)
    {
        *mode = M4A_PCM_MIXER_SAPPY;
        return true;
    }
    return false;
}

void m4a_params_init(M4APluginData* data)
{
    for (int i = 0; i < M4A_PLUGIN_TRACK_COUNT; ++i)
        atomic_init(&data->programParams[i], (uint8_t)i);
    atomic_init(&data->pcmMixerFrontendState, pcm_mixer_mode_bits(M4A_PCM_MIXER_IPATIX));
}

void m4a_params_set_program(M4APluginData* data, int trackIndex, uint8_t program)
{
    if (trackIndex < 0 || trackIndex >= M4A_PLUGIN_TRACK_COUNT)
        return;

    atomic_store(&data->programParams[trackIndex], program);
}

bool m4a_params_get_pcm_mixer(const M4APluginData* data, M4APcmMixerMode* mode)
{
    if (!data || !mode)
        return false;
    return pcm_mixer_mode_from_frontend_state(atomic_load(&data->pcmMixerFrontendState), mode);
}

void m4a_params_set_pcm_mixer(M4APluginData* data, M4APcmMixerMode mode)
{
    if (!m4a_pcm_mixer_name(mode))
        return;

    unsigned current = atomic_load(&data->pcmMixerFrontendState);
    for (;;)
    {
        M4APcmMixerMode currentMode;
        if (!pcm_mixer_mode_from_frontend_state(current, &currentMode))
            return;

        unsigned next = current & (PCM_MIXER_GUI_GENERATION_MASK | PCM_MIXER_AUDIO_PENDING_MASK);
        next |= pcm_mixer_mode_bits(mode);
        if (currentMode != mode)
            next |= PCM_MIXER_AUDIO_PENDING_MASK;
        if (next == current || atomic_compare_exchange_weak(&data->pcmMixerFrontendState, &current, next))
            return;
    }
}

bool m4a_params_request_gui_pcm_mixer(M4APluginData* data, M4APcmMixerMode mode)
{
    if (!m4a_pcm_mixer_name(mode))
        return false;

    unsigned current = atomic_load(&data->pcmMixerFrontendState);
    for (;;)
    {
        M4APcmMixerMode currentMode;
        if (!pcm_mixer_mode_from_frontend_state(current, &currentMode) || currentMode == mode)
            return false;

        const unsigned currentGeneration = current & PCM_MIXER_GUI_GENERATION_MASK;
        const unsigned nextGeneration =
            (currentGeneration + PCM_MIXER_GUI_GENERATION_INCREMENT) & PCM_MIXER_GUI_GENERATION_MASK;
        const unsigned next =
            nextGeneration | pcm_mixer_mode_bits(mode) | PCM_MIXER_AUDIO_PENDING_MASK | PCM_MIXER_GUI_PENDING_MASK;
        if (atomic_compare_exchange_weak(&data->pcmMixerFrontendState, &current, next))
            return true;
    }
}

void m4a_params_mark_pcm_mixer_pending(M4APluginData* data)
{
    atomic_fetch_or(&data->pcmMixerFrontendState, PCM_MIXER_AUDIO_PENDING_MASK);
}

bool m4a_params_consume_pcm_mixer(M4APluginData* data)
{
    if (!data->driver)
        return false;

    unsigned current = atomic_load(&data->pcmMixerFrontendState);
    for (;;)
    {
        if ((current & PCM_MIXER_AUDIO_PENDING_MASK) == 0)
            return true;

        M4APcmMixerMode mode;
        if (!pcm_mixer_mode_from_frontend_state(current, &mode))
            return false;
        const unsigned next = current & ~PCM_MIXER_AUDIO_PENDING_MASK;
        if (atomic_compare_exchange_weak(&data->pcmMixerFrontendState, &current, next))
            return m4a_driver_set_pcm_mixer_mode(data->driver, mode);
    }
}

static void apply_param_value(M4APluginData* data, clap_id param_id, double value)
{
    int trackIndex;
    if (is_program_param(param_id, &trackIndex))
    {
        uint8_t program = clamp_u8_param(value, 0, 127);
        m4a_params_set_program(data, trackIndex, program);
        if (data->activated && data->driver)
            m4a_program_change(data->driver, trackIndex, program);
        return;
    }

    if (param_id == M4A_PARAM_PCM_MIXER)
    {
        M4APcmMixerMode mode;
        if (pcm_mixer_mode_from_value(value, &mode))
            m4a_params_set_pcm_mixer(data, mode);
    }
}

void m4a_params_sync_to_driver(M4APluginData* data)
{
    if (!data->driver)
        return;

    /* A fresh driver has no host-program state. Replay the stored selections
     * whenever activation, reset, or a voicegroup reload replaces it. */
    for (int trackIndex = 0; trackIndex < M4A_PLUGIN_TRACK_COUNT; ++trackIndex)
    {
        uint8_t program = atomic_load(&data->programParams[trackIndex]);
        m4a_program_change(data->driver, trackIndex, program);
    }
}

void m4a_params_process_event(M4APluginData* data, const clap_event_param_value_t* ev)
{
    if (!ev)
        return;
    apply_param_value(data, ev->param_id, ev->value);
}

static bool push_param_gesture(const clap_output_events_t* out, uint16_t type)
{
    clap_event_param_gesture_t event;
    memset(&event, 0, sizeof(event));
    event.header.size = sizeof(event);
    event.header.space_id = CLAP_CORE_EVENT_SPACE_ID;
    event.header.type = type;
    event.param_id = M4A_PARAM_PCM_MIXER;
    return out->try_push(out, &event.header);
}

static bool push_pcm_mixer_value(const clap_output_events_t* out, M4APcmMixerMode mode)
{
    clap_event_param_value_t event;
    memset(&event, 0, sizeof(event));
    event.header.size = sizeof(event);
    event.header.space_id = CLAP_CORE_EVENT_SPACE_ID;
    event.header.type = CLAP_EVENT_PARAM_VALUE;
    event.param_id = M4A_PARAM_PCM_MIXER;
    event.note_id = -1;
    event.port_index = -1;
    event.channel = -1;
    event.key = -1;
    event.value = (double)mode;
    return out->try_push(out, &event.header);
}

static void emit_gui_pcm_mixer_event(M4APluginData* data, const clap_output_events_t* out)
{
    unsigned request = atomic_load(&data->pcmMixerFrontendState);
    if (!out || (request & PCM_MIXER_GUI_PENDING_MASK) == 0)
        return;

    M4APcmMixerMode mode;
    if (!pcm_mixer_mode_from_frontend_state(request, &mode))
        return;
    if (!push_param_gesture(out, CLAP_EVENT_PARAM_GESTURE_BEGIN) || !push_pcm_mixer_value(out, mode) ||
        !push_param_gesture(out, CLAP_EVENT_PARAM_GESTURE_END))
        return;

    const unsigned emittedGeneration = request & PCM_MIXER_GUI_GENERATION_MASK;
    while ((request & PCM_MIXER_GUI_PENDING_MASK) != 0 &&
           (request & PCM_MIXER_GUI_GENERATION_MASK) == emittedGeneration)
    {
        const unsigned consumed = request & ~PCM_MIXER_GUI_PENDING_MASK;
        if (atomic_compare_exchange_weak(&data->pcmMixerFrontendState, &request, consumed))
            return;
    }
}

static void params_flush(const clap_plugin_t* plugin, const clap_input_events_t* in, const clap_output_events_t* out)
{
    M4APluginData* data = (M4APluginData*)plugin->plugin_data;

    /* A GUI request is logically queued before the host flush that it
     * requested. Host events in this flush are then applied in their listed
     * order and therefore win over the GUI request when later. */
    emit_gui_pcm_mixer_event(data, out);

    if (!in)
        return;
    uint32_t eventCount = in->size(in);
    for (uint32_t i = 0; i < eventCount; ++i)
    {
        const clap_event_header_t* hdr = in->get(in, i);
        if (!hdr || hdr->space_id != CLAP_CORE_EVENT_SPACE_ID)
            continue;
        if (hdr->type == CLAP_EVENT_PARAM_VALUE)
            m4a_params_process_event(data, (const clap_event_param_value_t*)hdr);
    }
}

bool m4a_params_state_save(M4APluginData* data, const clap_ostream_t* stream)
{
    /* Persist the host-owned program mirror explicitly. Driver state is
     * rebuilt on activation and reset. */
    for (int i = 0; i < M4A_PLUGIN_TRACK_COUNT; ++i)
    {
        uint8_t program = atomic_load(&data->programParams[i]);
        if (stream->write(stream, &program, 1) != 1)
            return false;
    }

    return true;
}

void m4a_params_state_load(M4APluginData* data, const clap_istream_t* stream)
{
    /* Program bytes are optional so old states still load cleanly; missing
     * entries naturally fall back to program 0. */
    for (int i = 0; i < M4A_PLUGIN_TRACK_COUNT; ++i)
    {
        uint8_t program = 0;
        stream->read(stream, &program, 1);
        m4a_params_set_program(data, i, program);
    }
}

static uint32_t params_count(const clap_plugin_t* plugin)
{
    (void)plugin;
    return M4A_PARAM_COUNT;
}

static bool params_get_info(const clap_plugin_t* plugin, uint32_t param_index, clap_param_info_t* info)
{
    (void)plugin;
    if (!info || param_index >= M4A_PARAM_COUNT)
        return false;

    memset(info, 0, sizeof(*info));
    info->id = param_index < M4A_PLUGIN_TRACK_COUNT ? M4A_PARAM_PROGRAM_BASE + param_index : M4A_PARAM_PCM_MIXER;
    info->cookie = NULL;
    if (info->id == M4A_PARAM_PCM_MIXER)
    {
        snprintf(info->name, sizeof(info->name), "PCM Mixer");
        snprintf(info->module, sizeof(info->module), "Global");
        info->min_value = M4A_PCM_MIXER_PARAM_MIN;
        info->max_value = M4A_PCM_MIXER_PARAM_MAX;
        info->default_value = M4A_PCM_MIXER_IPATIX;
        info->flags = CLAP_PARAM_IS_STEPPED | CLAP_PARAM_IS_ENUM;
        return true;
    }

    const unsigned trackIndex = (unsigned)(info->id - M4A_PARAM_PROGRAM_BASE);
    snprintf(info->name, sizeof(info->name), "Chn %u", trackIndex + 1);
    snprintf(info->module, sizeof(info->module), "Programs");
    info->min_value = 0.0;
    info->max_value = 127.0;
    info->default_value = (double)trackIndex;
    info->flags = CLAP_PARAM_IS_AUTOMATABLE | CLAP_PARAM_IS_STEPPED;
    return true;
}

static bool params_get_value(const clap_plugin_t* plugin, clap_id param_id, double* value)
{
    M4APluginData* data = (M4APluginData*)plugin->plugin_data;
    if (!value)
        return false;

    int trackIndex;
    if (is_program_param(param_id, &trackIndex))
    {
        *value = atomic_load(&data->programParams[trackIndex]);
        return true;
    }
    if (param_id == M4A_PARAM_PCM_MIXER)
    {
        M4APcmMixerMode mode;
        if (!m4a_params_get_pcm_mixer(data, &mode))
            return false;
        *value = (double)mode;
        return true;
    }
    return false;
}

static bool
params_value_to_text(const clap_plugin_t* plugin, clap_id param_id, double value, char* display, uint32_t size)
{
    (void)plugin;
    if (!display || size == 0)
        return false;

    if (param_id == M4A_PARAM_PCM_MIXER)
    {
        M4APcmMixerMode mode;
        if (!pcm_mixer_mode_from_value(value, &mode))
            return false;
        snprintf(display, size, "%s", m4a_pcm_mixer_name(mode));
        return true;
    }

    if (!is_program_param(param_id, NULL))
        return false;
    snprintf(display, size, "%u", clamp_u8_param(value, 0, 127));
    return true;
}

static bool params_text_to_value(const clap_plugin_t* plugin, clap_id param_id, const char* display, double* value)
{
    (void)plugin;
    if (!display || !value)
        return false;

    if (param_id == M4A_PARAM_PCM_MIXER)
    {
        M4APcmMixerMode mode;
        if (!m4a_pcm_mixer_parse(display, &mode))
            return false;
        *value = (double)mode;
        return true;
    }

    if (!is_program_param(param_id, NULL))
        return false;
    char* end = NULL;
    double parsed = strtod(display, &end);
    if (end == display)
        return false;
    *value = clamp_u8_param(parsed, 0, 127);
    return true;
}

static const clap_plugin_params_t s_params = {
    .count = params_count,
    .get_info = params_get_info,
    .get_value = params_get_value,
    .value_to_text = params_value_to_text,
    .text_to_value = params_text_to_value,
    .flush = params_flush,
};

const clap_plugin_params_t* m4a_params_extension(void)
{
    return &s_params;
}
