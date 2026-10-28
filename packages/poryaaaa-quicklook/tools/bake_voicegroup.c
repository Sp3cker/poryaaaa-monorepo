#include "voicegroup_loader.h"
#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* See README.md: the wire representation never dumps native structures. */
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

typedef struct
{
    const void** pointers;
    uint32_t count;
} PointerMap;

typedef struct
{
    size_t offset;
    size_t size;
} Section;

/* Allocate identity-map capacity once using the loader's registered count. */
static bool init_map(PointerMap* map, int capacity)
{
    if (capacity < 0 || (size_t)capacity > SIZE_MAX / sizeof(*map->pointers))
        return false;
    if (capacity == 0)
        return true;
    map->pointers = malloc((size_t)capacity * sizeof(*map->pointers));
    return map->pointers != NULL;
}

/* Preserve first occurrence order, deduplicating only identical pointers. */
static bool add_pointer(PointerMap* map, const void* pointer)
{
    if (!pointer)
    {
        fprintf(stderr, "bake_voicegroup: NULL entry in registered resource array\n");
        return false;
    }
    for (uint32_t i = 0; i < map->count; ++i)
        if (map->pointers[i] == pointer)
            return true;
    map->pointers[map->count++] = pointer;
    return true;
}

/* Encode every multi-byte field explicitly in little-endian order. */
static void write_le(uint8_t* p, uint64_t value, unsigned bytes)
{
    for (unsigned i = 0; i < bytes; ++i)
        p[i] = (uint8_t)(value >> (8 * i));
}

/* Reserve an aligned section without overflowing size_t. */
static bool reserve_section(Section* section, size_t* cursor, uint64_t bytes)
{
    if (*cursor > SIZE_MAX - 7 || bytes > SIZE_MAX)
        return false;
    size_t offset = (*cursor + 7) & ~(size_t)7;
    if (bytes > SIZE_MAX - offset)
        return false;
    section->offset = offset;
    section->size = (size_t)bytes;
    *cursor = offset + (size_t)bytes;
    return true;
}

/* Map a real pointer to its unique wire slot; unmapped pointers are fatal. */
static bool
pointer_offset(const void* pointer, const PointerMap* map, const Section* section, size_t stride, uint64_t* offset)
{
    for (uint32_t i = 0; i < map->count; ++i)
    {
        if (map->pointers[i] == pointer)
        {
            *offset = section->offset + (uint64_t)i * stride;
            return true;
        }
    }
    return false;
}

/* Serialize one full group, including scalar pseudo-pointers used by PSG voices. */
static bool write_tones(uint8_t* output, const ToneData* voices, const PointerMap* maps, const Section* sections)
{
    for (unsigned i = 0; i < 128; ++i)
    {
        const ToneData* tone = &voices[i];
        uint8_t* wire = output + i * TONE_SIZE;
        uint64_t ptr0 = 0;
        uint64_t ptr1 = 0;
        uint8_t kind = PTR_NULL;
        const void* pointer = tone->subGroup;
        if (pointer)
        {
            if ((uintptr_t)pointer < 4096)
            {
                kind = PTR_SCALAR;
                ptr0 = (uintptr_t)pointer;
            }
            else if (pointer_offset(pointer, &maps[WAVES], &sections[WAVES], WAVE_SIZE, &ptr0))
                kind = PTR_WAVE;
            else if (pointer_offset(pointer, &maps[PROGWAVES], &sections[PROGWAVES], 16, &ptr0))
                kind = PTR_PROGWAVE;
            else if (pointer_offset(pointer, &maps[SUBGROUPS], &sections[SUBGROUPS], GROUP_SIZE, &ptr0))
                kind = PTR_SUBGROUP;
            else
            {
                fprintf(stderr, "bake_voicegroup: unmapped ptr0 in tone %u (type 0x%02x)\n", i, tone->type);
                return false;
            }
        }
        if (tone->keySplitTable && !pointer_offset(tone->keySplitTable, &maps[TABLES], &sections[TABLES], 128, &ptr1))
        {
            fprintf(stderr, "bake_voicegroup: unmapped keysplit table in tone %u\n", i);
            return false;
        }
        wire[0] = tone->type;
        wire[1] = tone->key;
        wire[2] = tone->length;
        wire[3] = tone->panSweep;
        wire[4] = tone->attack;
        wire[5] = tone->decay;
        wire[6] = tone->sustain;
        wire[7] = tone->release;
        write_le(wire + 8, ptr0, 8);
        write_le(wire + 16, ptr1, 8);
        wire[24] = kind;
    }
    return true;
}

