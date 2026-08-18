#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <clap/clap.h>

#include "m4a_plugin.h"
#include "m4a_params.h"
#include "test_assert.h"

enum
{
    STATE_FIXTURE_CAPACITY = 1024,
    STATE_MAX_READ_REQUEST = 512,
};
static void seed_plugin_state(M4APluginData* data);

typedef struct
{
    const uint8_t* bytes;
    size_t size;
    size_t offset;
    size_t requestedBytes;
    size_t largestRequest;
    size_t maxTransfer;
} FixtureReader;

typedef struct
{
    uint8_t bytes[STATE_FIXTURE_CAPACITY];
    size_t size;
    size_t requestedBytes;
    size_t largestRequest;
    size_t maxTransfer;
} FixtureWriter;

static int64_t CLAP_ABI fixture_write(const clap_ostream_t* stream, const void* buffer, uint64_t size)
{
    FixtureWriter* writer = (FixtureWriter*)stream->ctx;
    size_t request = (size_t)size;
    if (writer->maxTransfer != 0 && request > writer->maxTransfer)
        request = writer->maxTransfer;
    if (request > writer->largestRequest)
        writer->largestRequest = request;
    if (request <= SIZE_MAX - writer->requestedBytes)
        writer->requestedBytes += request;
    else
        writer->requestedBytes = SIZE_MAX;
    if (request > sizeof(writer->bytes) - writer->size)
        return -1;
    memcpy(writer->bytes + writer->size, buffer, request);
    writer->size += request;
    return (int64_t)request;
}
typedef struct
{
    const clap_event_header_t* event;
} ParamInput;

static uint32_t CLAP_ABI param_input_size(const clap_input_events_t* events)
{
    return ((const ParamInput*)events->ctx)->event ? 1u : 0u;
}

static const clap_event_header_t* CLAP_ABI param_input_get(const clap_input_events_t* events, uint32_t index)
{
    const ParamInput* input = (const ParamInput*)events->ctx;
    return index == 0 ? input->event : NULL;
}

typedef struct
{
    uint16_t types[4];
    double values[4];
    size_t count;
    M4APluginData* consumeMixerOnValue;
    bool mixerConsumed;
} ParamOutput;

static bool CLAP_ABI param_output_push(const clap_output_events_t* events, const clap_event_header_t* header)
{
    ParamOutput* output = (ParamOutput*)events->ctx;
    if (output->count >= 4)
        return false;
    output->types[output->count] = header->type;
    if (header->type == CLAP_EVENT_PARAM_VALUE)
    {
        output->values[output->count] = ((const clap_event_param_value_t*)header)->value;
        if (output->consumeMixerOnValue && !output->mixerConsumed)
            output->mixerConsumed = m4a_params_consume_pcm_mixer(output->consumeMixerOnValue);
    }
    output->count++;
    return true;
}

typedef struct
{
    uint8_t bytes[sizeof(M4APluginData)];
} PluginSnapshot;

static int64_t CLAP_ABI fixture_read(const clap_istream_t* stream, void* buffer, uint64_t size)
{
    FixtureReader* reader = (FixtureReader*)stream->ctx;
    size_t request = (size_t)size;
    if (reader->maxTransfer != 0 && request > reader->maxTransfer)
        request = reader->maxTransfer;
    if (request > reader->largestRequest)
        reader->largestRequest = request;
    if (request <= SIZE_MAX - reader->requestedBytes)
        reader->requestedBytes += request;
    else
        reader->requestedBytes = SIZE_MAX;

    size_t available = reader->size - reader->offset;
    if (request > available)
        request = available;
    if (request > 0)
    {
        memcpy(buffer, reader->bytes + reader->offset, request);
        reader->offset += request;
    }
    return (int64_t)request;
}

static bool read_fixture(const char* fixtureName, uint8_t bytes[STATE_FIXTURE_CAPACITY], size_t* size)
{
    static const char* directories[] = {
        "test/fixtures/state_compat",
        "../test/fixtures/state_compat",
        "../../packages/poryaaaa/test/fixtures/state_compat",
    };
    FILE* file = NULL;
    char path[512];
    for (size_t i = 0; i < sizeof(directories) / sizeof(directories[0]); ++i)
    {
        snprintf(path, sizeof(path), "%s/%s", directories[i], fixtureName);
        file = fopen(path, "rb");
        if (file)
            break;
    }
    if (!file)
        return false;

    size_t count = fread(bytes, 1, STATE_FIXTURE_CAPACITY, file);
    int trailing = fgetc(file);
    bool readError = ferror(file) != 0;
    fclose(file);
    if (trailing != EOF || readError)
        return false;
    *size = count;
    return true;
}

