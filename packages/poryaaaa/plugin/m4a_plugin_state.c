#include "m4a_plugin_state.h"

#include <limits.h>
#include <string.h>

#include "m4a/m4a_pcm_mixer_mode.h"

#define M4A_PLUGIN_STATE_VERSION 3
#define M4A_PLUGIN_STATE_HEADER_SIZE 16
#define M4A_PLUGIN_STATE_MAGIC "PORYM4A"
#define M4A_PLUGIN_STATE_MAX_PAYLOAD 1310
#define M4A_PLUGIN_STATE_IO_CHUNK 512

typedef struct
{
    const clap_istream_t* stream;
    uint8_t staged[8];
    size_t stagedOffset;
} StateReader;

static bool state_write_bytes(const clap_ostream_t* stream, const uint8_t* bytes, size_t size)
{
    size_t offset = 0;
    while (offset < size)
    {
        size_t chunk = size - offset;
        if (chunk > M4A_PLUGIN_STATE_IO_CHUNK)
            chunk = M4A_PLUGIN_STATE_IO_CHUNK;
        int64_t count = stream->write(stream, bytes + offset, chunk);
        if (count <= 0 || count > (int64_t)chunk)
            return false;
        offset += (size_t)count;
    }
    return true;
}

static void state_put_u32_le(uint8_t* bytes, size_t* offset, uint32_t value)
{
    bytes[(*offset)++] = (uint8_t)value;
    bytes[(*offset)++] = (uint8_t)(value >> 8);
    bytes[(*offset)++] = (uint8_t)(value >> 16);
    bytes[(*offset)++] = (uint8_t)(value >> 24);
}

static uint32_t state_get_u32_le(const uint8_t* bytes)
{
    return (uint32_t)bytes[0] | ((uint32_t)bytes[1] << 8) | ((uint32_t)bytes[2] << 16) | ((uint32_t)bytes[3] << 24);
}

static bool state_utf8_valid(const uint8_t* bytes, size_t size)
{
    size_t i = 0;
    while (i < size)
    {
        uint8_t first = bytes[i++];
        if (first == 0)
            return false;
        if (first <= 0x7f)
            continue;
        if (first >= 0xc2 && first <= 0xdf)
        {
            if (i >= size || bytes[i] < 0x80 || bytes[i] > 0xbf)
                return false;
            ++i;
            continue;
        }
        if (first >= 0xe0 && first <= 0xef)
        {
            if (i + 1 >= size || bytes[i] < 0x80 || bytes[i] > 0xbf || bytes[i + 1] < 0x80 || bytes[i + 1] > 0xbf)
                return false;
            if (first == 0xe0 && bytes[i] < 0xa0)
                return false;
            if (first == 0xed && bytes[i] > 0x9f)
                return false;
            i += 2;
            continue;
        }
        if (first >= 0xf0 && first <= 0xf4)
        {
            if (i + 2 >= size || bytes[i] < 0x80 || bytes[i] > 0xbf || bytes[i + 1] < 0x80 || bytes[i + 1] > 0xbf ||
                bytes[i + 2] < 0x80 || bytes[i + 2] > 0xbf)
                return false;
            if (first == 0xf0 && bytes[i] < 0x90)
                return false;
            if (first == 0xf4 && bytes[i] > 0x8f)
                return false;
            i += 3;
            continue;
        }
        return false;
    }
    return true;
}

static bool state_payload_put_string(uint8_t* payload, size_t* offset, const char* string, size_t capacity)
{
    size_t length = strlen(string);
    if (length >= capacity || length > UINT32_MAX || !state_utf8_valid((const uint8_t*)string, length))
        return false;
    state_put_u32_le(payload, offset, (uint32_t)length);
    if (length > 0)
    {
        memcpy(payload + *offset, string, length);
        *offset += length;
    }
    return true;
}

static int64_t state_reader_read_some(StateReader* reader, void* buffer, size_t size)
{
    if (size == 0)
        return 0;
    uint8_t* destination = (uint8_t*)buffer;
    size_t copied = 0;
    while (copied < size && reader->stagedOffset < sizeof(reader->staged))
        destination[copied++] = reader->staged[reader->stagedOffset++];
    if (copied == size)
        return (int64_t)copied;
    int64_t result = reader->stream->read(reader->stream, destination + copied, size - copied);
    if (result < 0)
        return result;
    return (int64_t)copied + result;
}

