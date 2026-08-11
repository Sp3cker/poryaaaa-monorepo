#include "hw_audio/hw_audio_trace_text.h"

#include <limits.h>

#include <errno.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct
{
    const char* input_path;
    const char* output_prefix;
    const char* oracle_path;
    const char* driver_path;
    uint32_t solo_mask;
    bool compare_driver;
} Options;

/* Keep replay and event comparison interface narrow enough for shell automation. */
static void print_usage(const char* program)
{
    fprintf(stderr,
            "Usage: %s --input TRACE --output-prefix PATH [--solo CHANNELS]\n"
            "       %s --compare-driver --oracle TRACE --driver TRACE\n"
            "CHANNELS: sq1,sq2,wave,noise,fifo-a,fifo-b,psg,directsound,all\n",
            program,
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
        else if (strcmp(argv[index], "--oracle") == 0 && index + 1 < argc)
            options->oracle_path = argv[++index];
        else if (strcmp(argv[index], "--driver") == 0 && index + 1 < argc)
            options->driver_path = argv[++index];
        else if (strcmp(argv[index], "--compare-driver") == 0)
            options->compare_driver = true;
        else if (strcmp(argv[index], "--solo") == 0 && index + 1 < argc)
        {
            if (!parse_solo_mask(argv[++index], &options->solo_mask))
                return false;
        }
        else
            return false;
    }
    if (options->compare_driver)
    {
        return options->oracle_path && options->driver_path && !options->input_path && !options->output_prefix;
    }
    return options->input_path && options->output_prefix && !options->oracle_path && !options->driver_path;
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

typedef struct
{
    HwAudioTraceEvent* events;
    size_t count;
    size_t capacity;
    bool allocation_failed;
    bool cycle_out_of_range;
} ReplayEvents;
/* Retain hardware events so FIFO sample bins can be resolved before replay. */
static bool collect_replay_event(void* context, const HwAudioTraceTextRecord* record)
{
    ReplayEvents* collected = context;
    if (record->cycle > (uint64_t)INT32_MAX)
    {
        collected->cycle_out_of_range = true;
        return false;
    }
    if (record->kind == HW_AUDIO_TRACE_TEXT_BEGIN || record->kind == HW_AUDIO_TRACE_TEXT_END)
        return true;
    if (collected->count == collected->capacity)
    {
        size_t capacity = collected->capacity ? collected->capacity * 2u : 1024u;
        if (capacity < collected->capacity || capacity > SIZE_MAX / sizeof(*collected->events))
        {
            collected->allocation_failed = true;
            return false;
        }
        HwAudioTraceEvent* events = realloc(collected->events, capacity * sizeof(*collected->events));
        if (!events)
        {
            collected->allocation_failed = true;
            return false;
        }
        collected->events = events;
        collected->capacity = capacity;
    }
    collected->events[collected->count++] = record->event;
    return true;
}

typedef struct
{
    HwAudio* audio;
    FILE* pcm;
    FILE* cycles;
    const HwAudioTraceFifoSample* fifo_samples;
    size_t fifo_sample_count;
    size_t fifo_sample_index;
    const HwAudioTraceEvent* events;
    size_t event_count;
    size_t event_index;
    HwAudioTraceFifoSample pending_fifo_sample;
    uint64_t pending_sample_cycle;
    uint64_t pending_sample_deadline;
    bool pending_sample;
    bool pending_measurement;
    bool measurement_open;
    uint64_t frame_count;
    uint64_t first_cycle;
    uint64_t last_cycle;
    HwAudioTraceStatus apply_status;
    bool output_failed;
} ReplayContext;

/* Emit one staged sample when mGBA's next native sample interval matures. */
static bool emit_pending_sample(ReplayContext* replay)
{
    if (!replay->pending_sample)
        return true;

    HwAudioNativeFrame frame;
    replay->apply_status = hw_audio_trace_observe_sample(
        replay->audio, replay->pending_sample_cycle, &replay->pending_fifo_sample, &frame);
    if (replay->apply_status != HW_AUDIO_TRACE_OK)
        return false;
    replay->pending_sample = false;
    if (!replay->pending_measurement)
        return true;
    if (!write_pcm16_frame(replay->pcm, &frame) || !write_cycle(replay->cycles, frame.cycle))
    {
        replay->output_failed = true;
        return false;
    }
    if (replay->frame_count == 0)
        replay->first_cycle = frame.cycle;
    replay->last_cycle = frame.cycle;
    replay->frame_count++;
    return true;
}

/* Replay each parsed event while preserving the trace interval for PCM output. */
static bool replay_record(void* context, const HwAudioTraceTextRecord* record)
{
    ReplayContext* replay = context;
    if (record->kind == HW_AUDIO_TRACE_TEXT_BEGIN)
    {
        replay->measurement_open = true;
        return true;
    }
    if (record->kind == HW_AUDIO_TRACE_TEXT_END)
    {
        if (!emit_pending_sample(replay))
            return false;
        replay->measurement_open = false;
        return true;
    }

    if (replay->event_index >= replay->event_count)
    {
        replay->apply_status = HW_AUDIO_TRACE_INVALID_ARGUMENT;
        return false;
    }
    size_t event_index = replay->event_index++;
    if (replay->pending_sample && record->event.cycle >= replay->pending_sample_deadline &&
        !emit_pending_sample(replay))
        return false;

    HwAudioNativeFrame frame;
    if (record->event.kind == HW_AUDIO_TRACE_SAMPLE)
    {
        if (!emit_pending_sample(replay))
            return false;
        if (replay->fifo_sample_index >= replay->fifo_sample_count)
        {
            replay->apply_status = HW_AUDIO_TRACE_INVALID_ARGUMENT;
            return false;
        }
        replay->apply_status = hw_audio_trace_stage_sample(replay->audio, &record->event, &frame);
        if (replay->apply_status != HW_AUDIO_TRACE_OK)
            return false;
        replay->pending_fifo_sample = replay->fifo_samples[replay->fifo_sample_index++];
        replay->pending_sample_cycle = record->event.cycle;
        replay->pending_sample = true;
        replay->pending_sample_deadline = UINT64_MAX;
        for (size_t index = event_index + 1u; index < replay->event_count; ++index)
        {
            if (replay->events[index].kind == HW_AUDIO_TRACE_SAMPLE)
            {
                replay->pending_sample_deadline = replay->events[index].cycle;
                break;
            }
        }
        replay->pending_measurement = replay->measurement_open;
        return true;
    }

    replay->apply_status = hw_audio_trace_apply(replay->audio, &record->event, &frame);
    return replay->apply_status == HW_AUDIO_TRACE_OK;
}

/* Convert one shared-grammar trace into atomic PCM, cycle, and manifest artifacts. */
static bool
replay_trace(const Options* options, const char* pcm_temp, const char* cycles_temp, const char* manifest_temp)
{
    FILE* input = fopen(options->input_path, "rb");
    if (!input)
    {
        fprintf(stderr, "Could not open trace input: %s\n", strerror(errno));
        return false;
    }

    ReplayEvents collected = {0};
    unsigned error_line = 0;
    HwAudioTraceTextStatus parse_status =
        hw_audio_trace_text_read(input, collect_replay_event, &collected, &error_line);
    if (parse_status != HW_AUDIO_TRACE_TEXT_OK)
    {
        if (collected.cycle_out_of_range)
            fprintf(stderr, "Invalid trace line %u: event cycle exceeds INT32_MAX\n", error_line);
        else if (collected.allocation_failed)
            fprintf(stderr, "Could not allocate replay event timeline\n");
        else if (error_line)
            fprintf(stderr, "Invalid trace line %u: %s\n", error_line, hw_audio_trace_text_status_string(parse_status));
        else
            fprintf(stderr, "Invalid trace: %s\n", hw_audio_trace_text_status_string(parse_status));
        free(collected.events);
        fclose(input);
        return false;
    }

    HwAudioTraceFifoSample* fifo_samples = calloc(collected.count ? collected.count : 1u, sizeof(*fifo_samples));
    size_t fifo_sample_count = 0;
    HwAudioTraceStatus schedule_status =
        fifo_samples ? hw_audio_trace_schedule_fifo_samples(
                           collected.events, collected.count, fifo_samples, collected.count, &fifo_sample_count)
                     : HW_AUDIO_TRACE_INVALID_ARGUMENT;
    if (schedule_status != HW_AUDIO_TRACE_OK || fseek(input, 0, SEEK_SET) != 0)
    {
        fprintf(stderr,
                "Could not schedule DirectSound native samples: %s\n",
                hw_audio_trace_status_string(schedule_status));
        free(fifo_samples);
        free(collected.events);
        fclose(input);
        return false;
    }

    FILE* pcm = fopen(pcm_temp, "wb");
    FILE* cycles = fopen(cycles_temp, "wb");
    if (!pcm || !cycles)
    {
        fprintf(stderr, "Could not open trace artifacts: %s\n", strerror(errno));
        if (pcm)
            fclose(pcm);
        if (cycles)
            fclose(cycles);
        free(fifo_samples);
        free(collected.events);
        fclose(input);
        return false;
    }

    HwAudio* audio = hw_audio_create(65536.0f);
    if (!audio)
    {
        fprintf(stderr, "Could not allocate poryaaaa audio trace runner\n");
        fclose(input);
        fclose(pcm);
        fclose(cycles);
        free(fifo_samples);
        free(collected.events);
        return false;
    }
    hw_audio_trace_reset(audio);
    hw_audio_set_solo_mask(audio, options->solo_mask);

    ReplayContext replay = {
        .audio = audio,
        .pcm = pcm,
        .cycles = cycles,
        .fifo_samples = fifo_samples,
        .events = collected.events,
        .event_count = collected.count,
        .fifo_sample_count = fifo_sample_count,
        .apply_status = HW_AUDIO_TRACE_OK,
    };
    error_line = 0;
    parse_status = hw_audio_trace_text_read(input, replay_record, &replay, &error_line);
    bool ok = parse_status == HW_AUDIO_TRACE_TEXT_OK;
    if (!ok)
    {
        if (replay.apply_status != HW_AUDIO_TRACE_OK)
        {
            fprintf(stderr,
                    "Trace line %u cannot be replayed: %s\n",
                    error_line,
                    hw_audio_trace_status_string(replay.apply_status));
        }
        else if (replay.output_failed)
        {
            fprintf(stderr, "Could not write native capture: %s\n", strerror(errno));
        }
        else if (error_line)
        {
            fprintf(stderr, "Invalid trace line %u: %s\n", error_line, hw_audio_trace_text_status_string(parse_status));
        }
        else
        {
            fprintf(stderr, "Invalid trace: %s\n", hw_audio_trace_text_status_string(parse_status));
        }
    }
    if (ok && replay.fifo_sample_index != replay.fifo_sample_count)
    {
        fprintf(stderr, "Trace replay did not consume every scheduled DirectSound sample\n");
        ok = false;
    }
    if (ok && replay.frame_count == 0)
    {
        fprintf(stderr, "Trace requires at least one captured SAMPLE\n");
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
    free(fifo_samples);
    free(collected.events);

    if (ok)
        ok = write_manifest(
            manifest_temp, replay.frame_count, replay.first_cycle, replay.last_cycle, options->solo_mask);
    return ok;
}

typedef struct
{
    HwAudioTraceTextRecord* records;
    size_t count;
    size_t capacity;
    uint64_t retained_cycle;
    uint32_t retained_order;
    bool measurement_open;
    bool retained_position_valid;
    bool allocation_failed;
    bool order_overflow;
} DriverTraceRecords;

/* Retain interval markers and hardware bus events while deliberately omitting SAMPLE. */
static bool collect_driver_record(void* context, const HwAudioTraceTextRecord* record)
{
    DriverTraceRecords* collected = context;
    bool include = record->kind == HW_AUDIO_TRACE_TEXT_BEGIN || record->kind == HW_AUDIO_TRACE_TEXT_END ||
                   (collected->measurement_open && record->event.kind != HW_AUDIO_TRACE_SAMPLE);
    if (include)
    {
        HwAudioTraceTextRecord retained = *record;
        if (!collected->retained_position_valid || record->cycle > collected->retained_cycle)
        {
            collected->retained_cycle = record->cycle;
            collected->retained_order = 0u;
            collected->retained_position_valid = true;
        }
        else if (record->cycle == collected->retained_cycle)
        {
            if (collected->retained_order == UINT32_MAX)
            {
                collected->order_overflow = true;
                return false;
            }
            collected->retained_order++;
        }
        else
        {
            collected->order_overflow = true;
            return false;
        }
        retained.order = collected->retained_order;
        if (collected->count == collected->capacity)
        {
            size_t capacity = collected->capacity ? collected->capacity * 2u : 256u;
            if (capacity < collected->capacity || capacity > SIZE_MAX / sizeof(*collected->records))
            {
                collected->allocation_failed = true;
                return false;
            }
            HwAudioTraceTextRecord* records =
                (HwAudioTraceTextRecord*)realloc(collected->records, capacity * sizeof(*collected->records));
            if (!records)
            {
                collected->allocation_failed = true;
                return false;
            }
            collected->records = records;
            collected->capacity = capacity;
        }
        collected->records[collected->count++] = retained;
    }
    if (record->kind == HW_AUDIO_TRACE_TEXT_BEGIN)
        collected->measurement_open = true;
    else if (record->kind == HW_AUDIO_TRACE_TEXT_END)
        collected->measurement_open = false;
    return true;
}

/* Use the same trace reader as replay before the driver-equivalence gate. */
static bool read_driver_trace(const char* path, DriverTraceRecords* records)
{
    FILE* input = fopen(path, "rb");
    if (!input)
    {
        fprintf(stderr, "Could not open driver trace '%s': %s\n", path, strerror(errno));
        return false;
    }
    unsigned error_line = 0;
    HwAudioTraceTextStatus status = hw_audio_trace_text_read(input, collect_driver_record, records, &error_line);
    bool ok = fclose(input) == 0 && status == HW_AUDIO_TRACE_TEXT_OK;
    if (!ok)
    {
        if (records->allocation_failed)
            fprintf(stderr, "Could not allocate driver trace comparison records\n");
        else if (records->order_overflow)
            fprintf(stderr, "Driver trace has too many retained same-cycle hardware records\n");
        else if (error_line)
            fprintf(stderr,
                    "Invalid driver trace '%s' line %u: %s\n",
                    path,
                    error_line,
                    hw_audio_trace_text_status_string(status));
        else
            fprintf(stderr, "Invalid driver trace '%s': %s\n", path, hw_audio_trace_text_status_string(status));
    }
    return ok;
}

static const char* trace_record_name(const HwAudioTraceTextRecord* record)
{
    if (record->kind == HW_AUDIO_TRACE_TEXT_BEGIN)
        return "BEGIN";
    if (record->kind == HW_AUDIO_TRACE_TEXT_END)
        return "END";
    if (record->event.kind == HW_AUDIO_TRACE_WRITE)
        return "WRITE";
    if (record->event.kind == HW_AUDIO_TRACE_TIMER)
        return "TIMER";
    return "SAMPLE";
}

static bool compare_position(const char* name,
                             size_t index,
                             const HwAudioTraceTextRecord* oracle,
                             const HwAudioTraceTextRecord* driver)
{
    if (oracle->cycle != driver->cycle)
    {
        fprintf(stderr,
                "driver-event compare: first divergence at record %zu %s cycle: oracle=%" PRIu64 " driver=%" PRIu64
                "\n",
                index + 1u,
                name,
                oracle->cycle,
                driver->cycle);
        return false;
    }
    if (oracle->order != driver->order)
    {
        fprintf(stderr,
                "driver-event compare: first divergence at record %zu %s order: oracle=%" PRIu32 " driver=%" PRIu32
                "\n",
                index + 1u,
                name,
                oracle->order,
                driver->order);
        return false;
    }
    return true;
}

/* Compare interval markers and every normalized hardware event, including
 * DirectSound FIFO writes and timer edges; SAMPLE observations are excluded. */
static bool compare_driver_traces(const char* oracle_path, const char* driver_path)
{
    DriverTraceRecords oracle = {0};
    DriverTraceRecords driver = {0};
    bool ok = read_driver_trace(oracle_path, &oracle) && read_driver_trace(driver_path, &driver);
    if (!ok)
    {
        free(oracle.records);
        free(driver.records);
        return false;
    }

    size_t shared = oracle.count < driver.count ? oracle.count : driver.count;
    for (size_t index = 0; index < shared; index++)
    {
        const HwAudioTraceTextRecord* expected = &oracle.records[index];
        const HwAudioTraceTextRecord* actual = &driver.records[index];
        if (expected->kind != actual->kind)
        {
            fprintf(stderr,
                    "driver-event compare: first divergence at record %zu kind: oracle=%s driver=%s\n",
                    index + 1u,
                    trace_record_name(expected),
                    trace_record_name(actual));
            ok = false;
            break;
        }
        if (expected->kind == HW_AUDIO_TRACE_TEXT_EVENT && expected->event.kind != actual->event.kind)
        {
            fprintf(stderr,
                    "driver-event compare: first divergence at record %zu event kind: oracle=%s driver=%s\n",
                    index + 1u,
                    trace_record_name(expected),
                    trace_record_name(actual));
            ok = false;
            break;
        }
        if (!compare_position(trace_record_name(expected), index, expected, actual))
        {
            ok = false;
            break;
        }
        if (expected->kind != HW_AUDIO_TRACE_TEXT_EVENT)
            continue;
        if (expected->event.kind == HW_AUDIO_TRACE_WRITE)
        {
            if (expected->event.address != actual->event.address)
            {
                fprintf(stderr,
                        "driver-event compare: first divergence at record %zu WRITE address: oracle=0x%08" PRIX32
                        " driver=0x%08" PRIX32 "\n",
                        index + 1u,
                        expected->event.address,
                        actual->event.address);
                ok = false;
                break;
            }
            if (expected->event.width != actual->event.width)
            {
                fprintf(stderr,
                        "driver-event compare: first divergence at record %zu WRITE width: oracle=%u driver=%u\n",
                        index + 1u,
                        (unsigned)expected->event.width,
                        (unsigned)actual->event.width);
                ok = false;
                break;
            }
        }
        if (expected->event.value != actual->event.value)
        {
            fprintf(stderr,
                    "driver-event compare: first divergence at record %zu %s value: oracle=0x%08" PRIX32
                    " driver=0x%08" PRIX32 "\n",
                    index + 1u,
                    trace_record_name(expected),
                    expected->event.value,
                    actual->event.value);
            ok = false;
            break;
        }
    }
    if (ok && oracle.count != driver.count)
    {
        const bool oracle_extra = oracle.count > driver.count;
        const HwAudioTraceTextRecord* extra = oracle_extra ? &oracle.records[shared] : &driver.records[shared];
        const char* source = oracle_extra ? "oracle" : "driver";
        fprintf(stderr,
                "driver-event compare: first divergence at record %zu: %s has %s after the other trace ends\n",
                shared + 1u,
                source,
                trace_record_name(extra));
        ok = false;
    }
    if (ok)
    {
        size_t hardware_events = oracle.count >= 2u ? oracle.count - 2u : 0u;
        printf("driver-event compare: exact match (%zu hardware events: register/FIFO writes and timer edges; SAMPLE "
               "excluded)\n",
               hardware_events);
    }
    free(oracle.records);
    free(driver.records);
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
    if (options.compare_driver)
        return compare_driver_traces(options.oracle_path, options.driver_path) ? 0 : 1;

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