/* Load once, build deterministic identity maps, and write one pointer-free bank. */
int main(int argc, char** argv)
{
    if (argc != 4)
    {
        fprintf(stderr, "Usage: bake_voicegroup <projectRoot> <voicegroupName> <outputPath>\n");
        return EXIT_FAILURE;
    }
    const uint32_t endianCheck = 1;
    if (*(const uint8_t*)&endianCheck != 1)
    {
        fprintf(stderr, "bake_voicegroup: little-endian host required for raw programmable wave bytes\n");
        return EXIT_FAILURE;
    }
    LoadedVoiceGroup* vg = voicegroup_load(argv[1], argv[2], NULL);
    if (!vg)
    {
        fprintf(stderr, "bake_voicegroup: failed to load voicegroup '%s'\n", argv[2]);
        return EXIT_FAILURE;
    }

    int result = EXIT_FAILURE;
    PointerMap maps[SECTION_COUNT] = {0};
    Section sections[SECTION_COUNT] = {0};
    uint8_t* blob = NULL;
    uint64_t* sampleOffsets = NULL;
    FILE* file = NULL;
    if (!init_map(&maps[SUBGROUPS], vg->subGroupCount) || !init_map(&maps[WAVES], vg->waveDataCount) ||
        !init_map(&maps[PROGWAVES], vg->progWaveCount) || !init_map(&maps[TABLES], vg->keySplitTableCount))
        goto allocation_error;
    if ((vg->subGroupCount && !vg->subGroups) || (vg->waveDataCount && !vg->waveDatas) ||
        (vg->progWaveCount && !vg->progWaves) || (vg->keySplitTableCount && !vg->keySplitTables))
    {
        fprintf(stderr, "bake_voicegroup: missing registered resource array\n");
        goto cleanup;
    }
    for (int i = 0; i < vg->subGroupCount; ++i)
        if (!add_pointer(&maps[SUBGROUPS], vg->subGroups[i]))
            goto cleanup;
    for (int i = 0; i < vg->waveDataCount; ++i)
        if (!add_pointer(&maps[WAVES], vg->waveDatas[i]))
            goto cleanup;
    for (int i = 0; i < vg->progWaveCount; ++i)
        if (!add_pointer(&maps[PROGWAVES], vg->progWaves[i]))
            goto cleanup;
    for (int i = 0; i < vg->keySplitTableCount; ++i)
        if (!add_pointer(&maps[TABLES], vg->keySplitTables[i]))
            goto cleanup;

    size_t cursor = HEADER_SIZE;
    if (!reserve_section(&sections[VOICES], &cursor, GROUP_SIZE) ||
        !reserve_section(&sections[SUBGROUPS], &cursor, (uint64_t)maps[SUBGROUPS].count * GROUP_SIZE) ||
        !reserve_section(&sections[WAVES], &cursor, (uint64_t)maps[WAVES].count * WAVE_SIZE))
        goto allocation_error;
    if (maps[WAVES].count)
    {
        if ((uint64_t)maps[WAVES].count * sizeof(*sampleOffsets) > SIZE_MAX)
            goto allocation_error;
        sampleOffsets = malloc((size_t)maps[WAVES].count * sizeof(*sampleOffsets));
        if (!sampleOffsets)
            goto allocation_error;
    }
    for (uint32_t i = 0; i < maps[WAVES].count; ++i)
    {
        const WaveData* wave = maps[WAVES].pointers[i];
        Section payload;
        if (wave->size && !wave->data)
        {
            fprintf(stderr, "bake_voicegroup: wave %u has size but no sample data\n", i);
            goto cleanup;
        }
        if (!reserve_section(&payload, &cursor, wave->size))
            goto allocation_error;
        sampleOffsets[i] = payload.offset;
    }
    sections[WAVES].size = cursor - sections[WAVES].offset;
    if (!reserve_section(&sections[PROGWAVES], &cursor, (uint64_t)maps[PROGWAVES].count * 16) ||
        !reserve_section(&sections[TABLES], &cursor, (uint64_t)maps[TABLES].count * 128))
        goto allocation_error;
    blob = calloc(1, cursor);
    if (!blob)
        goto allocation_error;
    memcpy(blob, "PQLBANK1", 8);
    write_le(blob + 8, 1, 4);
    write_le(blob + 12, 128, 4);
    for (unsigned i = 1; i < SECTION_COUNT; ++i)
        write_le(blob + 12 + i * 4, maps[i].count, 4);
    for (unsigned i = 0; i < SECTION_COUNT; ++i)
    {
        write_le(blob + 32 + i * 16, sections[i].offset, 8);
        write_le(blob + 40 + i * 16, sections[i].size, 8);
    }
    if (!write_tones(blob + sections[VOICES].offset, vg->voices, maps, sections))
        goto cleanup;
    for (uint32_t i = 0; i < maps[SUBGROUPS].count; ++i)
        if (!write_tones(blob + sections[SUBGROUPS].offset + (size_t)i * GROUP_SIZE,
                         maps[SUBGROUPS].pointers[i],
                         maps,
                         sections))
            goto cleanup;
    for (uint32_t i = 0; i < maps[WAVES].count; ++i)
    {
        const WaveData* wave = maps[WAVES].pointers[i];
        uint8_t* wire = blob + sections[WAVES].offset + (size_t)i * WAVE_SIZE;
        write_le(wire, wave->type, 2);
        write_le(wire + 2, wave->status, 2);
        write_le(wire + 4, wave->freq, 4);
        write_le(wire + 8, wave->loopStart, 4);
        write_le(wire + 12, wave->size, 4);
        write_le(wire + 16, sampleOffsets[i], 8);
        if (wave->size)
            memcpy(blob + sampleOffsets[i], wave->data, wave->size);
    }
    for (uint32_t i = 0; i < maps[PROGWAVES].count; ++i)
        memcpy(blob + sections[PROGWAVES].offset + (size_t)i * 16, maps[PROGWAVES].pointers[i], 16);
    for (uint32_t i = 0; i < maps[TABLES].count; ++i)
        memcpy(blob + sections[TABLES].offset + (size_t)i * 128, maps[TABLES].pointers[i], 128);

    file = fopen(argv[3], "wb");
    if (!file)
    {
        fprintf(stderr, "bake_voicegroup: cannot open output '%s'\n", argv[3]);
        goto cleanup;
    }
    bool written = fwrite(blob, 1, cursor, file) == cursor;
    int closeResult = fclose(file);
    file = NULL;
    if (!written || closeResult != 0)
    {
        fprintf(stderr, "bake_voicegroup: failed writing output '%s'\n", argv[3]);
        goto cleanup;
    }
    printf("voices=128 subGroups=%" PRIu32 " waves=%" PRIu32 " progWaves=%" PRIu32 " tables=%" PRIu32 " bytes=%zu\n",
           maps[SUBGROUPS].count,
           maps[WAVES].count,
           maps[PROGWAVES].count,
           maps[TABLES].count,
           cursor);
    result = EXIT_SUCCESS;
    goto cleanup;

allocation_error:
    fprintf(stderr, "bake_voicegroup: allocation or bank size overflow\n");
cleanup:
    if (file)
        fclose(file);
    free(blob);
    free(sampleOffsets);
    for (unsigned i = 0; i < SECTION_COUNT; ++i)
        free(maps[i].pointers);
    voicegroup_free(vg);
    return result;
}
