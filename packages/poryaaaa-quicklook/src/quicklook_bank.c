#include "poryaaaa_quicklook_bank.h"
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

/* Wire sizes are independent of the engine's native structure layout. */
enum
{
    HEADER_SIZE = 112,
    TONE_SIZE = 32,
    WAVE_SIZE = 24,
    GROUP_SIZE = 128 * TONE_SIZE
};
enum
{
    VOICES,
    SUBGROUPS,
    WAVES,
    PROGWAVES,
    TABLES,
    SECTION_COUNT
};
enum
{
    PTR_NULL,
    PTR_SCALAR,
    PTR_WAVE,
    PTR_PROGWAVE,
    PTR_SUBGROUP
};

_Static_assert(sizeof(ToneData) <= TONE_SIZE, "ToneData must fit its wire slot");
_Static_assert(sizeof(WaveData) <= WAVE_SIZE, "WaveData must fit its wire slot");
_Static_assert(_Alignof(ToneData) <= 8 && _Alignof(WaveData) <= 8, "Bank alignment is eight bytes");

typedef struct
{
    uint64_t offset;
    uint64_t size;
    uint32_t count;
} Section;

struct PqlBank
{
    uint8_t* buffer;
    ToneData* voices;
};

/* Decode wire integers without assuming host alignment or byte order. */
static uint64_t read_le(const uint8_t* p, unsigned bytes)
{
    uint64_t value = 0;
    for (unsigned i = 0; i < bytes; ++i)
        value |= (uint64_t)p[i] << (i * 8);
    return value;
}

/* A non-NULL reference must name a whole registered object, not its interior. */
static bool valid_slot(uint64_t offset, const Section* section, uint64_t stride)
{
    return offset >= section->offset && (offset - section->offset) % stride == 0 &&
           (offset - section->offset) / stride < section->count;
}

/* Validate all tones before any wire bytes are overwritten with native structs. */
static bool validate_tones(const uint8_t* blob, const Section* sections, unsigned sectionIndex)
{
    const Section* section = &sections[sectionIndex];
    uint64_t count = section->size / TONE_SIZE;
    for (uint64_t i = 0; i < count; ++i)
    {
        const uint8_t* tone = blob + section->offset + i * TONE_SIZE;
        uint64_t ptr0 = read_le(tone + 8, 8);
        uint64_t ptr1 = read_le(tone + 16, 8);
        switch (tone[24])
        {
        case PTR_NULL:
            if (ptr0 != 0)
                return false;
            break;
        case PTR_SCALAR:
            if (ptr0 == 0 || ptr0 >= 4096 || ptr0 > UINTPTR_MAX)
                return false;
            break;
        case PTR_WAVE:
            if (!valid_slot(ptr0, &sections[WAVES], WAVE_SIZE))
                return false;
            break;
        case PTR_PROGWAVE:
            if (!valid_slot(ptr0, &sections[PROGWAVES], 16))
                return false;
            break;
        case PTR_SUBGROUP:
            if (!valid_slot(ptr0, &sections[SUBGROUPS], GROUP_SIZE))
                return false;
            break;
        default:
            return false;
        }
        if (ptr1 && !valid_slot(ptr1, &sections[TABLES], 128))
            return false;
        for (unsigned byte = 25; byte < TONE_SIZE; ++byte)
            if (tone[byte] != 0)
                return false;
    }
    return true;
}

/* Convert a voice array in place, compacting native slots if necessary. */
static void fix_tones(uint8_t* buffer, const Section* sections, uint64_t offset)
{
    for (unsigned i = 0; i < 128; ++i)
    {
        const uint8_t* wire = buffer + offset + i * TONE_SIZE;
        uint64_t ptr0 = read_le(wire + 8, 8);
        uint64_t ptr1 = read_le(wire + 16, 8);
        ToneData tone = {0};
        tone.type = wire[0];
        tone.key = wire[1];
        tone.length = wire[2];
        tone.panSweep = wire[3];
        tone.attack = wire[4];
        tone.decay = wire[5];
        tone.sustain = wire[6];
        tone.release = wire[7];
        switch (wire[24])
        {
        case PTR_SCALAR:
            tone.subGroup = (void*)(uintptr_t)ptr0;
            break;
        case PTR_WAVE:
            tone.wav = (WaveData*)(buffer + sections[WAVES].offset +
                                   ((ptr0 - sections[WAVES].offset) / WAVE_SIZE) * sizeof(WaveData));
            break;
        case PTR_PROGWAVE:
            tone.wavePointer = (uint32_t*)(buffer + ptr0);
            break;
        case PTR_SUBGROUP:
            tone.subGroup = buffer + ptr0;
            break;
        default:
            break;
        }
        if (ptr1)
            tone.keySplitTable = buffer + ptr1;
        memcpy(buffer + offset + i * sizeof(ToneData), &tone, sizeof(tone));
    }
}