static void snapshot_plugin(const M4APluginData* data, PluginSnapshot* snapshot)
{
    memcpy(snapshot->bytes, data, sizeof(snapshot->bytes));
}

static bool plugin_matches_snapshot(const M4APluginData* data, const PluginSnapshot* snapshot)
{
    return memcmp(snapshot->bytes, data, sizeof(snapshot->bytes)) == 0;
}
static void assert_mixer_mode(const M4APluginData* data, M4APcmMixerMode expected, const char* message)
{
    M4APcmMixerMode actual;
    ASSERT(m4a_params_get_pcm_mixer(data, &actual) && actual == expected, message);
}

static void assert_bounded_reader(const FixtureReader* reader, const char* fixtureName)
{
    char message[160];
    snprintf(message, sizeof(message), "%s keeps every state read bounded", fixtureName);
    ASSERT(reader->largestRequest <= STATE_MAX_READ_REQUEST, message);
    snprintf(message, sizeof(message), "%s does not advance beyond its bounded fixture", fixtureName);
    ASSERT(reader->offset <= reader->size, message);
    snprintf(message, sizeof(message), "%s performs a bounded number of byte reads", fixtureName);
    ASSERT(reader->requestedBytes <= reader->size + STATE_MAX_READ_REQUEST, message);
}

static bool load_fixture(const clap_plugin_t* plugin,
                         const clap_plugin_state_t* state,
                         const char* fixtureName,
                         FixtureReader* reader)
{
    reader->bytes = NULL;
    reader->size = 0;
    reader->offset = 0;
    reader->requestedBytes = 0;
    reader->largestRequest = 0;

    uint8_t bytes[STATE_FIXTURE_CAPACITY];
    size_t size = 0;
    char message[160];
    snprintf(message, sizeof(message), "%s fixture is available", fixtureName);
    ASSERT(read_fixture(fixtureName, bytes, &size), message);
    if (size == 0)
        return false;

    reader->bytes = bytes;
    reader->size = size;
    reader->offset = 0;
    reader->requestedBytes = 0;
    reader->largestRequest = 0;

    clap_istream_t stream = {
        .ctx = reader,
        .read = fixture_read,
    };
    return state->load(plugin, &stream);
}

static void assert_v3_save_bytes(const clap_plugin_t* plugin, const clap_plugin_state_t* state)
{
    static const uint8_t expected[] = {
        'P',  'O',  'R', 'Y', 'M', '4', 'A', 0, 3, 0, 0, 0, 36, 0, 0,  0,  1,  0,  0,  0,  'r', 1, 0, 0, 0, 'n',
        0x4d, 0x17, 1,   0,   0,   1,   2,   3, 4, 5, 6, 7, 8,  9, 10, 11, 12, 13, 14, 15, 1,   1, 0, 0, 0, 'p',
    };
    FixtureWriter writer = {.maxTransfer = 3};
    clap_ostream_t stream = {
        .ctx = &writer,
        .write = fixture_write,
    };
    bool saved = state->save(plugin, &stream);
    ASSERT(saved, "state_save emits the v3 transaction successfully");
    ASSERT_EQ(writer.size, sizeof(expected), "state_save emits the exact v3 transaction length");
    ASSERT(writer.size == sizeof(expected) && memcmp(writer.bytes, expected, sizeof(expected)) == 0,
           "state_save emits byte-exact v3 transaction bytes");
    ASSERT(writer.largestRequest <= STATE_MAX_READ_REQUEST, "state_save writes bounded chunks");
}

static bool load_buffer(const clap_plugin_t* plugin,
                        const clap_plugin_state_t* state,
                        const uint8_t* bytes,
                        size_t size,
                        FixtureReader* reader)
{
    *reader = (FixtureReader){
        .bytes = bytes,
        .size = size,
        .maxTransfer = 3,
    };
    clap_istream_t stream = {
        .ctx = reader,
        .read = fixture_read,
    };
    return state->load(plugin, &stream);
}

