#include "poryaaaa_quicklook_bank.h"
#include "m4a_engine.h"
#include "voicegroup_loader.h"
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Unlike assert(), checks remain active in Release builds. */
#define CHECK(condition)                                                                                               \
    do                                                                                                                 \
    {                                                                                                                  \
        if (!(condition))                                                                                              \
        {                                                                                                              \
            fprintf(stderr, "%s:%d: check failed: %s\n", __FILE__, __LINE__, #condition);                              \
            exit(EXIT_FAILURE);                                                                                        \
        }                                                                                                              \
    } while (0)

/* Decode and mutate wire fields for corruption tests, never native structs. */
static uint64_t read_le(const uint8_t* p, unsigned bytes)
{
    uint64_t value = 0;
    for (unsigned i = 0; i < bytes; ++i)
        value |= (uint64_t)p[i] << (8 * i);
    return value;
}

static void write_le(uint8_t* p, uint64_t value, unsigned bytes)
{
    for (unsigned i = 0; i < bytes; ++i)
        p[i] = (uint8_t)(value >> (8 * i));
}

/* Read the committed asset independently of the source decomp project. */
static uint8_t* read_file(const char* path, size_t* size)
{
    FILE* file = fopen(path, "rb");
    CHECK(file != NULL);
    CHECK(fseek(file, 0, SEEK_END) == 0);
    long length = ftell(file);
    CHECK(length > 0 && (uintmax_t)length <= SIZE_MAX);
    CHECK(fseek(file, 0, SEEK_SET) == 0);
    *size = (size_t)length;
    uint8_t* blob = malloc(*size);
    CHECK(blob != NULL);
    CHECK(fread(blob, 1, *size, file) == *size);
    CHECK(fclose(file) == 0);
    return blob;
}

/* Pick the same concrete top-level DirectSound program for every render. */
static int first_directsound(const ToneData* voices)
{
    for (int i = 0; i < 128; ++i)
        if (voices[i].type == VOICE_DIRECTSOUND && voices[i].wav && voices[i].wav->size)
            return i;
    CHECK(false);
    return -1;
}

/* Prove the independently loaded bank can drive the real synthesis engine. */
static void test_render(ToneData* voices)
{
    M4AEngine* engine = m4a_engine_create(44100);
    CHECK(engine != NULL);
    m4a_engine_set_voicegroup(engine, voices);
    m4a_engine_program_change(engine, 0, (uint8_t)first_directsound(voices));
    m4a_engine_note_on(engine, 0, 60, 127);
    float left[2048];
    float right[2048];
    float peak = 0;
    for (unsigned block = 0; block < 8; ++block)
    {
        m4a_engine_process(engine, left, right, 2048);
        for (unsigned frame = 0; frame < 2048; ++frame)
        {
            CHECK(isfinite(left[frame]) && isfinite(right[frame]));
            peak = fmaxf(peak, fmaxf(fabsf(left[frame]), fabsf(right[frame])));
        }
    }
    CHECK(peak > 0.01f);
    printf("Blob render: peak=%.6f (8 x 2048 stereo frames)\n", peak);
    m4a_engine_free(engine);
}

/* Every mutation starts from valid bytes so each rejected condition is isolated. */
static void test_corruption(const uint8_t* blob, size_t size)
{
    CHECK(pql_bank_load(NULL, size) == NULL);
    CHECK(pql_bank_load(blob, 0) == NULL);
    CHECK(pql_bank_load(blob, 111) == NULL);
    CHECK(pql_bank_load(blob, size - 1) == NULL);
    uint8_t* bad = malloc(size);
    CHECK(bad != NULL);
    size_t voices = (size_t)read_le(blob + 32, 8);
    size_t waves = (size_t)read_le(blob + 64, 8);
    const struct Mutation
    {
        size_t offset;
        uint64_t value;
        unsigned bytes;
    } mutations[] = {
        {0, 'X', 1},                 /* Magic. */
        {8, 2, 4},                   /* Version. */
        {12, 127, 4},                /* Voice count. */
        {32, UINT64_MAX - 7, 8},     /* Section offset overflow. */
        {40, UINT64_MAX, 8},         /* Section size overflow. */
        {48, voices, 8},             /* Overlapping sections. */
        {voices + 24, 255, 1},       /* Unknown ptr0 kind. */
        {voices + 8, 1, 8},          /* Subgroup offset outside its section. */
        {voices + 16, 1, 8},         /* Invalid table reference. */
        {voices + 25, 1, 1},         /* Reserved bytes. */
        {waves + 16, UINT64_MAX, 8}, /* Invalid sample offset. */
        {waves + 12, UINT32_MAX, 4}, /* Sample payload exceeds section. */
        {waves + 16, waves, 8},      /* Payload aliases wave headers. */
    };
    for (size_t i = 0; i < sizeof(mutations) / sizeof(mutations[0]); ++i)
    {
        memcpy(bad, blob, size);
        write_le(bad + mutations[i].offset, mutations[i].value, mutations[i].bytes);
        CHECK(pql_bank_load(bad, size) == NULL);
    }
    /* Exercise each kind's section granularity and scalar/null validation. */
    const uint64_t offsets[] = {1, 4096, waves + 1, read_le(blob + 80, 8) + 1, read_le(blob + 48, 8) + 32};
    for (unsigned kind = 0; kind < 5; ++kind)
    {
        memcpy(bad, blob, size);
        bad[voices + 24] = (uint8_t)kind;
        write_le(bad + voices + 8, offsets[kind], 8);
        CHECK(pql_bank_load(bad, size) == NULL);
    }
    free(bad);
    puts("Corruption: truncated input, bad magic, layout, reference and payload mutations rejected");
}

typedef struct
{
    const ToneData* original;
    const ToneData* baked;
} GroupPair;

/* Compare every reachable field and payload, also preserving shared group identity. */
static void compare_group(
    const ToneData* original, const ToneData* baked, GroupPair* visited, size_t* visitedCount, size_t capacity)
{
    for (size_t i = 0; i < *visitedCount; ++i)
    {
        if (visited[i].original == original)
        {
            CHECK(visited[i].baked == baked);
            return;
        }
    }
    CHECK(*visitedCount < capacity);
    visited[(*visitedCount)++] = (GroupPair){original, baked};
    for (unsigned i = 0; i < 128; ++i)
    {
        const ToneData* a = &original[i];
        const ToneData* b = &baked[i];
        CHECK(a->type == b->type && a->key == b->key && a->length == b->length && a->panSweep == b->panSweep);
        CHECK(a->attack == b->attack && a->decay == b->decay && a->sustain == b->sustain && a->release == b->release);
        CHECK((a->keySplitTable == NULL) == (b->keySplitTable == NULL));
        if (a->keySplitTable)
            CHECK(memcmp(a->keySplitTable, b->keySplitTable, 128) == 0);
        if ((uintptr_t)a->subGroup < 4096)
            CHECK((uintptr_t)a->subGroup == (uintptr_t)b->subGroup);
        else if (a->type & (VOICE_KEYSPLIT | VOICE_KEYSPLIT_ALL))
        {
            CHECK(b->subGroup != NULL);
            compare_group(a->subGroup, b->subGroup, visited, visitedCount, capacity);
        }
        else if ((a->type & VOICE_TYPE_CGB_MASK) == VOICE_PROGRAMMABLE_WAVE)
        {
            CHECK(b->wavePointer != NULL);
            CHECK(memcmp(a->wavePointer, b->wavePointer, 16) == 0);
        }
        else
        {
            CHECK(b->wav != NULL);
            CHECK(a->wav->type == b->wav->type && a->wav->status == b->wav->status && a->wav->freq == b->wav->freq);
            CHECK(a->wav->loopStart == b->wav->loopStart && a->wav->size == b->wav->size);
            if (a->wav->size)
                CHECK(memcmp(a->wav->data, b->wav->data, a->wav->size) == 0);
        }
    }
}

/* Compare two real engines for both a sample voice and slot-zero percussion. */
static void test_parity(ToneData* voices)
{
    const char* root = getenv("PQL_HEARTH_ROOT");
    if (!root || !*root)
    {
        puts("Parity: SKIP (PQL_HEARTH_ROOT not set)");
        return;
    }
    LoadedVoiceGroup* direct = voicegroup_load(root, "hanabi", NULL);
    CHECK(direct != NULL);
    size_t capacity = (size_t)direct->subGroupCount + 1;
    GroupPair* visited = calloc(capacity, sizeof(*visited));
    CHECK(visited != NULL);
    size_t visitedCount = 0;
    compare_group(direct->voices, voices, visited, &visitedCount, capacity);
    free(visited);
    M4AEngine* engines[2] = {m4a_engine_create(44100), m4a_engine_create(44100)};
    CHECK(engines[0] != NULL && engines[1] != NULL);
    m4a_engine_set_voicegroup(engines[0], direct->voices);
    m4a_engine_set_voicegroup(engines[1], voices);
    int program = first_directsound(direct->voices);
    float left[2][2048];
    float right[2][2048];
    float percussionPeak = 0;
    for (unsigned block = 0; block < 16; ++block)
    {
        for (unsigned engine = 0; engine < 2; ++engine)
        {
            if (block == 0 || block == 6)
            {
                m4a_engine_program_change(engines[engine], 0, block == 0 ? (uint8_t)program : 0);
                m4a_engine_note_on(engines[engine], 0, block == 0 ? 60 : 36, 127);
            }
            if (block == 4 || block == 12)
                m4a_engine_note_off(engines[engine], 0, block == 4 ? 60 : 36);
            m4a_engine_process(engines[engine], left[engine], right[engine], 2048);
        }
        CHECK(memcmp(left[0], left[1], sizeof(left[0])) == 0);
        CHECK(memcmp(right[0], right[1], sizeof(right[0])) == 0);
        if (block >= 6)
            for (unsigned frame = 0; frame < 2048; ++frame)
                percussionPeak = fmaxf(percussionPeak, fmaxf(fabsf(left[1][frame]), fabsf(right[1][frame])));
    }
    CHECK(percussionPeak > 0.01f);
    m4a_engine_free(engines[0]);
    m4a_engine_free(engines[1]);
    voicegroup_free(direct);
    puts("Parity: all reachable fields/payloads equal; 32768 stereo frames sample-exact (DirectSound + slot 0 key 36)");
}

/* Exercise the asset with no project access, then optional source-loader parity. */
int main(int argc, char** argv)
{
    CHECK(argc == 2);
    size_t size;
    uint8_t* blob = read_file(argv[1], &size);
    test_corruption(blob, size);
    PqlBank* bank = pql_bank_load(blob, size);
    CHECK(bank != NULL);
    ToneData* voices = pql_bank_voices(bank);
    CHECK(voices != NULL);
    CHECK(voices[0].type == VOICE_KEYSPLIT_ALL && voices[0].subGroup != NULL);
    CHECK(voices[0].keySplitTable == NULL);
    for (unsigned i = 1; i <= 6; ++i)
        CHECK(voices[i].type == VOICE_KEYSPLIT && voices[i].subGroup != NULL && voices[i].keySplitTable != NULL);
    bool foundProgrammable = false;
    for (unsigned i = 0; i < 128; ++i)
        if (voices[i].type == VOICE_PROGRAMMABLE_WAVE && voices[i].wavePointer)
        {
            volatile uint32_t readable = voices[i].wavePointer[0];
            (void)readable;
            foundProgrammable = true;
        }
    CHECK(foundProgrammable);
    int program = first_directsound(voices);
    CHECK(voices[program].wav->data != NULL);
    volatile int8_t readable = voices[program].wav->data[voices[program].wav->size - 1];
    (void)readable;
    /* Destroy the caller's input to prove bank ownership before engine rendering. */
    memset(blob, 0, size);
    free(blob);
    test_render(voices);
    test_parity(voices);
    pql_bank_free(bank);
    CHECK(pql_bank_voices(NULL) == NULL);
    pql_bank_free(NULL);
    puts("quicklook_bank: PASS");
    return EXIT_SUCCESS;
}