static bool state_reader_read_exact(StateReader* reader, void* buffer, size_t size)
{
    size_t offset = 0;
    while (offset < size)
    {
        size_t chunk = size - offset;
        if (chunk > M4A_PLUGIN_STATE_IO_CHUNK)
            chunk = M4A_PLUGIN_STATE_IO_CHUNK;
        int64_t count = state_reader_read_some(reader, (uint8_t*)buffer + offset, chunk);
        if (count <= 0 || count > (int64_t)chunk)
            return false;
        offset += (size_t)count;
    }
    return true;
}

static int state_reader_read_byte(StateReader* reader, uint8_t* byte)
{
    int64_t count = state_reader_read_some(reader, byte, 1);
    if (count == 1)
        return 1;
    if (count == 0)
        return 0;
    return -1;
}

static bool state_reader_read_u32(StateReader* reader, uint32_t* value)
{
    uint8_t bytes[4];
    if (!state_reader_read_exact(reader, bytes, sizeof(bytes)))
        return false;
    *value = state_get_u32_le(bytes);
    return true;
}

static bool
state_payload_get_string(const uint8_t* payload, size_t payloadSize, size_t* offset, char* destination, size_t capacity)
{
    if (*offset > payloadSize || payloadSize - *offset < 4)
        return false;
    uint32_t length = state_get_u32_le(payload + *offset);
    *offset += 4;
    if (length >= capacity || length > payloadSize - *offset)
        return false;
    if (!state_utf8_valid(payload + *offset, length))
        return false;
    if (length > 0)
        memcpy(destination, payload + *offset, length);
    destination[length] = '\0';
    *offset += length;
    return true;
}

static bool state_parse_v3_payload(const uint8_t* payload, size_t payloadSize, M4APluginStateData* state)
{
    size_t offset = 0;
    if (!state_payload_get_string(payload, payloadSize, &offset, state->projectRoot, sizeof(state->projectRoot)) ||
        !state_payload_get_string(
            payload, payloadSize, &offset, state->voicegroupName, sizeof(state->voicegroupName)) ||
        payloadSize - offset < 4 + M4A_PLUGIN_STATE_TRACK_COUNT + 1 + 4)
        return false;
    state->volume = payload[offset++];
    state->reverbAmount = payload[offset++];
    if (!m4a_pcm_mixer_from_raw(payload[offset++], &state->mixerMode) || payload[offset++] != 0)
        return false;
    memcpy(state->programs, payload + offset, M4A_PLUGIN_STATE_TRACK_COUNT);
    offset += M4A_PLUGIN_STATE_TRACK_COUNT;
    uint8_t armed = payload[offset++];
    if (armed > 1)
        return false;
    state->recorderArmed = armed != 0;
    return state_payload_get_string(payload, payloadSize, &offset, state->recorderPath, sizeof(state->recorderPath)) &&
           offset == payloadSize;
}

static bool state_parse_legacy(StateReader* reader, bool versioned, M4APluginStateData* state)
{
    uint32_t rootLength;
    uint32_t nameLength;
    if (versioned)
    {
        uint32_t version;
        if (!state_reader_read_u32(reader, &version) || version != 2 || !state_reader_read_u32(reader, &rootLength))
            return false;
    }
    else if (!state_reader_read_u32(reader, &rootLength))
    {
        return false;
    }
    if (rootLength >= sizeof(state->projectRoot) || !state_reader_read_exact(reader, state->projectRoot, rootLength))
        return false;
    state->projectRoot[rootLength] = '\0';
    if (!state_reader_read_u32(reader, &nameLength) || nameLength >= sizeof(state->voicegroupName) ||
        !state_reader_read_exact(reader, state->voicegroupName, nameLength))
        return false;
    state->voicegroupName[nameLength] = '\0';

    if (versioned)
    {
        if (!state_reader_read_exact(reader, &state->volume, 1) ||
            !state_reader_read_exact(reader, &state->reverbAmount, 1))
            return false;
    }
    else
    {
        uint8_t unusedMasterVolume;
        uint8_t unusedAnalogFilter;
        uint8_t unusedMaxPcmChannels;
        if (!state_reader_read_exact(reader, &state->reverbAmount, 1) ||
            !state_reader_read_exact(reader, &unusedMasterVolume, 1) ||
            !state_reader_read_exact(reader, &state->volume, 1) ||
            !state_reader_read_exact(reader, &unusedAnalogFilter, 1) ||
            !state_reader_read_exact(reader, &unusedMaxPcmChannels, 1))
            return false;
    }
    if (!state_reader_read_exact(reader, state->programs, M4A_PLUGIN_STATE_TRACK_COUNT))
        return false;

    state->recorderArmed = false;
    state->recorderPath[0] = '\0';
    if (versioned)
    {
        uint8_t armed;
        int present = state_reader_read_byte(reader, &armed);
        if (present < 0)
            return false;
        if (present > 0)
        {
            uint32_t pathLength;
            if (!state_reader_read_u32(reader, &pathLength) || pathLength >= sizeof(state->recorderPath) ||
                !state_reader_read_exact(reader, state->recorderPath, pathLength))
                return false;
            state->recorderPath[pathLength] = '\0';
            state->recorderArmed = armed != 0;
        }
    }
    state->mixerMode = M4A_PCM_MIXER_IPATIX;
    return true;
}