static void assert_v3_state(const clap_plugin_t* plugin, const clap_plugin_state_t* state, M4APluginData* data)
{
    FixtureWriter writer = {0};
    clap_ostream_t ostream = {
        .ctx = &writer,
        .write = fixture_write,
    };
    ASSERT(state->save(plugin, &ostream), "v3 transaction can be saved for restore");
    seed_plugin_state(data);
    FixtureReader reader;
    ASSERT(load_buffer(plugin, state, writer.bytes, writer.size, &reader), "v3 transaction restores");
    assert_mixer_mode(data, M4A_PCM_MIXER_SAPPY, "v3 transaction restores mixer mode");
    ASSERT(strcmp(data->projectRoot, "r") == 0, "v3 transaction restores project root");
    ASSERT(strcmp(data->voicegroupName, "n") == 0, "v3 transaction restores voicegroup");
    ASSERT_EQ(data->volume, 0x4d, "v3 transaction restores volume");
    ASSERT_EQ(data->reverbAmount, 0x17, "v3 transaction restores reverb");
    ASSERT(atomic_load(&data->recorderArmed), "v3 transaction restores recorder armed state");
    ASSERT(strcmp(data->recorderPath, "p") == 0, "v3 transaction restores recorder path");
}
static void assert_v3_rejected(const clap_plugin_t* plugin,
                               const clap_plugin_state_t* state,
                               M4APluginData* data,
                               const char* name,
                               const uint8_t* bytes,
                               size_t size)
{
    PluginSnapshot before;
    snapshot_plugin(data, &before);
    FixtureReader reader;
    bool loaded = load_buffer(plugin, state, bytes, size, &reader);
    char message[160];
    snprintf(message, sizeof(message), "%s v3 transaction is rejected", name);
    ASSERT(!loaded, message);
    snprintf(message, sizeof(message), "%s rejection leaves plugin state unchanged", name);
    ASSERT(plugin_matches_snapshot(data, &before), message);
    assert_bounded_reader(&reader, name);
}
static clap_event_param_value_t mixer_param_event(double value)
{
    clap_event_param_value_t event;
    memset(&event, 0, sizeof(event));
    event.header.size = sizeof(event);
    event.header.space_id = CLAP_CORE_EVENT_SPACE_ID;
    event.header.type = CLAP_EVENT_PARAM_VALUE;
    event.param_id = M4A_PARAM_PCM_MIXER;
    event.value = value;
    return event;
}

static void flush_mixer_event(const clap_plugin_t* plugin,
                              const clap_plugin_params_t* params,
                              const clap_event_param_value_t* event,
                              ParamOutput* outputState)
{
    ParamInput inputState = {.event = event ? &event->header : NULL};
    clap_input_events_t input = {
        .ctx = &inputState,
        .size = param_input_size,
        .get = param_input_get,
    };
    clap_output_events_t output = {
        .ctx = outputState,
        .try_push = param_output_push,
    };
    params->flush(plugin, &input, &output);
}