/* Reject malformed layout/references, then allocate and fix up one owned copy. */
PqlBank* pql_bank_load(const uint8_t* blob, size_t size)
{
    if (!blob || size < HEADER_SIZE || memcmp(blob, "PQLBANK1", 8) != 0 || read_le(blob + 8, 4) != 1 ||
        read_le(blob + 12, 4) != 128)
        return NULL;

    Section sections[SECTION_COUNT];
    uint64_t previousEnd = HEADER_SIZE;
    const uint64_t strides[SECTION_COUNT] = {TONE_SIZE, GROUP_SIZE, WAVE_SIZE, 16, 128};
    for (unsigned i = 0; i < SECTION_COUNT; ++i)
    {
        Section* section = &sections[i];
        section->count = (uint32_t)read_le(blob + 12 + i * 4, 4);
        section->offset = read_le(blob + 32 + i * 16, 8);
        section->size = read_le(blob + 40 + i * 16, 8);
        if (section->offset % 8 != 0 || section->offset < previousEnd || section->offset > size ||
            section->size > size - section->offset)
            return NULL;
        uint64_t objectBytes = (uint64_t)section->count * strides[i];
        if ((i == WAVES && section->size < objectBytes) || (i != WAVES && section->size != objectBytes))
            return NULL;
        previousEnd = section->offset + section->size;
    }
    if (!validate_tones(blob, sections, VOICES) || !validate_tones(blob, sections, SUBGROUPS))
        return NULL;

    const Section* waves = &sections[WAVES];
    uint64_t payloadStart = waves->offset + (uint64_t)waves->count * WAVE_SIZE;
    uint64_t wavesEnd = waves->offset + waves->size;
    for (uint32_t i = 0; i < waves->count; ++i)
    {
        const uint8_t* wave = blob + waves->offset + (uint64_t)i * WAVE_SIZE;
        uint64_t data = read_le(wave + 16, 8);
        uint64_t bytes = read_le(wave + 12, 4);
        if (data % 8 != 0 || data < payloadStart || data > wavesEnd || bytes > wavesEnd - data)
            return NULL;
    }

    PqlBank* bank = malloc(sizeof(*bank));
    if (!bank)
        return NULL;
    bank->buffer = malloc(size);
    if (!bank->buffer)
    {
        free(bank);
        return NULL;
    }
    memcpy(bank->buffer, blob, size);
    bank->voices = (ToneData*)(bank->buffer + sections[VOICES].offset);
    fix_tones(bank->buffer, sections, sections[VOICES].offset);
    for (uint32_t i = 0; i < sections[SUBGROUPS].count; ++i)
        fix_tones(bank->buffer, sections, sections[SUBGROUPS].offset + (uint64_t)i * GROUP_SIZE);
    for (uint32_t i = 0; i < waves->count; ++i)
    {
        const uint8_t* wire = bank->buffer + waves->offset + (uint64_t)i * WAVE_SIZE;
        WaveData wave = {0};
        wave.type = (uint16_t)read_le(wire, 2);
        wave.status = (uint16_t)read_le(wire + 2, 2);
        wave.freq = (uint32_t)read_le(wire + 4, 4);
        wave.loopStart = (uint32_t)read_le(wire + 8, 4);
        wave.size = (uint32_t)read_le(wire + 12, 4);
        wave.data = (int8_t*)(bank->buffer + read_le(wire + 16, 8));
        memcpy(bank->buffer + waves->offset + (size_t)i * sizeof(WaveData), &wave, sizeof(wave));
    }
    for (uint64_t i = 0; i < sections[PROGWAVES].size; i += 4)
    {
        uint8_t* word = bank->buffer + sections[PROGWAVES].offset + i;
        uint32_t native = (uint32_t)read_le(word, 4);
        memcpy(word, &native, sizeof(native));
    }
    return bank;
}

/* Return borrowed voices without exposing the backing allocation. */
ToneData* pql_bank_voices(PqlBank* bank)
{
    return bank ? bank->voices : NULL;
}

/* A bank owns only its handle and one allocation containing all engine data. */
void pql_bank_free(PqlBank* bank)
{
    if (bank)
    {
        free(bank->buffer);
        free(bank);
    }
}
