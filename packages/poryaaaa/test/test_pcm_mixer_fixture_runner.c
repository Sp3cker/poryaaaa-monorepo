#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "m4a/m4a_internal.h"
#include "m4a/m4a_pcm_internal.h"
#include "pcm_mixer_case_format.h"

#define FIFO_A_ADDRESS UINT32_C(0x040000A0)
#define FIFO_B_ADDRESS UINT32_C(0x040000A4)

static uint16_t read_u16(const uint8_t* data)
{
    return (uint16_t)((uint16_t)data[0] | ((uint16_t)data[1] << 8u));
}

static uint32_t read_u32(const uint8_t* data)
{
    return (uint32_t)data[0] | ((uint32_t)data[1] << 8u) | ((uint32_t)data[2] << 16u) | ((uint32_t)data[3] << 24u);
}

static uint64_t read_u64(const uint8_t* data)
{
    return (uint64_t)read_u32(data) | ((uint64_t)read_u32(data + 4) << 32u);
}

static bool load_file(const char* path, uint8_t** data, size_t* size)
{
    FILE* input = fopen(path, "rb");
    if (!input)
        return false;
    bool ok = fseek(input, 0, SEEK_END) == 0;
    const long length = ok ? ftell(input) : -1;
    ok = ok && length >= 0 && fseek(input, 0, SEEK_SET) == 0;
    uint8_t* result = ok && length > 0 ? malloc((size_t)length) : NULL;
    ok = ok && result && fread(result, 1, (size_t)length, input) == (size_t)length && fgetc(input) == EOF;
    if (fclose(input) != 0)
        ok = false;
    if (!ok)
    {
        free(result);
        return false;
    }
    *data = result;
    *size = (size_t)length;
    return true;
}

static uint32_t pack_samples(const int8_t* samples)
{
    return (uint32_t)(uint8_t)samples[0] | ((uint32_t)(uint8_t)samples[1] << 8u) |
           ((uint32_t)(uint8_t)samples[2] << 16u) | ((uint32_t)(uint8_t)samples[3] << 24u);
}

static bool write_fifo_row(FILE* output, const uint8_t* schedule, uint32_t expected_address, uint32_t value)
{
    const uint64_t cycle = read_u64(schedule + PCM_CASE_SCHEDULE_CYCLE_OFFSET);
    const uint32_t order = read_u32(schedule + PCM_CASE_SCHEDULE_ORDER_OFFSET);
    const uint32_t address = read_u32(schedule + PCM_CASE_SCHEDULE_ADDRESS_OFFSET);
    if (address != expected_address)
        return false;
    return fprintf(output,
                   "WRITE %" PRIu64 " %" PRIu32 " 4 0x%08" PRIX32 " 0x%08" PRIX32 "\n",
                   cycle,
                   order,
                   address,
                   value) > 0;
}