static void assert_mixer_parameter(const clap_plugin_t* plugin, M4APluginData* data)
{
    const clap_plugin_params_t* params = (const clap_plugin_params_t*)plugin->get_extension(plugin, CLAP_EXT_PARAMS);
    ASSERT(params != NULL, "C frontend exposes parameter extension");
    if (!params)
        return;
    ASSERT_EQ(params->count(plugin), M4A_PARAM_COUNT, "mixer parameter is added globally");

    clap_param_info_t info;
    for (uint32_t index = 0; index < M4A_PARAM_COUNT; ++index)
    {
        ASSERT(params->get_info(plugin, index, &info), "stable parameter index is available");
        const clap_id expectedId =
            index < M4A_PLUGIN_TRACK_COUNT ? M4A_PARAM_PROGRAM_BASE + index : M4A_PARAM_PCM_MIXER;
        ASSERT_EQ(info.id, expectedId, "parameter index maps to its explicit stable ID");
    }
    ASSERT(params->get_info(plugin, M4A_PLUGIN_TRACK_COUNT, &info), "mixer parameter info is available");
    ASSERT_EQ(info.id, M4A_PARAM_PCM_MIXER, "mixer index maps to its derived stable ID");
    ASSERT((info.flags & CLAP_PARAM_IS_STEPPED) != 0, "mixer parameter is stepped");
    ASSERT((info.flags & CLAP_PARAM_IS_ENUM) != 0, "mixer parameter is enumerated");
    ASSERT((info.flags & CLAP_PARAM_IS_AUTOMATABLE) == 0, "mixer parameter is not automatable");

    char text[16];
    ASSERT(params->value_to_text(plugin, M4A_PARAM_PCM_MIXER, 0.0, text, sizeof(text)), "ipatix formats by name");
    ASSERT(strcmp(text, "ipatix") == 0, "ipatix parameter text is stable");
    ASSERT(params->value_to_text(plugin, M4A_PARAM_PCM_MIXER, 1.0, text, sizeof(text)), "sappy formats by name");
    ASSERT(strcmp(text, "sappy") == 0, "sappy parameter text is stable");

    m4a_params_set_pcm_mixer(data, M4A_PCM_MIXER_IPATIX);
    ASSERT(m4a_params_request_gui_pcm_mixer(data, M4A_PCM_MIXER_SAPPY), "editor accepts a changed mixer value");
    double stableValue = -1.0;
    ASSERT(params->get_value(plugin, M4A_PARAM_PCM_MIXER, &stableValue),
           "parameter get observes the shared stable mirror");
    ASSERT_EQ(stableValue, M4A_PCM_MIXER_SAPPY, "editor change updates the stable mirror before flush");

    clap_event_param_value_t hostValue = mixer_param_event(M4A_PCM_MIXER_IPATIX);
    ParamOutput outputState = {0};
    flush_mixer_event(plugin, params, &hostValue, &outputState);
    ASSERT_EQ(outputState.count, 3, "editor change emits one gesture transaction");
    ASSERT_EQ(outputState.types[0], CLAP_EVENT_PARAM_GESTURE_BEGIN, "gesture begins before mixer value");
    ASSERT_EQ(outputState.types[1], CLAP_EVENT_PARAM_VALUE, "mixer value is emitted between gestures");
    ASSERT_EQ(outputState.types[2], CLAP_EVENT_PARAM_GESTURE_END, "gesture ends after mixer value");
    ASSERT_EQ(outputState.values[1], M4A_PCM_MIXER_SAPPY, "editor transaction carries selected mode");
    assert_mixer_mode(data, M4A_PCM_MIXER_IPATIX, "later host event wins over editor change");

    memset(&outputState, 0, sizeof(outputState));
    flush_mixer_event(plugin, params, NULL, &outputState);
    ASSERT_EQ(outputState.count, 0, "completed editor transaction is not emitted twice");
    ASSERT(!m4a_params_request_gui_pcm_mixer(data, M4A_PCM_MIXER_IPATIX), "duplicate editor value is a no-op");
    ASSERT(!m4a_params_request_gui_pcm_mixer(data, (M4APcmMixerMode)2), "invalid editor value is rejected");

    hostValue = mixer_param_event(M4A_PCM_MIXER_SAPPY);
    flush_mixer_event(plugin, params, &hostValue, &outputState);
    assert_mixer_mode(data, M4A_PCM_MIXER_SAPPY, "earlier host event updates the stable mirror");
    ASSERT(m4a_params_request_gui_pcm_mixer(data, M4A_PCM_MIXER_IPATIX), "later editor change is accepted");
    memset(&outputState, 0, sizeof(outputState));
    flush_mixer_event(plugin, params, NULL, &outputState);
    ASSERT_EQ(outputState.count, 3, "later editor change emits one transaction");
    ASSERT_EQ(outputState.values[1], M4A_PCM_MIXER_IPATIX, "later editor transaction carries the winning mode");
    assert_mixer_mode(data, M4A_PCM_MIXER_IPATIX, "later editor change wins over host event");

    hostValue = mixer_param_event(2.0);
    flush_mixer_event(plugin, params, &hostValue, &outputState);
    assert_mixer_mode(data, M4A_PCM_MIXER_IPATIX, "invalid host value leaves the stable mirror unchanged");
}

static void save_mixer_state(const clap_plugin_t* plugin,
                             const clap_plugin_state_t* state,
                             FixtureWriter* writer,
                             M4APcmMixerMode mode)
{
    m4a_params_set_pcm_mixer((M4APluginData*)plugin->plugin_data, mode);
    clap_ostream_t stream = {
        .ctx = writer,
        .write = fixture_write,
    };
    ASSERT(state->save(plugin, &stream), "ordered mixer test state saves");
}

