#include "hw_audio/hw_audio_trace.h"

#include <errno.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TRACE_LINE_CAPACITY 512

typedef struct
{
    const char* input_path;
    const char* output_prefix;
    uint32_t solo_mask;
} Options;

typedef struct
{
    bool valid;
    uint64_t cycle;
    uint32_t order;
} TracePosition;

/* Keep the recorder interface narrow enough for shell automation. */
static void print_usage(const char* program)
{
    fprintf(stderr,
            "Usage: %s --input TRACE --output-prefix PATH [--solo CHANNELS]\n"
            "CHANNELS: sq1,sq2,wave,noise,fifo-a,fifo-b,psg,directsound,all\n",
            program);
}

/* Parse the same channel names accepted by the full-ROM reference recorder. */
static bool parse_solo_mask(const char* text, uint32_t* mask)
{
    size_t length = strlen(text);
    char* copy = (char*)malloc(length + 1u);
    if (!copy)
        return false;
    memcpy(copy, text, length + 1u);

    uint32_t parsed = 0;
    for (char* token = strtok(copy, ","); token; token = strtok(NULL, ","))
    {
        if (strcmp(token, "sq1") == 0)
            parsed |= HW_AUDIO_SOLO_SQ1;
        else if (strcmp(token, "sq2") == 0)
            parsed |= HW_AUDIO_SOLO_SQ2;
        else if (strcmp(token, "wave") == 0)
            parsed |= HW_AUDIO_SOLO_WAVE;
        else if (strcmp(token, "noise") == 0)
            parsed |= HW_AUDIO_SOLO_NOISE;
        else if (strcmp(token, "fifo-a") == 0 || strcmp(token, "dma-a") == 0)
            parsed |= HW_AUDIO_SOLO_DMA_A;
        else if (strcmp(token, "fifo-b") == 0 || strcmp(token, "dma-b") == 0)
            parsed |= HW_AUDIO_SOLO_DMA_B;
        else if (strcmp(token, "psg") == 0)
            parsed |= HW_AUDIO_SOLO_PSG;
        else if (strcmp(token, "directsound") == 0)
            parsed |= HW_AUDIO_SOLO_DSOUND;
        else if (strcmp(token, "all") == 0)
            parsed |= HW_AUDIO_SOLO_FULL;
        else
        {
            free(copy);
            return false;
        }
    }
    free(copy);
    if (!parsed)
        return false;
    *mask = parsed;
    return true;
}

/* Reject incomplete or ambiguous command lines before creating artifacts. */
static bool parse_options(int argc, char** argv, Options* options)
{
    memset(options, 0, sizeof(*options));
    options->solo_mask = HW_AUDIO_SOLO_FULL;
    for (int index = 1; index < argc; index++)
    {
        if (strcmp(argv[index], "--input") == 0 && index + 1 < argc)
            options->input_path = argv[++index];
        else if (strcmp(argv[index], "--output-prefix") == 0 && index + 1 < argc)
            options->output_prefix = argv[++index];
        else if (strcmp(argv[index], "--solo") == 0 && index + 1 < argc)
        {
            if (!parse_solo_mask(argv[++index], &options->solo_mask))
                return false;
        }
        else
            return false;
    }
    return options->input_path && options->output_prefix;
}

/* Allocate an output path without imposing a platform PATH_MAX. */
static char* path_with_suffix(const char* prefix, const char* suffix)
{
    size_t prefix_length = strlen(prefix);
    size_t suffix_length = strlen(suffix);
    char* path = (char*)malloc(prefix_length + suffix_length + 1u);
    if (!path)
        return NULL;
    memcpy(path, prefix, prefix_length);
    memcpy(path + prefix_length, suffix, suffix_length + 1u);
    return path;
}