static bool
initialize_driver(M4ADriver* driver, const uint8_t* descriptor, size_t descriptor_size, WaveData* wave, bool sappy)
{
    const char* descriptor_magic = sappy ? "SPFD" : "IPFD";
    if (descriptor_size < PCM_DESCRIPTOR_WAVE_DATA_OFFSET ||
        memcmp(descriptor + PCM_DESCRIPTOR_MAGIC_OFFSET, descriptor_magic, 4) != 0 ||
        read_u16(descriptor + PCM_DESCRIPTOR_VERSION_OFFSET) != PCM_DESCRIPTOR_VERSION ||
        read_u16(descriptor + PCM_DESCRIPTOR_HEADER_SIZE_OFFSET) != PCM_DESCRIPTOR_HEADER_SIZE ||
        read_u32(descriptor + PCM_DESCRIPTOR_FRAME_SIZE_OFFSET) != PCM_DESCRIPTOR_FRAME_SIZE)
    {
        return false;
    }
    const uint32_t wave_data_size = read_u32(descriptor + PCM_DESCRIPTOR_WAVE_DATA_SIZE_OFFSET);
    if ((uint64_t)PCM_DESCRIPTOR_WAVE_DATA_OFFSET + wave_data_size > descriptor_size)
        return false;
    const uint32_t voice_count = read_u32(descriptor + PCM_DESCRIPTOR_VOICE_COUNT_OFFSET);
    const uint32_t logical_ring_size = read_u32(descriptor + PCM_DESCRIPTOR_LOGICAL_RING_SIZE_OFFSET);
    if (voice_count > M4A_MAX_PCM_CHANNELS || logical_ring_size != PCM_DESCRIPTOR_LOGICAL_RING_SIZE ||
        logical_ring_size > M4A_PCM_MAX_DMA_BUF_SIZE)
        return false;

    memset(driver->pcm.ring_a, (int8_t)descriptor[PCM_DESCRIPTOR_RING_RIGHT_OFFSET], logical_ring_size);
    memset(driver->pcm.ring_b, (int8_t)descriptor[PCM_DESCRIPTOR_RING_LEFT_OFFSET], logical_ring_size);
    driver->pcm_rate_hz = read_u32(descriptor + PCM_DESCRIPTOR_PCM_RATE_HZ_OFFSET);
    driver->pcm_max_samples_per_vblank = PCM_DESCRIPTOR_FRAME_SIZE;
    driver->pcm_dma_buf_size = logical_ring_size;
    driver->pcm_dma_period = (uint8_t)(logical_ring_size / PCM_DESCRIPTOR_FRAME_SIZE);
    driver->max_pcm_channels = (uint8_t)voice_count;
    driver->master_volume = descriptor[PCM_DESCRIPTOR_MASTER_VOLUME_OFFSET];
    driver->reverb_amount = descriptor[PCM_DESCRIPTOR_REVERB_OFFSET];
    driver->active_pcm_mode = sappy ? M4A_PCM_MIXER_SAPPY : M4A_PCM_MIXER_IPATIX;
    driver->requested_pcm_mode = driver->active_pcm_mode;
    m4a_drv_pcm_reset(driver);
    if (!sappy)
    {
        const uint32_t mix_seed = read_u32(descriptor + PCM_DESCRIPTOR_MIX_SEED_OFFSET);
        for (uint32_t index = 0; index < PCM_DESCRIPTOR_FRAME_SIZE / 4u; ++index)
            driver->pcmMixerState.ipatix.packed_mix[index] = mix_seed;
    }

    *wave = (WaveData){
        .type = descriptor[PCM_DESCRIPTOR_WAVE_HEADER_OFFSET],
        .status = descriptor[PCM_DESCRIPTOR_WAVE_HEADER_STATUS_OFFSET] == 0u ? 0u : 0xC000u,
        .freq = read_u32(descriptor + PCM_DESCRIPTOR_WAVE_FREQUENCY_OFFSET),
        .loopStart = read_u32(descriptor + PCM_DESCRIPTOR_WAVE_LOOP_START_OFFSET),
        .size = read_u32(descriptor + PCM_DESCRIPTOR_WAVE_HEADER_LENGTH_OFFSET),
        .data = (int8_t*)(descriptor + PCM_DESCRIPTOR_WAVE_DATA_OFFSET),
    };
    if (wave->size != read_u32(descriptor + PCM_DESCRIPTOR_WAVE_LENGTH_OFFSET))
        return false;

    for (uint32_t index = 0; index < voice_count; ++index)
    {
        M4ADriverPcmChan* channel = &driver->pcmChans[index];
        m4a_drv_pcm_start(driver,
                          channel,
                          wave,
                          descriptor[PCM_DESCRIPTOR_CHANNEL_MODE_OFFSET],
                          read_u32(descriptor + PCM_DESCRIPTOR_START_OFFSET_OFFSET));
        channel->rightVolume = descriptor[PCM_DESCRIPTOR_VOLUME_RIGHT_OFFSET];
        channel->leftVolume = descriptor[PCM_DESCRIPTOR_VOLUME_LEFT_OFFSET];
        channel->attack = descriptor[PCM_DESCRIPTOR_ATTACK_OFFSET];
        channel->decay = descriptor[PCM_DESCRIPTOR_DECAY_OFFSET];
        channel->sustain = descriptor[PCM_DESCRIPTOR_SUSTAIN_OFFSET];
        channel->release = descriptor[PCM_DESCRIPTOR_RELEASE_OFFSET];
        channel->pseudoEchoVolume = descriptor[PCM_DESCRIPTOR_ECHO_VOLUME_OFFSET];
        channel->pseudoEchoLength = descriptor[PCM_DESCRIPTOR_ECHO_LENGTH_OFFSET];
        m4a_drv_pcm_update_pitch(driver, channel, read_u32(descriptor + PCM_DESCRIPTOR_STEP_OFFSET));
    }
    return true;
}