static void assert_mixer_state_order(const clap_plugin_t* plugin, const clap_plugin_state_t* state, M4APluginData* data)
{
    const clap_plugin_params_t* params = (const clap_plugin_params_t*)plugin->get_extension(plugin, CLAP_EXT_PARAMS);
    FixtureWriter ipatixState = {0};
    FixtureWriter sappyState = {0};
    save_mixer_state(plugin, state, &ipatixState, M4A_PCM_MIXER_IPATIX);
    save_mixer_state(plugin, state, &sappyState, M4A_PCM_MIXER_SAPPY);

    m4a_params_set_pcm_mixer(data, M4A_PCM_MIXER_SAPPY);
    assert_mixer_mode(data, M4A_PCM_MIXER_SAPPY, "fresh configuration seeds the mixer parameter");
    FixtureReader reader;
    ASSERT(load_buffer(plugin, state, ipatixState.bytes, ipatixState.size, &reader),
           "restored state loads over fresh configuration");
    assert_mixer_mode(data, M4A_PCM_MIXER_IPATIX, "restored state takes precedence over fresh configuration");

    ASSERT(m4a_params_request_gui_pcm_mixer(data, M4A_PCM_MIXER_SAPPY), "editor queues a value before state restore");
    ASSERT(load_buffer(plugin, state, ipatixState.bytes, ipatixState.size, &reader), "later state restore succeeds");
    ParamOutput outputState = {0};
    flush_mixer_event(plugin, params, NULL, &outputState);
    ASSERT_EQ(outputState.count, 0, "later state restore cancels the earlier editor transaction");
    assert_mixer_mode(data, M4A_PCM_MIXER_IPATIX, "later state restore wins over editor change");

    ASSERT(load_buffer(plugin, state, sappyState.bytes, sappyState.size, &reader), "earlier state restore succeeds");
    ASSERT(m4a_params_request_gui_pcm_mixer(data, M4A_PCM_MIXER_IPATIX), "editor queues a value after state restore");
    flush_mixer_event(plugin, params, NULL, &outputState);
    assert_mixer_mode(data, M4A_PCM_MIXER_IPATIX, "later editor change wins over state restore");

    clap_event_param_value_t hostValue = mixer_param_event(M4A_PCM_MIXER_SAPPY);
    flush_mixer_event(plugin, params, &hostValue, &outputState);
    ASSERT(load_buffer(plugin, state, ipatixState.bytes, ipatixState.size, &reader),
           "state restore follows host event");
    assert_mixer_mode(data, M4A_PCM_MIXER_IPATIX, "later state restore wins over host event");

    ASSERT(load_buffer(plugin, state, sappyState.bytes, sappyState.size, &reader), "state restore precedes host event");
    hostValue = mixer_param_event(M4A_PCM_MIXER_IPATIX);
    flush_mixer_event(plugin, params, &hostValue, &outputState);
    assert_mixer_mode(data, M4A_PCM_MIXER_IPATIX, "later host event wins over state restore");
}