/* Make every sample artifact explicitly little-endian. */
static bool write_pcm16_frame(FILE* output, const HwAudioNativeFrame* frame)
{
    uint16_t left = (uint16_t)frame->left;
    uint16_t right = (uint16_t)frame->right;
    uint8_t bytes[4] = {
        (uint8_t)(left & 0xFFu),
        (uint8_t)(left >> 8u),
        (uint8_t)(right & 0xFFu),
        (uint8_t)(right >> 8u),
    };
    return fwrite(bytes, sizeof(bytes), 1u, output) == 1u;
}

/* Preserve each frame's absolute GBA cycle for exact timing comparison. */
static bool write_cycle(FILE* output, uint64_t cycle)
{
    uint8_t bytes[8];
    for (unsigned index = 0; index < 8; index++)
        bytes[index] = (uint8_t)(cycle >> (index * 8u));
    return fwrite(bytes, sizeof(bytes), 1u, output) == 1u;
}

/* Require one total order across writes, samples, and measurement markers. */
static bool advance_position(TracePosition* position, uint64_t cycle, uint32_t order)
{
    if (position->valid && (cycle < position->cycle || (cycle == position->cycle && order <= position->order)))
        return false;
    position->valid = true;
    position->cycle = cycle;
    position->order = order;
    return true;
}

/* Emit the canonical metadata consumed by native_compare.py. */
static bool
write_manifest(const char* path, uint64_t frame_count, uint64_t first_cycle, uint64_t last_cycle, uint32_t solo_mask)
{
    FILE* output = fopen(path, "wb");
    if (!output)
        return false;
    int written = fprintf(output,
                          "{\n"
                          "  \"format\": \"poryaaaa-native-capture\",\n"
                          "  \"version\": 1,\n"
                          "  \"source\": \"poryaaaa\",\n"
                          "  \"clock_hz\": %u,\n"
                          "  \"channels\": 2,\n"
                          "  \"sample_format\": \"s16le\",\n"
                          "  \"cycle_format\": \"u64le\",\n"
                          "  \"frame_count\": %" PRIu64 ",\n"
                          "  \"first_cycle\": %" PRIu64 ",\n"
                          "  \"last_cycle\": %" PRIu64 ",\n"
                          "  \"solo_mask\": %u\n"
                          "}\n",
                          HW_AUDIO_GBA_CLOCK_HZ,
                          frame_count,
                          first_cycle,
                          last_cycle,
                          solo_mask);
    bool ok = written > 0;
    if (fflush(output) != 0)
        ok = false;
    if (fclose(output) != 0)
        ok = false;
    if (!ok)
        remove(path);
    return ok;
}