bool m4a_plugin_state_write(const clap_ostream_t* stream, const M4APluginStateData* state)
{
    if (!stream || !state || !m4a_pcm_mixer_name(state->mixerMode))
        return false;

    uint8_t payload[M4A_PLUGIN_STATE_MAX_PAYLOAD];
    uint8_t header[M4A_PLUGIN_STATE_HEADER_SIZE];
    size_t payloadOffset = 0;
    if (!state_payload_put_string(payload, &payloadOffset, state->projectRoot, sizeof(state->projectRoot)) ||
        !state_payload_put_string(payload, &payloadOffset, state->voicegroupName, sizeof(state->voicegroupName)))
        return false;
    if (payloadOffset + 4 + M4A_PLUGIN_STATE_TRACK_COUNT + 1 + 4 > sizeof(payload))
        return false;
    payload[payloadOffset++] = state->volume;
    payload[payloadOffset++] = state->reverbAmount;
    payload[payloadOffset++] = (uint8_t)state->mixerMode;
    payload[payloadOffset++] = 0;
    memcpy(payload + payloadOffset, state->programs, M4A_PLUGIN_STATE_TRACK_COUNT);
    payloadOffset += M4A_PLUGIN_STATE_TRACK_COUNT;
    payload[payloadOffset++] = state->recorderArmed ? 1 : 0;
    if (!state_payload_put_string(payload, &payloadOffset, state->recorderPath, sizeof(state->recorderPath)))
        return false;

    memcpy(header, M4A_PLUGIN_STATE_MAGIC, 7);
    header[7] = 0;
    size_t headerOffset = 8;
    state_put_u32_le(header, &headerOffset, M4A_PLUGIN_STATE_VERSION);
    state_put_u32_le(header, &headerOffset, (uint32_t)payloadOffset);
    return state_write_bytes(stream, header, sizeof(header)) && state_write_bytes(stream, payload, payloadOffset);
}

bool m4a_plugin_state_read(const clap_istream_t* stream, M4APluginStateData* state)
{
    if (!stream || !state)
        return false;

    uint8_t staged[8];
    size_t stagedSize = 0;
    while (stagedSize < sizeof(staged))
    {
        int64_t count = stream->read(stream, staged + stagedSize, sizeof(staged) - stagedSize);
        if (count <= 0)
            return false;
        stagedSize += (size_t)count;
    }

    StateReader reader = {.stream = stream, .stagedOffset = 0};
    memcpy(reader.staged, staged, sizeof(reader.staged));
    M4APluginStateData candidate;
    memset(&candidate, 0, sizeof(candidate));

    bool parsed;
    if (memcmp(staged, M4A_PLUGIN_STATE_MAGIC, 7) == 0 && staged[7] == 0)
    {
        reader.stagedOffset = sizeof(reader.staged);
        uint32_t version;
        uint32_t payloadSize;
        if (!state_reader_read_u32(&reader, &version) || !state_reader_read_u32(&reader, &payloadSize) ||
            version != M4A_PLUGIN_STATE_VERSION || payloadSize > M4A_PLUGIN_STATE_MAX_PAYLOAD)
            return false;
        uint8_t payload[M4A_PLUGIN_STATE_MAX_PAYLOAD];
        if (!state_reader_read_exact(&reader, payload, payloadSize) ||
            !state_parse_v3_payload(payload, payloadSize, &candidate))
            return false;
        uint8_t trailing;
        if (state_reader_read_byte(&reader, &trailing) != 0)
            return false;
        parsed = true;
    }
    else
    {
        parsed = state_parse_legacy(&reader, state_get_u32_le(staged) == 2, &candidate);
    }
    if (!parsed)
        return false;
    *state = candidate;
    return true;
}