static void assert_audio_thread_mixer_handoff(const clap_plugin_t* plugin, M4APluginData* data)
{
    const clap_plugin_params_t* params = (const clap_plugin_params_t*)plugin->get_extension(plugin, CLAP_EXT_PARAMS);
    data->driver = m4a_driver_create(44100.0f);
    ASSERT(data->driver != NULL, "audio handoff test driver allocates");
    if (!data->driver)
        return;

    m4a_params_set_pcm_mixer(data, M4A_PCM_MIXER_IPATIX);
    m4a_params_mark_pcm_mixer_pending(data);
    ASSERT(m4a_params_consume_pcm_mixer(data), "audio thread consumes the initial mixer request");
    m4a_advance(data->driver, 1024);
    ASSERT_EQ(m4a_driver_get_pcm_mixer_mode(data->driver), M4A_PCM_MIXER_IPATIX, "audio handoff test starts in iPatix");

    ASSERT(m4a_params_request_gui_pcm_mixer(data, M4A_PCM_MIXER_SAPPY), "GUI queues Sappy for the audio thread");
    ParamOutput outputState = {0};
    flush_mixer_event(plugin, params, NULL, &outputState);
    m4a_advance(data->driver, 1024);
    ASSERT_EQ(m4a_driver_get_pcm_mixer_mode(data->driver),
              M4A_PCM_MIXER_IPATIX,
              "GUI and flush never call the driver setter");

    ASSERT(m4a_params_consume_pcm_mixer(data), "audio thread consumes the queued Sappy mode");
    ASSERT_EQ(m4a_driver_get_pcm_mixer_mode(data->driver),
              M4A_PCM_MIXER_IPATIX,
              "audio-thread setter still waits for the SoundMain boundary");
    m4a_advance(data->driver, 1024);
    ASSERT_EQ(m4a_driver_get_pcm_mixer_mode(data->driver),
              M4A_PCM_MIXER_SAPPY,
              "audio-thread request commits at the SoundMain boundary");

    m4a_params_set_pcm_mixer(data, M4A_PCM_MIXER_IPATIX);
    m4a_advance(data->driver, 1024);
    ASSERT_EQ(m4a_driver_get_pcm_mixer_mode(data->driver),
              M4A_PCM_MIXER_SAPPY,
              "host/state mirror update never calls the driver setter");
    ASSERT(m4a_params_consume_pcm_mixer(data), "audio thread consumes the host/state request");
    m4a_advance(data->driver, 1024);
    ASSERT_EQ(m4a_driver_get_pcm_mixer_mode(data->driver),
              M4A_PCM_MIXER_IPATIX,
              "audio thread alone commits the host/state request");

    ASSERT(m4a_params_request_gui_pcm_mixer(data, M4A_PCM_MIXER_SAPPY),
           "GUI queues a transaction for the interleaved handoff");
    ParamOutput interleavedOutput = {
        .consumeMixerOnValue = data,
    };
    flush_mixer_event(plugin, params, NULL, &interleavedOutput);
    ASSERT(interleavedOutput.mixerConsumed, "audio consume can interleave with GUI transaction emission");
    ParamOutput retryOutput = {0};
    flush_mixer_event(plugin, params, NULL, &retryOutput);
    ASSERT_EQ(retryOutput.count, 0, "interleaved audio consume does not duplicate the GUI transaction");
    m4a_advance(data->driver, 1024);
    ASSERT_EQ(m4a_driver_get_pcm_mixer_mode(data->driver),
              M4A_PCM_MIXER_SAPPY,
              "interleaved audio consume commits the emitted stable mode");

    m4a_driver_destroy(data->driver);
    data->driver = NULL;
}

static void assert_failed_without_mutation(const clap_plugin_t* plugin,
                                           const clap_plugin_state_t* state,
                                           M4APluginData* data,
                                           const char* fixtureName)
{
    PluginSnapshot before;
    snapshot_plugin(data, &before);

    FixtureReader reader;
    bool loaded = load_fixture(plugin, state, fixtureName, &reader);
    char message[160];
    snprintf(message, sizeof(message), "%s state load rejects the truncated or malformed transaction", fixtureName);
    ASSERT(!loaded, message);
    snprintf(message, sizeof(message), "%s rejection leaves plugin state unchanged", fixtureName);
    ASSERT(plugin_matches_snapshot(data, &before), message);
    assert_bounded_reader(&reader, fixtureName);
}

static void assert_v2_state(const clap_plugin_t* plugin,
                            const clap_plugin_state_t* state,
                            M4APluginData* data,
                            const char* fixtureName,
                            bool includesRecorder)
{
    FixtureReader reader;
    bool loaded = load_fixture(plugin, state, fixtureName, &reader);
    char message[160];
    snprintf(message, sizeof(message), "%s state load succeeds", fixtureName);
    ASSERT(loaded, message);
    assert_bounded_reader(&reader, fixtureName);
    if (!loaded)
        return;

    ASSERT(strcmp(data->projectRoot, "r") == 0, "v2 fixture restores project root");
    ASSERT(strcmp(data->voicegroupName, "n") == 0, "v2 fixture restores voicegroup name");
    ASSERT_EQ(data->volume, 0x4d, "v2 fixture restores volume");
    ASSERT_EQ(data->reverbAmount, 0x17, "v2 fixture restores reverb");
    assert_mixer_mode(data, M4A_PCM_MIXER_IPATIX, "v2 fixture forces iPatix mixer mode");
    for (int i = 0; i < M4A_PLUGIN_TRACK_COUNT; ++i)
        ASSERT_EQ(atomic_load(&data->programParams[i]), i, "v2 fixture restores every program");

    if (includesRecorder)
    {
        ASSERT(atomic_load(&data->recorderArmed), "v2 fixture restores recorder armed state");
        ASSERT(strcmp(data->recorderPath, "p") == 0, "v2 fixture restores recorder path");
    }
}