/* Convert one validated trace into atomic PCM, cycle, and manifest artifacts. */
static bool
replay_trace(const Options* options, const char* pcm_temp, const char* cycles_temp, const char* manifest_temp)
{
    FILE* input = fopen(options->input_path, "rb");
    FILE* pcm = fopen(pcm_temp, "wb");
    FILE* cycles = fopen(cycles_temp, "wb");
    if (!input || !pcm || !cycles)
    {
        fprintf(stderr, "Could not open trace artifacts: %s\n", strerror(errno));
        if (input)
            fclose(input);
        if (pcm)
            fclose(pcm);
        if (cycles)
            fclose(cycles);
        return false;
    }

    HwAudio* audio = hw_audio_create(65536.0f);
    if (!audio)
    {
        fprintf(stderr, "Could not allocate poryaaaa audio trace runner\n");
        fclose(input);
        fclose(pcm);
        fclose(cycles);
        return false;
    }
    hw_audio_trace_reset(audio);
    hw_audio_set_solo_mask(audio, options->solo_mask);

    bool ok = true;
    bool clock_seen = false;
    bool measurement_open = false;
    bool measurement_closed = false;
    uint64_t frame_count = 0;
    uint64_t first_cycle = 0;
    uint64_t last_cycle = 0;
    TracePosition position = {0};
    char line[TRACE_LINE_CAPACITY];
    unsigned line_number = 0;

    if (!fgets(line, sizeof(line), input) || strcmp(line, "PORYAAAA_AUDIO_TRACE 1\n") != 0)
    {
        fprintf(stderr, "Trace must begin with PORYAAAA_AUDIO_TRACE 1\n");
        ok = false;
    }
    line_number++;

    while (ok && fgets(line, sizeof(line), input))
    {
        line_number++;
        if (!strchr(line, '\n') && !feof(input))
        {
            fprintf(stderr, "Trace line %u exceeds %d bytes\n", line_number, TRACE_LINE_CAPACITY - 1);
            ok = false;
            break;
        }
        if (line[0] == '#' || line[0] == '\n' || line[0] == '\r')
            continue;

        uint64_t cycle = 0;
        uint32_t order = 0;
        uint32_t address = 0;
        uint32_t value = 0;
        unsigned width = 0;
        unsigned clock = 0;
        char extra = 0;

        if (sscanf(line, "CLOCK %u %c", &clock, &extra) == 1)
        {
            if (clock_seen || position.valid || clock != HW_AUDIO_GBA_CLOCK_HZ)
            {
                fprintf(stderr, "Invalid CLOCK declaration on trace line %u\n", line_number);
                ok = false;
            }
            clock_seen = true;
            continue;
        }

        if (!clock_seen)
        {
            fprintf(stderr, "Trace line %u precedes CLOCK declaration\n", line_number);
            ok = false;
            break;
        }

        if (sscanf(line, "BEGIN %" SCNu64 " %" SCNu32 " %c", &cycle, &order, &extra) == 2)
        {
            if (measurement_open || measurement_closed || !advance_position(&position, cycle, order))
            {
                fprintf(stderr, "Invalid BEGIN marker on trace line %u\n", line_number);
                ok = false;
            }
            measurement_open = true;
            continue;
        }
        if (sscanf(line, "END %" SCNu64 " %" SCNu32 " %c", &cycle, &order, &extra) == 2)
        {
            if (!measurement_open || measurement_closed || !advance_position(&position, cycle, order))
            {
                fprintf(stderr, "Invalid END marker on trace line %u\n", line_number);
                ok = false;
            }
            measurement_open = false;
            measurement_closed = true;
            continue;
        }
        if (measurement_closed)
        {
            fprintf(stderr, "Trace event follows END on line %u\n", line_number);
            ok = false;
            break;
        }

        HwAudioTraceEvent event = {0};
        if (sscanf(line,
                   "WRITE %" SCNu64 " %" SCNu32 " %u %" SCNx32 " %" SCNx32 " %c",
                   &cycle,
                   &order,
                   &width,
                   &address,
                   &value,
                   &extra) == 5)
        {
            if (width > UINT8_MAX || !advance_position(&position, cycle, order))
            {
                fprintf(stderr, "Invalid WRITE ordering or width on trace line %u\n", line_number);
                ok = false;
                break;
            }
            event.cycle = cycle;
            event.order = order;
            event.kind = HW_AUDIO_TRACE_WRITE;
            event.width = (uint8_t)width;
            event.address = address;
            event.value = value;
        }
        else if (sscanf(line, "SAMPLE %" SCNu64 " %" SCNu32 " %c", &cycle, &order, &extra) == 2)
        {
            if (!advance_position(&position, cycle, order))
            {
                fprintf(stderr, "Invalid SAMPLE ordering on trace line %u\n", line_number);
                ok = false;
                break;
            }
            event.cycle = cycle;
            event.order = order;
            event.kind = HW_AUDIO_TRACE_SAMPLE;
        }
        else if (sscanf(line, "TIMER %" SCNu64 " %" SCNu32 " %" SCNu32 " %c", &cycle, &order, &value, &extra) == 3)
        {
            if (value > 1 || !advance_position(&position, cycle, order))
            {
                fprintf(stderr, "Invalid TIMER value or ordering on trace line %u\n", line_number);
                ok = false;
                break;
            }
            event.cycle = cycle;
            event.order = order;
            event.kind = HW_AUDIO_TRACE_TIMER;
            event.value = value;
        }
        else
        {
            fprintf(stderr, "Unrecognized trace line %u\n", line_number);
            ok = false;
            break;
        }

        HwAudioNativeFrame frame;
        HwAudioTraceStatus status = hw_audio_trace_apply(audio, &event, &frame);
        if (status != HW_AUDIO_TRACE_OK)
        {
            fprintf(
                stderr, "Trace line %u cannot be replayed: %s\n", line_number, hw_audio_trace_status_string(status));
            ok = false;
            break;
        }
        if (event.kind == HW_AUDIO_TRACE_SAMPLE && measurement_open)
        {
            if (!write_pcm16_frame(pcm, &frame) || !write_cycle(cycles, frame.cycle))
            {
                fprintf(stderr, "Could not write native capture: %s\n", strerror(errno));
                ok = false;
                break;
            }
            if (frame_count == 0)
                first_cycle = frame.cycle;
            last_cycle = frame.cycle;
            frame_count++;
        }
    }

    if (ok && ferror(input))
    {
        fprintf(stderr, "Could not read trace: %s\n", strerror(errno));
        ok = false;
    }
    if (ok && (!clock_seen || measurement_open || !measurement_closed || frame_count == 0))
    {
        fprintf(stderr, "Trace requires CLOCK, one closed measurement, and at least one captured SAMPLE\n");
        ok = false;
    }
    if (fflush(pcm) != 0)
        ok = false;
    if (fflush(cycles) != 0)
        ok = false;
    if (fclose(input) != 0)
        ok = false;
    if (fclose(pcm) != 0)
        ok = false;
    if (fclose(cycles) != 0)
        ok = false;
    hw_audio_destroy(audio);

    if (ok)
        ok = write_manifest(manifest_temp, frame_count, first_cycle, last_cycle, options->solo_mask);
    return ok;
}