static int replay_fixture(const uint8_t* data, size_t size, FILE* output)
{
    const bool sappy = size >= PCM_CASE_HEADER_SIZE && memcmp(data + PCM_CASE_MAGIC_OFFSET, "SPCA", 4) == 0;
    const char* case_magic = sappy ? "SPCA" : "IPCA";
    if (size < PCM_CASE_HEADER_SIZE || memcmp(data + PCM_CASE_MAGIC_OFFSET, case_magic, 4) != 0 ||
        read_u16(data + PCM_CASE_VERSION_OFFSET) != PCM_CASE_VERSION ||
        read_u16(data + PCM_CASE_HEADER_SIZE_OFFSET) != PCM_CASE_HEADER_SIZE)
    {
        return 1;
    }
    const uint32_t descriptor_size = read_u32(data + PCM_CASE_DESCRIPTOR_SIZE_OFFSET);
    const uint32_t row_count = read_u32(data + PCM_CASE_ROW_COUNT_OFFSET);
    const uint64_t expected_size =
        (uint64_t)PCM_CASE_HEADER_SIZE + descriptor_size + (uint64_t)row_count * PCM_CASE_SCHEDULE_ROW_SIZE;
    if (expected_size != size || row_count == 0u || descriptor_size < PCM_DESCRIPTOR_WAVE_DATA_OFFSET)
        return 1;
    const uint8_t* descriptor = data + PCM_CASE_HEADER_SIZE;
    const uint8_t* schedule = descriptor + descriptor_size;
    const uint32_t block_count = read_u32(descriptor + PCM_DESCRIPTOR_BLOCK_COUNT_OFFSET);
    if (row_count != block_count * (PCM_DESCRIPTOR_FRAME_SIZE / 2u))
        return 1;

    M4ADriver* driver = m4a_driver_create(44100.0f);
    WaveData wave;
    if (!driver || !initialize_driver(driver, descriptor, descriptor_size, &wave, sappy))
    {
        m4a_driver_destroy(driver);
        return 1;
    }
    bool ok = fprintf(output,
                      "PORYAAAA_AUDIO_TRACE 1\nCLOCK 16777216\nBEGIN %" PRIu64 " %" PRIu32 "\n",
                      read_u64(data + PCM_CASE_BEGIN_CYCLE_OFFSET),
                      read_u32(data + PCM_CASE_BEGIN_ORDER_OFFSET)) > 0;
    uint32_t schedule_index = 0u;
    for (uint32_t block = 0; ok && block < block_count; ++block)
    {
        if (block == descriptor[PCM_DESCRIPTOR_RELEASE_BLOCK_OFFSET])
        {
            for (uint32_t channel = 0; channel < read_u32(descriptor + PCM_DESCRIPTOR_VOICE_COUNT_OFFSET); ++channel)
                driver->pcmChans[channel].status |= M4A_CHN_STOP;
        }
        uint32_t segment = read_u32(descriptor + PCM_DESCRIPTOR_START_SEGMENT_OFFSET);
        if ((descriptor[PCM_DESCRIPTOR_FLAGS_OFFSET] & 2u) != 0u)
            segment = (segment + block) % 3u;
        driver->pcm_dma_counter = segment == 0u ? 1u : (uint8_t)(4u - segment);
        const M4APcmBlockOutput block_output = m4a_drv_pcm_render(driver, PCM_DESCRIPTOR_FRAME_SIZE);
        const int8_t* right = block_output.right;
        const int8_t* left = block_output.left;
        for (uint32_t word = 0; ok && word < PCM_DESCRIPTOR_FRAME_SIZE / 4u; ++word)
        {
            ok = write_fifo_row(output,
                                schedule + (size_t)schedule_index * PCM_CASE_SCHEDULE_ROW_SIZE,
                                FIFO_A_ADDRESS,
                                pack_samples(right + word * 4u));
            ++schedule_index;
        }
        for (uint32_t word = 0; ok && word < PCM_DESCRIPTOR_FRAME_SIZE / 4u; ++word)
        {
            ok = write_fifo_row(output,
                                schedule + (size_t)schedule_index * PCM_CASE_SCHEDULE_ROW_SIZE,
                                FIFO_B_ADDRESS,
                                pack_samples(left + word * 4u));
            ++schedule_index;
        }
        const uint32_t base = segment * PCM_DESCRIPTOR_FRAME_SIZE;
        memcpy(driver->pcm.ring_a + base, right, PCM_DESCRIPTOR_FRAME_SIZE);
        memcpy(driver->pcm.ring_b + base, left, PCM_DESCRIPTOR_FRAME_SIZE);
    }
    ok = ok && schedule_index == row_count &&
         fprintf(output,
                 "END %" PRIu64 " %" PRIu32 "\n",
                 read_u64(data + PCM_CASE_END_CYCLE_OFFSET),
                 read_u32(data + PCM_CASE_END_ORDER_OFFSET)) > 0;
    m4a_driver_destroy(driver);
    return ok ? 0 : 1;
}

int main(int argc, char** argv)
{
    const char* input_path = NULL;
    const char* trace_path = NULL;
    for (int index = 1; index < argc; ++index)
    {
        if (index + 1 >= argc)
            return 2;
        if (strcmp(argv[index], "--input") == 0 && !input_path)
            input_path = argv[++index];
        else if (strcmp(argv[index], "--trace-output") == 0 && !trace_path)
            trace_path = argv[++index];
        else
            return 2;
    }
    if (!input_path || !trace_path)
        return 2;

    uint8_t* data = NULL;
    size_t size = 0;
    if (!load_file(input_path, &data, &size))
        return 1;

    FILE* output = fopen(trace_path, "wb");
    if (!output)
    {
        free(data);
        return 1;
    }
    int result = replay_fixture(data, size, output);
    free(data);
    if (fclose(output) != 0)
        result = 1;
    if (result != 0)
        remove(trace_path);
    return result;
}