static void assert_unversioned_state(const clap_plugin_t* plugin,
                                     const clap_plugin_state_t* state,
                                     M4APluginData* data,
                                     const char* fixtureName)
{
    FixtureReader reader;
    bool loaded = load_fixture(plugin, state, fixtureName, &reader);
    char message[160];
    snprintf(message, sizeof(message), "%s state load succeeds", fixtureName);
    ASSERT(loaded, message);
    assert_bounded_reader(&reader, fixtureName);
    if (!loaded)
        return;

    ASSERT(strcmp(data->projectRoot, "r") == 0, "unversioned fixture restores project root");
    ASSERT(strcmp(data->voicegroupName, "n") == 0, "unversioned fixture restores voicegroup name");
    ASSERT_EQ(data->volume, 0x4d, "unversioned fixture restores song volume");
    ASSERT_EQ(data->reverbAmount, 0x17, "unversioned fixture restores reverb");
    assert_mixer_mode(data, M4A_PCM_MIXER_IPATIX, "unversioned fixture forces iPatix mixer mode");
    for (int i = 0; i < M4A_PLUGIN_TRACK_COUNT; ++i)
        ASSERT_EQ(atomic_load(&data->programParams[i]), i, "unversioned fixture restores every program");
}

static void seed_plugin_state(M4APluginData* data)
{
    snprintf(data->projectRoot, sizeof(data->projectRoot), "%s", "seed-root");
    snprintf(data->voicegroupName, sizeof(data->voicegroupName), "%s", "seed-name");
    data->volume = 11;
    data->reverbAmount = 22;
    for (int i = 0; i < M4A_PLUGIN_TRACK_COUNT; ++i)
        atomic_store(&data->programParams[i], (uint8_t)(0x70 + i));
    atomic_store(&data->recorderArmed, true);
    snprintf(data->recorderPath, sizeof(data->recorderPath), "%s", "seed-recorder");
}

static void set_v2_fixture_state(M4APluginData* data)
{
    snprintf(data->projectRoot, sizeof(data->projectRoot), "%s", "r");
    snprintf(data->voicegroupName, sizeof(data->voicegroupName), "%s", "n");
    data->volume = 0x4d;
    data->reverbAmount = 0x17;
    for (int i = 0; i < M4A_PLUGIN_TRACK_COUNT; ++i)
        atomic_store(&data->programParams[i], (uint8_t)i);
    atomic_store(&data->recorderArmed, true);
    snprintf(data->recorderPath, sizeof(data->recorderPath), "%s", "p");
}

/*
 * The fixture stream deliberately returns short reads at EOF.  This suite is
 * called with an initialized, inactive C plugin so state_load can be exercised
 * through the public CLAP state extension without loading project assets.
 *
 * The root-length-2 fixture is a valid pre-v2 save.  Its first u32 is also the
 * v2 version marker, so the current discriminator must choose v2 and reject
 * the remaining legacy bytes rather than guessing a second interpretation.
 */