/* Publish complete artifacts only after the full trace validates and replays. */
int main(int argc, char** argv)
{
    Options options;
    if (!parse_options(argc, argv, &options))
    {
        print_usage(argv[0]);
        return 2;
    }

    char* pcm_path = path_with_suffix(options.output_prefix, ".pcm");
    char* cycles_path = path_with_suffix(options.output_prefix, ".cycles");
    char* manifest_path = path_with_suffix(options.output_prefix, ".json");
    char* pcm_temp = path_with_suffix(options.output_prefix, ".pcm.tmp");
    char* cycles_temp = path_with_suffix(options.output_prefix, ".cycles.tmp");
    char* manifest_temp = path_with_suffix(options.output_prefix, ".json.tmp");
    if (!pcm_path || !cycles_path || !manifest_path || !pcm_temp || !cycles_temp || !manifest_temp)
    {
        fprintf(stderr, "Could not allocate output paths\n");
        free(pcm_path);
        free(cycles_path);
        free(manifest_path);
        free(pcm_temp);
        free(cycles_temp);
        free(manifest_temp);
        return 1;
    }

    bool ok = replay_trace(&options, pcm_temp, cycles_temp, manifest_temp);
    if (ok)
    {
        remove(pcm_path);
        remove(cycles_path);
        remove(manifest_path);
        bool pcm_published = rename(pcm_temp, pcm_path) == 0;
        bool cycles_published = rename(cycles_temp, cycles_path) == 0;
        bool manifest_published = rename(manifest_temp, manifest_path) == 0;
        ok = pcm_published && cycles_published && manifest_published;
        if (!ok)
        {
            fprintf(stderr, "Could not publish capture artifacts: %s\n", strerror(errno));
            remove(pcm_path);
            remove(cycles_path);
            remove(manifest_path);
        }
    }
    if (!ok)
    {
        remove(pcm_temp);
        remove(cycles_temp);
        remove(manifest_temp);
    }

    free(pcm_path);
    free(cycles_path);
    free(manifest_path);
    free(pcm_temp);
    free(cycles_temp);
    free(manifest_temp);
    return ok ? 0 : 1;
}