void test_m4a_state_compat_run_all(const clap_plugin_t* plugin)
{
    printf("Testing C plugin state compatibility fixtures...\n");
    ASSERT(plugin != NULL, "state compatibility receives a plugin");
    if (!plugin || !plugin->plugin_data)
        return;

    const clap_plugin_state_t* state = (const clap_plugin_state_t*)plugin->get_extension(plugin, CLAP_EXT_STATE);
    ASSERT(state != NULL && state->load != NULL && state->save != NULL, "plugin exposes CLAP state load and save");
    if (!state || !state->load || !state->save)
        return;

    M4APluginData* data = (M4APluginData*)plugin->plugin_data;
    ASSERT(!data->activated, "state compatibility fixture plugin is inactive");
    ASSERT(data->loadedVg == NULL, "state compatibility fixture has no loaded voicegroup");
    assert_mixer_mode(data, M4A_PCM_MIXER_SAPPY, "fresh plugin configuration seeds the stable mixer mirror");
    assert_mixer_parameter(plugin, data);
    assert_mixer_state_order(plugin, state, data);
    assert_audio_thread_mixer_handoff(plugin, data);

    set_v2_fixture_state(data);
    m4a_params_set_pcm_mixer(data, M4A_PCM_MIXER_SAPPY);
    assert_v3_save_bytes(plugin, state);
    assert_v3_state(plugin, state, data);
    set_v2_fixture_state(data);
    m4a_params_set_pcm_mixer(data, M4A_PCM_MIXER_SAPPY);
    FixtureWriter validV3 = {0};
    clap_ostream_t validStream = {
        .ctx = &validV3,
        .write = fixture_write,
    };
    ASSERT(state->save(plugin, &validStream), "v3 rejection cases have a valid source transaction");
    uint8_t malformedV3[STATE_FIXTURE_CAPACITY];
    memcpy(malformedV3, validV3.bytes, validV3.size);
    assert_v3_rejected(plugin, state, data, "v3-truncated-header", malformedV3, 7);
    assert_v3_rejected(plugin, state, data, "v3-truncated-payload", malformedV3, validV3.size - 1);

    memcpy(malformedV3, validV3.bytes, validV3.size);
    malformedV3[8] = 4;
    assert_v3_rejected(plugin, state, data, "v3-invalid-version", malformedV3, validV3.size);
    memcpy(malformedV3, validV3.bytes, validV3.size);
    malformedV3[12] = 37;
    assert_v3_rejected(plugin, state, data, "v3-payload-size-mismatch", malformedV3, validV3.size);
    memcpy(malformedV3, validV3.bytes, validV3.size);
    malformedV3[29] = 1;
    assert_v3_rejected(plugin, state, data, "v3-nonzero-reserved", malformedV3, validV3.size);
    memcpy(malformedV3, validV3.bytes, validV3.size);
    malformedV3[28] = 2;
    assert_v3_rejected(plugin, state, data, "v3-invalid-mixer-mode", malformedV3, validV3.size);
    memcpy(malformedV3, validV3.bytes, validV3.size);
    malformedV3[46] = 2;
    assert_v3_rejected(plugin, state, data, "v3-invalid-recorder-boolean", malformedV3, validV3.size);
    memcpy(malformedV3, validV3.bytes, validV3.size);
    malformedV3[20] = 0xc0;
    assert_v3_rejected(plugin, state, data, "v3-invalid-utf8", malformedV3, validV3.size);
    memcpy(malformedV3, validV3.bytes, validV3.size);
    malformedV3[16] = 0;
    malformedV3[17] = 2;
    assert_v3_rejected(plugin, state, data, "v3-over-capacity-root", malformedV3, validV3.size);
    memcpy(malformedV3, validV3.bytes, validV3.size + 1);
    malformedV3[validV3.size] = 0xaa;
    assert_v3_rejected(plugin, state, data, "v3-trailing-bytes", malformedV3, validV3.size + 1);

    static const char* failures[] = {
        "v2-truncated-version.bin",
        "v2-truncated-root-length.bin",
        "v2-truncated-root.bin",
        "v2-truncated-name-length.bin",
        "v2-truncated-name.bin",
        "v2-truncated-volume.bin",
        "v2-truncated-reverb.bin",
        "v2-truncated-programs.bin",
        "v2-truncated-recorder-path-length.bin",
        "v2-truncated-recorder-path.bin",
        "v2-malformed-root-length.bin",
        "v2-malformed-name-length.bin",
        "v2-malformed-recorder-path-length.bin",
        "unversioned-truncated-root-length.bin",
        "unversioned-truncated-root.bin",
        "unversioned-truncated-name-length.bin",
        "unversioned-truncated-name.bin",
        "unversioned-truncated-reverb.bin",
        "unversioned-truncated-master-volume.bin",
        "unversioned-truncated-volume.bin",
        "unversioned-truncated-analog-filter.bin",
        "unversioned-truncated-max-pcm-channels.bin",
        "unversioned-truncated-programs.bin",
        "unversioned-malformed-root-length.bin",
        "unversioned-malformed-name-length.bin",
        "v2-malformed-root-length-max.bin",
        "v2-malformed-name-length-max.bin",
        "v2-malformed-recorder-path-length-max.bin",
        "unversioned-malformed-root-length-max.bin",
        "unversioned-malformed-name-length-max.bin",
        "unversioned-root-length-2-collision.bin",
    };
    for (size_t i = 0; i < sizeof(failures) / sizeof(failures[0]); ++i)
        assert_failed_without_mutation(plugin, state, data, failures[i]);

    /* Missing recorder bytes are the one intentional valid v2 truncation. */
    assert_v2_state(plugin, state, data, "v2-truncated-recorder-armed.bin", false);
    assert_v2_state(plugin, state, data, "v2-valid-without-recorder.bin", false);
    assert_v2_state(plugin, state, data, "v2-valid.bin", true);
    assert_unversioned_state(plugin, state, data, "unversioned-valid.bin");
}
