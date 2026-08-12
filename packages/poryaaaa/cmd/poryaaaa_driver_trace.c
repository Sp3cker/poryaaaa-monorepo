/*
 * poryaaaa_driver_trace - candidate adapter for the driver lifecycle harness
 *
 * Usage: poryaaaa_driver_trace PROJECT_ROOT VOICEGROUP VOICE_INDEX
 *            --scenario NAME --trace-output FILE
 *            [--note N] [--velocity N] [--volume N] [--pan N]
 *
 * Loads one supported voice slot through voicegroup_loader and derives its
 * family from ToneData. It drives M4ADriver directly (no MIDI parser,
 * renderer, or chip), then serializes only that family's unmodified pending
 * bus events through m4a_driver_trace. The deterministic companion manifest
 * (FILE.manifest.json) proves fixture identity for driver_compare /
 * validate_driver.
 *
 * The trace goes to FILE; the manifest is appended as FILE.manifest.json so
 * the publisher never collides with an arbitrary suffix.  Exit 2 means the
 * command line was rejected, 1 a capture/publication failure, and 0 only
 * after the complete pair (trace + manifest) is committed.
 */

#if !defined(_WIN32) && !defined(_POSIX_C_SOURCE)
#    define _POSIX_C_SOURCE 200809L
#endif

#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if defined(_WIN32)
#    include <windows.h>
#    include <io.h>
#    include <process.h>
#    include <sys/stat.h>
#else
#    include <sys/types.h>
#    include <sys/stat.h>
#    include <unistd.h>
#endif

#if defined(_WIN32) && !defined(ESTALE)
#    define ESTALE EIO
#endif

#include "m4a/m4a_driver.h"
#include "m4a/m4a_driver_trace.h"
#include "voicegroup/voicegroup_loader.h"
#include "voicegroup/voicegroup_types.h"

/* Every family shares one observation span. The seven-frame settle tail exposes
 * DirectSound only after MP2K's zeroed DMA ring has made a complete pass. */
#define DRIVER_START_CAPTURE_FRAMES 9u
#define DRIVER_ENVELOPE_CAPTURE_FRAMES 14u
#define DRIVER_PITCH_CAPTURE_FRAMES 12u
#define DRIVER_VOLUME_PAN_CAPTURE_FRAMES 12u
#define DRIVER_RETRIGGER_CAPTURE_FRAMES 13u
#define DRIVER_RELEASE_CAPTURE_FRAMES 14u
#define DRIVER_CAPTURE_END_CYCLE(frames)                                                                               \
    ((((uint64_t)(frames) * M4A_VBLANK_CYCLES + 384u + 511u) & ~UINT64_C(511)) + UINT64_C(512))
#define DRIVER_HOST_RATE_HZ 44100u
#define DRIVER_WAVEFORM_SIZE 16u
#define DRIVER_NORMALIZED_TONE_SIZE 12u
#define DRIVER_SQUARE_PAYLOAD_SIZE 6u
#define DRIVER_DEFAULT_NOTE 60u
#define DRIVER_DEFAULT_VELOCITY 127u
#define DRIVER_DEFAULT_VOLUME 127u
#define DRIVER_DEFAULT_PAN 64u
/* The reference BEGIN is the next logical native sample, while Timer 0 is
 * resumed at mGBA's current cycle 1005 cycles later. Preserve that measured
 * capture phase in the CPU-free DirectSound adapter so END sees the same
 * complete timer intervals. PSG families have no retained timer projection. */
#define DRIVER_DIRECTSOUND_ORIGIN_CYCLES 1005u

typedef enum
{
    DRIVER_FAMILY_PSW,
    DRIVER_FAMILY_SQ1,
    DRIVER_FAMILY_SQ2,
    DRIVER_FAMILY_DIRECTSOUND,
} DriverFamilyId;

typedef struct
{
    DriverFamilyId id;
    const char* name;
    uint8_t accepted_types[3];
    size_t accepted_type_count;
    uint64_t sample_period_cycles;
    uint64_t driver_origin_cycle;
} DriverFamily;

static const DriverFamily DRIVER_FAMILIES[] = {
    {DRIVER_FAMILY_PSW, "psw", {VOICE_PROGRAMMABLE_WAVE, VOICE_PROGRAMMABLE_WAVE_ALT}, 2u, 512u, 0u},
    {DRIVER_FAMILY_SQ1, "sq1", {VOICE_SQUARE_1}, 1u, 512u, 0u},
    {DRIVER_FAMILY_SQ2, "sq2", {VOICE_SQUARE_2}, 1u, 512u, 0u},
    {DRIVER_FAMILY_DIRECTSOUND, "directsound", {VOICE_DIRECTSOUND}, 1u, 256u, DRIVER_DIRECTSOUND_ORIGIN_CYCLES},
};

/* Classify the resolved ToneData type once so every later choice is
 * family-scoped and unsupported instruments fail before publication. */
static const DriverFamily* driver_family_for_type(uint8_t type)
{
    for (size_t family_index = 0u; family_index < sizeof(DRIVER_FAMILIES) / sizeof(DRIVER_FAMILIES[0]); ++family_index)
    {
        const DriverFamily* family = &DRIVER_FAMILIES[family_index];
        for (size_t type_index = 0u; type_index < family->accepted_type_count; ++type_index)
        {
            if (family->accepted_types[type_index] == type)
                return family;
        }
    }
    return NULL;
}

typedef enum
{
    DRIVER_SCENARIO_START,
    DRIVER_SCENARIO_ENVELOPE,
    DRIVER_SCENARIO_PITCH,
    DRIVER_SCENARIO_VOLUME_PAN,
    DRIVER_SCENARIO_RETRIGGER,
    DRIVER_SCENARIO_RELEASE,
} DriverScenarioId;

typedef struct
{
    DriverScenarioId id;
    const char* name;
    unsigned logical_vblanks;
    unsigned capture_frames;
    uint64_t end_cycle;
    const char* high_level_action;
} DriverScenario;

static const DriverScenario DRIVER_SCENARIOS[] = {
    {DRIVER_SCENARIO_START,
     "start",
     1u,
     DRIVER_START_CAPTURE_FRAMES,
     DRIVER_CAPTURE_END_CYCLE(DRIVER_START_CAPTURE_FRAMES),
     "note-on at tick 0"},
    {DRIVER_SCENARIO_ENVELOPE,
     "envelope",
     6u,
     DRIVER_ENVELOPE_CAPTURE_FRAMES,
     DRIVER_CAPTURE_END_CYCLE(DRIVER_ENVELOPE_CAPTURE_FRAMES),
     "note-on at tick 0; sustain through tick 6"},
    {DRIVER_SCENARIO_PITCH,
     "pitch",
     4u,
     DRIVER_PITCH_CAPTURE_FRAMES,
     DRIVER_CAPTURE_END_CYCLE(DRIVER_PITCH_CAPTURE_FRAMES),
     "note-on at tick 0; pitch bend +16 at tick 2; sustain through tick 4"},
    {DRIVER_SCENARIO_VOLUME_PAN,
     "volume-pan",
     4u,
     DRIVER_VOLUME_PAN_CAPTURE_FRAMES,
     DRIVER_CAPTURE_END_CYCLE(DRIVER_VOLUME_PAN_CAPTURE_FRAMES),
     "note-on at tick 0; volume 32 and pan 127 at tick 2; sustain through tick 4"},
    {DRIVER_SCENARIO_RETRIGGER,
     "retrigger",
     5u,
     DRIVER_RETRIGGER_CAPTURE_FRAMES,
     DRIVER_CAPTURE_END_CYCLE(DRIVER_RETRIGGER_CAPTURE_FRAMES),
     "note-on at tick 0; note-off at tick 2; note-on at tick 3; sustain through tick 5"},
    {DRIVER_SCENARIO_RELEASE,
     "release",
     6u,
     DRIVER_RELEASE_CAPTURE_FRAMES,
     DRIVER_CAPTURE_END_CYCLE(DRIVER_RELEASE_CAPTURE_FRAMES),
     "note-on at tick 0; note-off at tick 2; release through tick 6"},
};

/* The final capture tick observes the settled state without a new action. */
static bool driver_is_scenario_action_tick(const DriverScenario* scenario, unsigned tick)
{
    return tick < scenario->logical_vblanks;
}

typedef struct
{
    const char* project_root;
    const char* voicegroup;
    const char* voice_index_text;
    const char* scenario;
    const char* trace_output;
    uint8_t note;
    uint8_t velocity;
    uint8_t volume;
    uint8_t pan;
} Options;

/* This narrow control seam makes the fixed schedule behavior-testable without
 * coupling its assertions to any generated register transaction sequence. */
typedef struct
{
    void* context;
    void (*note_on)(void* context, uint8_t note, uint8_t velocity);
    void (*note_off)(void* context, uint8_t note);
    void (*pitch_bend)(void* context, int16_t bend);
    void (*cc)(void* context, uint8_t cc, uint8_t value);
} DriverScenarioControls;

/* Bind the family-independent schedule to M4ADriver's public controls. */

static void driver_m4a_note_on(void* context, uint8_t note, uint8_t velocity)
{
    m4a_note_on((M4ADriver*)context, 0, note, velocity);
}

static void driver_m4a_note_off(void* context, uint8_t note)
{
    m4a_note_off((M4ADriver*)context, 0, note);
}

static void driver_m4a_pitch_bend(void* context, int16_t bend)
{
    m4a_pitch_bend((M4ADriver*)context, 0, bend);
}

static void driver_m4a_cc(void* context, uint8_t cc, uint8_t value)
{
    m4a_cc((M4ADriver*)context, 0, cc, value);
}

/* Dispatch the fixed lifecycle schedule without encoding family bus output. */

static void driver_apply_scenario_actions(const DriverScenario* scenario,
                                          const Options* options,
                                          const DriverScenarioControls* controls,
                                          unsigned tick)
{
    if (tick == 0u)
    {
        controls->cc(controls->context, 0x07u, options->volume);
        controls->cc(controls->context, 0x0Au, options->pan);
        controls->note_on(controls->context, options->note, options->velocity);
    }

    switch (scenario->id)
    {
    case DRIVER_SCENARIO_PITCH:
        if (tick == 2u)
        {
            /* M4ADriver converts +2048 to MP2K's canonical BEND +16. */
            controls->pitch_bend(controls->context, 2048);
        }
        break;
    case DRIVER_SCENARIO_VOLUME_PAN:
        if (tick == 2u)
        {
            controls->cc(controls->context, 0x07u, 32u);
            controls->cc(controls->context, 0x0Au, 127u);
        }
        break;
    case DRIVER_SCENARIO_RETRIGGER:
        if (tick == 2u)
            controls->note_off(controls->context, options->note);
        else if (tick == 3u)
            controls->note_on(controls->context, options->note, options->velocity);
        break;
    case DRIVER_SCENARIO_RELEASE:
        if (tick == 2u)
            controls->note_off(controls->context, options->note);
        break;
    case DRIVER_SCENARIO_START:
    case DRIVER_SCENARIO_ENVELOPE:
        break;
    }
}

/* Resolve only the fixed public scenario names accepted by the harness. */

static const DriverScenario* driver_find_scenario(const char* name)
{
    for (size_t index = 0u; index < sizeof(DRIVER_SCENARIOS) / sizeof(DRIVER_SCENARIOS[0]); ++index)
    {
        if (strcmp(name, DRIVER_SCENARIOS[index].name) == 0)
            return &DRIVER_SCENARIOS[index];
    }
    return NULL;
}

/* Project only the selected family's real driver events. The projection never
 * rewrites an address, width, value, cycle, or same-cycle order. */
static bool driver_retain_register(const DriverFamily* family, M4ARegId reg)
{
    switch (family->id)
    {
    case DRIVER_FAMILY_PSW:
        switch (reg)
        {
        case M4A_REG_NR30:
        case M4A_REG_NR31:
        case M4A_REG_NR32:
        case M4A_REG_NR33:
        case M4A_REG_NR34:
        case M4A_REG_NR50:
        case M4A_REG_NR51:
        case M4A_REG_WAVE_RAM_BYTE:
        case M4A_REG_WAVE_RAM_WORD_0:
        case M4A_REG_WAVE_RAM_WORD_1:
        case M4A_REG_WAVE_RAM_WORD_2:
        case M4A_REG_WAVE_RAM_WORD_3:
            return true;
        default:
            return false;
        }
    case DRIVER_FAMILY_SQ1:
        switch (reg)
        {
        case M4A_REG_NR10:
        case M4A_REG_NR11:
        case M4A_REG_NR12:
        case M4A_REG_NR13:
        case M4A_REG_NR14:
        case M4A_REG_NR50:
        case M4A_REG_NR51:
            return true;
        default:
            return false;
        }
    case DRIVER_FAMILY_SQ2:
        switch (reg)
        {
        case M4A_REG_NR21:
        case M4A_REG_NR22:
        case M4A_REG_NR23:
        case M4A_REG_NR24:
        case M4A_REG_NR50:
        case M4A_REG_NR51:
            return true;
        default:
            return false;
        }
    case DRIVER_FAMILY_DIRECTSOUND:
        switch (reg)
        {
        case M4A_REG_SOUNDCNT_H:
        case M4A_REG_SOUNDBIAS:
        case M4A_REG_FIFO_A:
        case M4A_REG_FIFO_B:
        case M4A_REG_TIMER_0:
        case M4A_REG_TIMER_1:
            return true;
        default:
            return false;
        }
    }
    return false;
}

typedef struct
{
    M4ADriverTraceWriter* writer;
    const DriverFamily* family;
    uint64_t end_cycle;
    uint64_t next_sample_cycle;
} DriverTraceMerge;

/* Serialize native SAMPLE observations at the selected hardware family's
 * real reset cadence, preserving their cycle positions between driver events. */
static bool driver_write_next_sample(DriverTraceMerge* trace)
{
    const uint32_t order =
        trace->next_sample_cycle == trace->writer->previous_cycle ? trace->writer->previous_order + 1u : 0u;
    if (!m4a_driver_trace_write_sample(trace->writer, trace->next_sample_cycle, order))
        return false;
    trace->next_sample_cycle += trace->family->sample_period_cycles;
    return true;
}

/* Serialize borrowed pending events directly. Grouping by source cycle keeps
 * SAMPLE ordering exact without allocating a rewritten event batch. */
static bool driver_write_merged_batch(DriverTraceMerge* trace, const M4ARegWriteBatch* batch)
{
    if (!batch)
        return false;
    size_t index = 0u;
    while (index < batch->count)
    {
        const uint64_t cycle = batch->events[index].cycle;
        while (trace->next_sample_cycle < trace->end_cycle && trace->next_sample_cycle < cycle)
        {
            if (!driver_write_next_sample(trace))
                return false;
        }

        size_t group_end = index;
        while (group_end < batch->count && batch->events[group_end].cycle == cycle)
            ++group_end;
        for (size_t event_index = index; event_index < group_end; ++event_index)
        {
            if (!driver_retain_register(trace->family, batch->events[event_index].reg))
                continue;
            const M4ARegWriteBatch event = {
                .events = &batch->events[event_index],
                .count = 1u,
            };
            if (!m4a_driver_trace_write_batch(trace->writer, &event))
                return false;
        }
        index = group_end;

        if (trace->next_sample_cycle < trace->end_cycle && trace->next_sample_cycle == cycle &&
            !driver_write_next_sample(trace))
        {
            return false;
        }
    }
    return true;
}

/* Fill the final observation tail after the last pending event batch. */
static bool driver_finish_merged_trace(DriverTraceMerge* trace)
{
    while (trace->next_sample_cycle < trace->end_cycle)
    {
        if (!driver_write_next_sample(trace))
            return false;
    }
    return true;
}

static void print_usage(const char* program)
{
    fprintf(stderr,
            "Usage: %s PROJECT_ROOT VOICEGROUP VOICE_INDEX\n"
            "       --scenario NAME --trace-output FILE\n"
            "       [--note N] [--velocity N] [--volume N] [--pan N]\n"
            "VOICE_INDEX and every control are strict decimal 0..127 "
            "(defaults note 60, velocity 127, volume 127, pan 64).\n",
            program);
}

/* Parse one strict decimal 0..127 byte (control value or zero-based slot). */
static bool parse_control_byte(const char* text, uint8_t* value)
{
    if (!text || !*text || *text == '-' || *text == '+')
        return false;
    char* end = NULL;
    errno = 0;
    long parsed = strtol(text, &end, 10);
    if (errno != 0 || end == text || *end != '\0' || parsed < 0 || parsed > 127)
        return false;
    *value = (uint8_t)parsed;
    return true;
}

/* Reject incomplete, ambiguous, unknown, or duplicated command lines. */
static bool parse_options(int argc, char** argv, Options* options)
{
    memset(options, 0, sizeof(*options));
    options->note = DRIVER_DEFAULT_NOTE;
    options->velocity = DRIVER_DEFAULT_VELOCITY;
    options->volume = DRIVER_DEFAULT_VOLUME;
    options->pan = DRIVER_DEFAULT_PAN;

    int positional = 0;
    bool seen_scenario = false;
    bool seen_trace_output = false;
    bool seen_note = false;
    bool seen_velocity = false;
    bool seen_volume = false;
    bool seen_pan = false;

    for (int index = 1; index < argc; ++index)
    {
        const char* arg = argv[index];
        if (arg[0] == '-' && arg[1] != '\0')
        {
            if (strcmp(arg, "--scenario") == 0)
            {
                if (seen_scenario || index + 1 >= argc)
                    return false;
                seen_scenario = true;
                options->scenario = argv[++index];
            }
            else if (strcmp(arg, "--trace-output") == 0)
            {
                if (seen_trace_output || index + 1 >= argc)
                    return false;
                seen_trace_output = true;
                options->trace_output = argv[++index];
            }
            else if (strcmp(arg, "--note") == 0)
            {
                if (seen_note || index + 1 >= argc || !parse_control_byte(argv[index + 1], &options->note))
                    return false;
                seen_note = true;
                ++index;
            }
            else if (strcmp(arg, "--velocity") == 0)
            {
                if (seen_velocity || index + 1 >= argc || !parse_control_byte(argv[index + 1], &options->velocity))
                    return false;
                seen_velocity = true;
                ++index;
            }
            else if (strcmp(arg, "--volume") == 0)
            {
                if (seen_volume || index + 1 >= argc || !parse_control_byte(argv[index + 1], &options->volume))
                    return false;
                seen_volume = true;
                ++index;
            }
            else if (strcmp(arg, "--pan") == 0)
            {
                if (seen_pan || index + 1 >= argc || !parse_control_byte(argv[index + 1], &options->pan))
                    return false;
                seen_pan = true;
                ++index;
            }
            else
            {
                return false;
            }
        }
        else if (positional < 3)
        {
            if (positional == 0)
                options->project_root = arg;
            else if (positional == 1)
            {
                if (!arg[0])
                    return false;
                options->voicegroup = arg;
            }
            else
                options->voice_index_text = arg;
            ++positional;
        }
        else
        {
            return false;
        }
    }
    if (positional != 3 || !options->scenario || !options->trace_output || !driver_find_scenario(options->scenario))
        return false;
    return true;
}

/* Derive the loader bank name and the canonical manifest symbol from one
 * VOICEGROUP token.  The loader wants the bare project bank name
 * ('b_factory'); the candidate manifest must record the canonical ELF symbol
 * ('voicegroup_b_factory') exactly like the record_voice.sh/reference
 * manifests.  Either spelling is accepted; input is already known non-empty.
 * *bank_out may alias input; when the bare bank form is given, *symbol_out is
 * freshly allocated (owned by *owned_out, else NULL).  Returns false on an
 * empty derived bank name or length overflow. */
static bool canonicalize_voicegroup(const char* input, const char** bank_out, const char** symbol_out, char** owned_out)
{
    static const char prefix[] = "voicegroup_";
    const size_t prefix_length = sizeof(prefix) - 1u;
    char* owned = NULL;
    const char* bank;
    const char* symbol;

    if (strncmp(input, prefix, prefix_length) == 0)
    {
        /* Canonical symbol already given: strip the prefix for the loader. */
        bank = input + prefix_length;
        symbol = input;
    }
    else
    {
        /* Bare bank name: load it as-is, synthesize the canonical symbol. */
        bank = input;
        size_t name_length = strlen(input);
        if (name_length > SIZE_MAX - prefix_length - 1u)
            return false;
        owned = (char*)malloc(prefix_length + name_length + 1u);
        if (!owned)
            return false;
        memcpy(owned, prefix, prefix_length);
        memcpy(owned + prefix_length, input, name_length + 1u);
        symbol = owned;
    }

    if (!*bank)
    {
        free(owned);
        return false;
    }
    *bank_out = bank;
    *symbol_out = symbol;
    *owned_out = owned;
    return true;
}

/* Allocate a sibling path without imposing a platform PATH_MAX. */
static char* path_with_suffix(const char* prefix, const char* suffix)
{
    size_t prefix_length = strlen(prefix);
    size_t suffix_length = strlen(suffix);
    if (prefix_length > SIZE_MAX - suffix_length - 1u)
        return NULL;
    char* path = (char*)malloc(prefix_length + suffix_length + 1u);
    if (!path)
        return NULL;
    memcpy(path, prefix, prefix_length);
    memcpy(path + prefix_length, suffix, suffix_length + 1u);
    return path;
}

typedef struct
{
#if defined(_WIN32)
    DWORD volume_serial_number;
    DWORD file_index_high;
    DWORD file_index_low;
#else
    dev_t device;
    ino_t inode;
#endif
} DriverFileIdentity;

typedef struct
{
    char* manifest_path;
    char* trace_temp;
    char* manifest_temp;
    FILE* trace_file;
    FILE* manifest_file;
    bool trace_published;
    bool manifest_published;
} DriverOutputStage;
typedef enum
{
    DRIVER_OUTPUT_PAIR_EMPTY,
    DRIVER_OUTPUT_PAIR_COMPLETE,
    DRIVER_OUTPUT_PAIR_TRACE_ORPHAN,
    DRIVER_OUTPUT_PAIR_MANIFEST_ORPHAN,
} DriverOutputPairState;

static int driver_close_descriptor(int descriptor)
{
#if defined(_WIN32)
    return _close(descriptor);
#else
    return close(descriptor);
#endif
}

static FILE* driver_fdopen_binary_update(int descriptor)
{
#if defined(_WIN32)
    return _fdopen(descriptor, "w+b");
#else
    return fdopen(descriptor, "w+b");
#endif
}

#if defined(_WIN32)
/* Preserve useful errno contracts while reporting Win32 publication failures. */
static void driver_set_windows_errno(DWORD error)
{
    if (error == ERROR_FILE_EXISTS || error == ERROR_ALREADY_EXISTS)
        errno = EEXIST;
    else if (error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND)
        errno = ENOENT;
    else if (error == ERROR_ACCESS_DENIED || error == ERROR_SHARING_VIOLATION)
        errno = EACCES;
    else
        errno = EIO;
}

/* Recover the native handle so Windows can commit or delete this exact stream. */
static HANDLE driver_stream_handle(FILE* input)
{
    if (!input)
    {
        errno = EINVAL;
        return INVALID_HANDLE_VALUE;
    }
    const int descriptor = _fileno(input);
    const intptr_t raw_handle = descriptor < 0 ? -1 : _get_osfhandle(descriptor);
    if (raw_handle == -1)
    {
        errno = EBADF;
        return INVALID_HANDLE_VALUE;
    }
    return (HANDLE)raw_handle;
}

/* Rename the open stream itself, refusing to replace an existing final name. */
static bool driver_rename_stream_noreplace(FILE* input, const char* final_path)
{
    const HANDLE handle = driver_stream_handle(input);
    if (handle == INVALID_HANDLE_VALUE)
        return false;
    const int wide_count = MultiByteToWideChar(CP_ACP, 0, final_path, -1, NULL, 0);
    if (wide_count <= 0)
    {
        driver_set_windows_errno(GetLastError());
        return false;
    }
    const size_t information_size = sizeof(FILE_RENAME_INFO) + ((size_t)wide_count - 1u) * sizeof(WCHAR);
    if (information_size > UINT32_MAX)
    {
        errno = ENOMEM;
        return false;
    }
    FILE_RENAME_INFO* information = (FILE_RENAME_INFO*)calloc(1u, information_size);
    if (!information)
        return false;
    information->ReplaceIfExists = FALSE;
    information->RootDirectory = NULL;
    information->FileNameLength = (DWORD)((size_t)(wide_count - 1) * sizeof(WCHAR));
    const bool converted = MultiByteToWideChar(CP_ACP, 0, final_path, -1, information->FileName, wide_count) != 0;
    const bool renamed =
        converted && SetFileInformationByHandle(handle, FileRenameInfo, information, (DWORD)information_size) != 0;
    const DWORD error = renamed ? ERROR_SUCCESS : GetLastError();
    free(information);
    if (!renamed)
    {
        driver_set_windows_errno(error);
        return false;
    }
    return true;
}

/* Mark the exact open stream for deletion without resolving a rollback path again. */
static bool driver_delete_stream(FILE* input)
{
    const HANDLE handle = driver_stream_handle(input);
    if (handle == INVALID_HANDLE_VALUE)
        return false;
    const FILE_DISPOSITION_INFO information = {.DeleteFile = TRUE};
    if (!SetFileInformationByHandle(handle, FileDispositionInfo, &information, sizeof(information)))
    {
        driver_set_windows_errno(GetLastError());
        return false;
    }
    return true;
}
#endif
/* Observe a directory entry without dereferencing it; any entry is a collision. */
static bool driver_path_entry_exists(const char* path, bool* exists_out)
{
    if (!path || !exists_out)
    {
        errno = EINVAL;
        return false;
    }
#if defined(_WIN32)
    if (GetFileAttributesA(path) != INVALID_FILE_ATTRIBUTES)
    {
        *exists_out = true;
        return true;
    }
    driver_set_windows_errno(GetLastError());
#else
    struct stat information;
    if (lstat(path, &information) == 0)
    {
        *exists_out = true;
        return true;
    }
#endif
    if (errno == ENOENT)
    {
        *exists_out = false;
        return true;
    }
    return false;
}

/* Classify the public sibling names without deleting or opening either entry. */
static bool
driver_inspect_output_pair(const char* trace_path, const char* manifest_path, DriverOutputPairState* state_out)
{
    if (!state_out)
    {
        errno = EINVAL;
        return false;
    }
    bool trace_exists = false;
    bool manifest_exists = false;
    if (!driver_path_entry_exists(trace_path, &trace_exists) ||
        !driver_path_entry_exists(manifest_path, &manifest_exists))
    {
        return false;
    }
    if (trace_exists && manifest_exists)
        *state_out = DRIVER_OUTPUT_PAIR_COMPLETE;
    else if (trace_exists)
        *state_out = DRIVER_OUTPUT_PAIR_TRACE_ORPHAN;
    else if (manifest_exists)
        *state_out = DRIVER_OUTPUT_PAIR_MANIFEST_ORPHAN;
    else
        *state_out = DRIVER_OUTPUT_PAIR_EMPTY;
    return true;
}

/* Reject every occupied public output state before staging. The later
 * no-replace publication remains required because this observation races. */
static bool
driver_require_empty_output_pair(const char* trace_path, const char* manifest_path, DriverOutputPairState* state_out)
{
    if (!driver_inspect_output_pair(trace_path, manifest_path, state_out))
        return false;
    if (*state_out == DRIVER_OUTPUT_PAIR_EMPTY)
        return true;
    errno = EEXIST;
    return false;
}

/* Capture the file object behind an open stream before path publication. */
static bool driver_stream_identity(FILE* input, DriverFileIdentity* identity)
{
    if (!input || !identity)
    {
        errno = EINVAL;
        return false;
    }
#if defined(_WIN32)
    const int descriptor = _fileno(input);
    const intptr_t raw_handle = descriptor < 0 ? -1 : _get_osfhandle(descriptor);
    if (raw_handle == -1)
    {
        errno = EBADF;
        return false;
    }
    BY_HANDLE_FILE_INFORMATION information;
    if (!GetFileInformationByHandle((HANDLE)raw_handle, &information))
    {
        driver_set_windows_errno(GetLastError());
        return false;
    }
    *identity = (DriverFileIdentity){
        .volume_serial_number = information.dwVolumeSerialNumber,
        .file_index_high = information.nFileIndexHigh,
        .file_index_low = information.nFileIndexLow,
    };
#else
    struct stat information;
    if (fstat(fileno(input), &information) != 0)
        return false;
    *identity = (DriverFileIdentity){.device = information.st_dev, .inode = information.st_ino};
#endif
    return true;
}

/* Reject a pathname unless it still names the exact staged file object. */
static bool driver_path_matches_identity(const char* path, const DriverFileIdentity* expected)
{
    if (!path || !expected)
    {
        errno = EINVAL;
        return false;
    }
#if defined(_WIN32)
    HANDLE handle = CreateFileA(path,
                                FILE_READ_ATTRIBUTES,
                                FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                                NULL,
                                OPEN_EXISTING,
                                FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT,
                                NULL);
    if (handle == INVALID_HANDLE_VALUE)
    {
        driver_set_windows_errno(GetLastError());
        return false;
    }
    BY_HANDLE_FILE_INFORMATION information;
    const bool queried = GetFileInformationByHandle(handle, &information) != 0;
    const DWORD error = queried ? ERROR_SUCCESS : GetLastError();
    CloseHandle(handle);
    if (!queried)
    {
        driver_set_windows_errno(error);
        return false;
    }
    if (information.dwVolumeSerialNumber != expected->volume_serial_number ||
        information.nFileIndexHigh != expected->file_index_high ||
        information.nFileIndexLow != expected->file_index_low)
    {
        errno = ESTALE;
        return false;
    }
#else
    struct stat information;
    if (lstat(path, &information) != 0)
        return false;
    if (information.st_dev != expected->device || information.st_ino != expected->inode)
    {
        errno = ESTALE;
        return false;
    }
#endif
    return true;
}

/* Bind a staged pathname to its open stream so later cleanup cannot target a replacement. */
static bool driver_stream_path_matches(FILE* input, const char* path)
{
    DriverFileIdentity identity;
    return driver_stream_identity(input, &identity) && driver_path_matches_identity(path, &identity);
}

/* Remove only the stream's own current directory entry after identity validation. */
static bool driver_remove_matching_stream_path(FILE* input, const char* path)
{
    if (!driver_stream_path_matches(input, path))
        return false;
#if defined(_WIN32)
    return driver_delete_stream(input);
#else
    return unlink(path) == 0;
#endif
}

/* Create an exclusive sibling staging file (mode 0600 on POSIX), never by
 * reopening a predictable path. The Windows nonce supplements O_EXCL so
 * concurrent publishers do not share a guessable staging name. */
static FILE* driver_open_unique_temp(const char* final_path, char** temp_path_out)
{
    *temp_path_out = NULL;
#if defined(_WIN32)
    const size_t final_length = strlen(final_path);
    if (final_length > SIZE_MAX - 64u)
    {
        errno = ENOMEM;
        return NULL;
    }
    char* path = (char*)malloc(final_length + 64u);
    if (!path)
        return NULL;

    for (unsigned attempt = 0u; attempt < 1024u; ++attempt)
    {
        unsigned high = 0u;
        unsigned low = 0u;
        if (rand_s(&high) != 0 || rand_s(&low) != 0)
        {
            free(path);
            errno = EIO;
            return NULL;
        }
        const int count = snprintf(path, final_length + 64u, "%s.tmp.%08X%08X", final_path, high, low);
        if (count < 0 || (size_t)count >= final_length + 64u)
        {
            free(path);
            errno = ENOMEM;
            return NULL;
        }
        HANDLE handle = CreateFileA(path,
                                    GENERIC_READ | GENERIC_WRITE | DELETE,
                                    FILE_SHARE_READ | FILE_SHARE_WRITE,
                                    NULL,
                                    CREATE_NEW,
                                    FILE_ATTRIBUTE_NORMAL,
                                    NULL);
        if (handle != INVALID_HANDLE_VALUE)
        {
            const int descriptor = _open_osfhandle((intptr_t)handle, _O_RDWR | _O_BINARY);
            if (descriptor >= 0)
            {
                FILE* output = driver_fdopen_binary_update(descriptor);
                if (output)
                {
                    *temp_path_out = path;
                    return output;
                }
                const int saved_errno = errno;
                driver_close_descriptor(descriptor);
                DeleteFileA(path);
                free(path);
                errno = saved_errno;
                return NULL;
            }
            const int saved_errno = errno;
            CloseHandle(handle);
            DeleteFileA(path);
            free(path);
            errno = saved_errno;
            return NULL;
        }
        const DWORD error = GetLastError();
        if (error != ERROR_FILE_EXISTS && error != ERROR_ALREADY_EXISTS)
        {
            driver_set_windows_errno(error);
            break;
        }
    }

    const int saved_errno = errno;
    free(path);
    errno = saved_errno;
    return NULL;
#else
    char* path = path_with_suffix(final_path, ".tmp.XXXXXX");
    if (!path)
    {
        errno = ENOMEM;
        return NULL;
    }
    const int descriptor = mkstemp(path);
    if (descriptor < 0)
    {
        const int saved_errno = errno;
        free(path);
        errno = saved_errno;
        return NULL;
    }
    FILE* output = driver_fdopen_binary_update(descriptor);
    if (output)
    {
        *temp_path_out = path;
        return output;
    }
    const int saved_errno = errno;
    driver_close_descriptor(descriptor);
    unlink(path);
    free(path);
    errno = saved_errno;
    return NULL;
#endif
}

/* Atomically create a final name from the verified staged stream without replacement.
 * Windows renames the open handle with ReplaceIfExists false; POSIX failure
 * to remove the temporary hardlink is a publication failure so rollback can
 * prove the final file identity. */
static bool
driver_publish_noreplace(FILE* staged_file, const char* temp_path, const char* final_path, bool* published_out)
{
    *published_out = false;
    if (fflush(staged_file) != 0 || !driver_stream_path_matches(staged_file, temp_path))
        return false;
#if defined(_WIN32)
    if (!driver_rename_stream_noreplace(staged_file, final_path))
        return false;
    *published_out = true;
    return driver_stream_path_matches(staged_file, final_path);
#else
    if (link(temp_path, final_path) != 0)
        return false;
    *published_out = true;
    if (!driver_stream_path_matches(staged_file, final_path))
        return false;
    return driver_remove_matching_stream_path(staged_file, temp_path);
#endif
}

/* Roll back only publication entries that still resolve to this process's streams. */
static bool driver_output_stage_cleanup(DriverOutputStage* stage, const char* trace_output)
{
    bool ok = true;
    if (stage->manifest_published && !driver_remove_matching_stream_path(stage->manifest_file, stage->manifest_path))
        ok = false;
    if (stage->trace_published && !driver_remove_matching_stream_path(stage->trace_file, trace_output))
        ok = false;
    if (stage->trace_temp && !driver_remove_matching_stream_path(stage->trace_file, stage->trace_temp))
        ok = false;
    if (stage->manifest_temp && !driver_remove_matching_stream_path(stage->manifest_file, stage->manifest_temp))
        ok = false;
    if (stage->trace_file && fclose(stage->trace_file) != 0)
        ok = false;
    if (stage->manifest_file && fclose(stage->manifest_file) != 0)
        ok = false;
    free(stage->manifest_path);
    free(stage->trace_temp);
    free(stage->manifest_temp);
    return ok;
}

/* ---- Minimal self-contained SHA-256 (FIPS 180-4) ----
 * Kept in this command because the trace/manifest hashes must not depend on
 * host tooling or a third-party library; mirrors the reference recorder. */
typedef struct
{
    uint32_t state[8];
    uint64_t bits;
    unsigned used;
    uint8_t block[64];
} DriverSha256;

static uint32_t driver_rotate_right(uint32_t value, unsigned amount)
{
    return (value >> amount) | (value << (32u - amount));
}

static void driver_sha256_transform(DriverSha256* sha)
{
    static const uint32_t constants[64] = {
        0x428A2F98u, 0x71374491u, 0xB5C0FBCFu, 0xE9B5DBA5u, 0x3956C25Bu, 0x59F111F1u, 0x923F82A4u, 0xAB1C5ED5u,
        0xD807AA98u, 0x12835B01u, 0x243185BEu, 0x550C7DC3u, 0x72BE5D74u, 0x80DEB1FEu, 0x9BDC06A7u, 0xC19BF174u,
        0xE49B69C1u, 0xEFBE4786u, 0x0FC19DC6u, 0x240CA1CCu, 0x2DE92C6Fu, 0x4A7484AAu, 0x5CB0A9DCu, 0x76F988DAu,
        0x983E5152u, 0xA831C66Du, 0xB00327C8u, 0xBF597FC7u, 0xC6E00BF3u, 0xD5A79147u, 0x06CA6351u, 0x14292967u,
        0x27B70A85u, 0x2E1B2138u, 0x4D2C6DFCu, 0x53380D13u, 0x650A7354u, 0x766A0ABBu, 0x81C2C92Eu, 0x92722C85u,
        0xA2BFE8A1u, 0xA81A664Bu, 0xC24B8B70u, 0xC76C51A3u, 0xD192E819u, 0xD6990624u, 0xF40E3585u, 0x106AA070u,
        0x19A4C116u, 0x1E376C08u, 0x2748774Cu, 0x34B0BCB5u, 0x391C0CB3u, 0x4ED8AA4Au, 0x5B9CCA4Fu, 0x682E6FF3u,
        0x748F82EEu, 0x78A5636Fu, 0x84C87814u, 0x8CC70208u, 0x90BEFFFAu, 0xA4506CEBu, 0xBEF9A3F7u, 0xC67178F2u,
    };
    uint32_t work[64];
    for (unsigned index = 0; index < 16u; ++index)
    {
        work[index] = ((uint32_t)sha->block[index * 4u] << 24u) | ((uint32_t)sha->block[index * 4u + 1u] << 16u) |
                      ((uint32_t)sha->block[index * 4u + 2u] << 8u) | (uint32_t)sha->block[index * 4u + 3u];
    }
    for (unsigned index = 16u; index < 64u; ++index)
    {
        uint32_t sigma0 = driver_rotate_right(work[index - 15u], 7u) ^ driver_rotate_right(work[index - 15u], 18u) ^
                          (work[index - 15u] >> 3u);
        uint32_t sigma1 = driver_rotate_right(work[index - 2u], 17u) ^ driver_rotate_right(work[index - 2u], 19u) ^
                          (work[index - 2u] >> 10u);
        work[index] = work[index - 16u] + sigma0 + work[index - 7u] + sigma1;
    }
    uint32_t a = sha->state[0];
    uint32_t b = sha->state[1];
    uint32_t c = sha->state[2];
    uint32_t d = sha->state[3];
    uint32_t e = sha->state[4];
    uint32_t f = sha->state[5];
    uint32_t g = sha->state[6];
    uint32_t h = sha->state[7];
    for (unsigned index = 0; index < 64u; ++index)
    {
        uint32_t big_sigma1 = driver_rotate_right(e, 6u) ^ driver_rotate_right(e, 11u) ^ driver_rotate_right(e, 25u);
        uint32_t choose = (e & f) ^ (~e & g);
        uint32_t temp1 = h + big_sigma1 + choose + constants[index] + work[index];
        uint32_t big_sigma0 = driver_rotate_right(a, 2u) ^ driver_rotate_right(a, 13u) ^ driver_rotate_right(a, 22u);
        uint32_t majority = (a & b) ^ (a & c) ^ (b & c);
        uint32_t temp2 = big_sigma0 + majority;
        h = g;
        g = f;
        f = e;
        e = d + temp1;
        d = c;
        c = b;
        b = a;
        a = temp1 + temp2;
    }
    sha->state[0] += a;
    sha->state[1] += b;
    sha->state[2] += c;
    sha->state[3] += d;
    sha->state[4] += e;
    sha->state[5] += f;
    sha->state[6] += g;
    sha->state[7] += h;
}

static void driver_sha256_init(DriverSha256* sha)
{
    sha->state[0] = 0x6A09E667u;
    sha->state[1] = 0xBB67AE85u;
    sha->state[2] = 0x3C6EF372u;
    sha->state[3] = 0xA54FF53Au;
    sha->state[4] = 0x510E527Fu;
    sha->state[5] = 0x9B05688Cu;
    sha->state[6] = 0x1F83D9ABu;
    sha->state[7] = 0x5BE0CD19u;
    sha->bits = 0;
    sha->used = 0;
}

static void driver_sha256_update(DriverSha256* sha, const uint8_t* data, size_t count)
{
    sha->bits += (uint64_t)count * 8u;
    while (count != 0u)
    {
        size_t take = 64u - sha->used;
        if (take > count)
            take = count;
        memcpy(sha->block + sha->used, data, take);
        sha->used += (unsigned)take;
        data += take;
        count -= take;
        if (sha->used == 64u)
        {
            driver_sha256_transform(sha);
            sha->used = 0u;
        }
    }
}

static void driver_sha256_finish(DriverSha256* sha, char output[65])
{
    static const char hex[] = "0123456789abcdef";
    uint64_t bits = sha->bits;
    sha->block[sha->used] = 0x80u;
    ++sha->used;
    if (sha->used > 56u)
    {
        memset(sha->block + sha->used, 0, 64u - sha->used);
        driver_sha256_transform(sha);
        sha->used = 0u;
    }
    memset(sha->block + sha->used, 0, 56u - sha->used);
    for (unsigned index = 0; index < 8u; ++index)
        sha->block[63u - index] = (uint8_t)(bits >> (index * 8u));
    driver_sha256_transform(sha);
    for (unsigned index = 0; index < 32u; ++index)
    {
        uint8_t byte = (uint8_t)(sha->state[index / 4u] >> (24u - (index % 4u) * 8u));
        output[index * 2u] = hex[byte >> 4u];
        output[index * 2u + 1u] = hex[byte & 0x0Fu];
    }
    output[64] = '\0';
}

static void driver_sha256_bytes(const uint8_t* data, size_t count, char output[65])
{
    DriverSha256 sha;
    driver_sha256_init(&sha);
    driver_sha256_update(&sha, data, count);
    driver_sha256_finish(&sha, output);
}

/* Hash the still-open temporary descriptor after flushing it; this never
 * reopens a pathname that could have been replaced after creation. */
static bool driver_sha256_open_stream(FILE* input, char output[65])
{
    if (fflush(input) != 0 || fseek(input, 0, SEEK_SET) != 0)
        return false;
    DriverSha256 sha;
    driver_sha256_init(&sha);
    uint8_t buffer[4096];
    size_t count;
    while ((count = fread(buffer, 1u, sizeof(buffer), input)) != 0u)
        driver_sha256_update(&sha, buffer, count);
    if (ferror(input) || fseek(input, 0, SEEK_END) != 0)
        return false;
    driver_sha256_finish(&sha, output);
    return true;
}

/* Writes one JSON string so no voicegroup symbol can break the manifest. */
static bool write_json_string(FILE* output, const char* text)
{
    if (fputc('"', output) == EOF)
        return false;
    for (const unsigned char* cursor = (const unsigned char*)text; *cursor != '\0'; ++cursor)
    {
        if (*cursor == '"' || *cursor == '\\')
        {
            if (fputc('\\', output) == EOF || fputc(*cursor, output) == EOF)
                return false;
        }
        else if (*cursor < 0x20u)
        {
            if (fprintf(output, "\\u%04X", (unsigned)*cursor) < 0)
                return false;
        }
        else if (fputc(*cursor, output) == EOF)
        {
            return false;
        }
    }
    return fputc('"', output) != EOF;
}

typedef struct
{
    const DriverFamily* family;
    uint8_t normalized_tone[DRIVER_NORMALIZED_TONE_SIZE];
    uint8_t* payload;
    size_t payload_size;
    const WaveData* directsound_wave;
    char tone_data_sha256[65];
    char family_payload_sha256[65];
} DriverFixtureIdentity;

/* Normalize the ABI-sized ToneData record without embedding either adapter's
 * pointer address; the pointer target is committed by the family payload. */
static void driver_normalize_tone_data(const ToneData* tone, uint8_t output[DRIVER_NORMALIZED_TONE_SIZE])
{
    output[0] = tone->type;
    output[1] = tone->key;
    output[2] = tone->length;
    output[3] = tone->panSweep;
    memset(output + 4u, 0, 4u);
    output[8] = tone->attack;
    output[9] = tone->decay;
    output[10] = tone->sustain;
    output[11] = tone->release;
}

/* Encode binary fixture metadata in the GBA's little-endian representation. */

static void driver_write_u16_le(uint8_t output[2], uint16_t value)
{
    output[0] = (uint8_t)value;
    output[1] = (uint8_t)(value >> 8u);
}

static void driver_write_u32_le(uint8_t output[4], uint32_t value)
{
    output[0] = (uint8_t)value;
    output[1] = (uint8_t)(value >> 8u);
    output[2] = (uint8_t)(value >> 16u);
    output[3] = (uint8_t)(value >> 24u);
}

/* Build identity from loader-owned data only. It proves an independent
 * fixture selection without turning a host pointer into reproducible data. */
static bool
driver_build_fixture_identity(const ToneData* tone, const DriverFamily* family, DriverFixtureIdentity* identity)
{
    memset(identity, 0, sizeof(*identity));
    identity->family = family;
    driver_normalize_tone_data(tone, identity->normalized_tone);
    driver_sha256_bytes(identity->normalized_tone, sizeof(identity->normalized_tone), identity->tone_data_sha256);

    switch (family->id)
    {
    case DRIVER_FAMILY_PSW:
        if (!tone->wavePointer)
            return false;
        identity->payload_size = DRIVER_WAVEFORM_SIZE;
        identity->payload = (uint8_t*)malloc(identity->payload_size);
        if (!identity->payload)
            return false;
        memcpy(identity->payload, tone->wavePointer, identity->payload_size);
        break;
    case DRIVER_FAMILY_SQ1:
    case DRIVER_FAMILY_SQ2:
        if ((uintptr_t)tone->wavePointer > UINT8_MAX)
            return false;
        identity->payload_size = DRIVER_SQUARE_PAYLOAD_SIZE;
        identity->payload = (uint8_t*)malloc(identity->payload_size);
        if (!identity->payload)
            return false;
        identity->payload[0] = tone->panSweep;
        identity->payload[1] = (uint8_t)(uintptr_t)tone->wavePointer;
        identity->payload[2] = tone->attack;
        identity->payload[3] = tone->decay;
        identity->payload[4] = tone->sustain;
        identity->payload[5] = tone->release;
        break;
    case DRIVER_FAMILY_DIRECTSOUND:
    {
        const WaveData* wave = tone->wav;
        if (!wave || !wave->data)
            return false;
        identity->payload_size = 16u + wave->size;
        identity->payload = (uint8_t*)malloc(identity->payload_size);
        if (!identity->payload)
            return false;
        driver_write_u16_le(identity->payload, wave->type);
        driver_write_u16_le(identity->payload + 2u, wave->status);
        driver_write_u32_le(identity->payload + 4u, wave->freq);
        driver_write_u32_le(identity->payload + 8u, wave->loopStart);
        driver_write_u32_le(identity->payload + 12u, wave->size);
        memcpy(identity->payload + 16u, wave->data, wave->size);
        identity->directsound_wave = wave;
        break;
    }
    }
    driver_sha256_bytes(identity->payload, identity->payload_size, identity->family_payload_sha256);
    return true;
}

/* Release the one owned family payload after manifest publication or failure. */

static void driver_fixture_identity_cleanup(DriverFixtureIdentity* identity)
{
    free(identity->payload);
    identity->payload = NULL;
}

/* Hash immutable decomp provenance before trace publication can commit a
 * candidate to a different ROM/ELF pairing. */
static bool driver_sha256_file(const char* path, char output[65])
{
    FILE* input = fopen(path, "rb");
    if (!input)
        return false;
    DriverSha256 sha;
    driver_sha256_init(&sha);
    uint8_t buffer[4096];
    size_t count;
    while ((count = fread(buffer, 1u, sizeof(buffer), input)) != 0u)
        driver_sha256_update(&sha, buffer, count);
    const bool read_ok = !ferror(input);
    const bool close_ok = fclose(input) == 0;
    if (!read_ok || !close_ok)
        return false;
    driver_sha256_finish(&sha, output);
    return true;
}

/* Hash the paired fixed decomp artifacts used to resolve the loaded fixture. */

static bool driver_hash_decomp_pair(const char* project_root, char rom_sha256[65], char elf_sha256[65])
{
    char* rom_path = path_with_suffix(project_root, "/pokeemerald-hearth.gba");
    char* elf_path = path_with_suffix(project_root, "/pokeemerald-hearth.elf");
    const bool ok =
        rom_path && elf_path && driver_sha256_file(rom_path, rom_sha256) && driver_sha256_file(elf_path, elf_sha256);
    free(rom_path);
    free(elf_path);
    return ok;
}

/* Write one JSON hexadecimal byte string without allocating a second copy of
 * large DirectSound payloads solely for diagnostics. */
static bool write_json_hex(FILE* output, const uint8_t* data, size_t size)
{
    static const char hex[] = "0123456789abcdef";
    if (fputc('"', output) == EOF)
        return false;
    for (size_t index = 0u; index < size; ++index)
    {
        if (fputc(hex[data[index] >> 4u], output) == EOF || fputc(hex[data[index] & 0x0Fu], output) == EOF)
            return false;
    }
    return fputc('"', output) != EOF;
}

/* Deterministic companion manifest: fixed field order, no paths, timestamps,
 * pointers, or self-hash. The trace hash commits the sibling's bytes. */
static bool write_manifest(FILE* output,
                           const Options* options,
                           const DriverScenario* scenario,
                           uint64_t trace_begin_cycle,
                           uint64_t trace_end_cycle,
                           const char* voicegroup_symbol,
                           uint8_t voice_index,
                           uint8_t resolved_type,
                           const DriverFixtureIdentity* identity,
                           const char rom_sha256[65],
                           const char elf_sha256[65],
                           const char trace_sha256[65])
{
    bool ok = fputs("{\n", output) >= 0 && fputs("  \"format\": ", output) >= 0 &&
              write_json_string(output, "poryaaaa-driver-candidate-trace") && fputs(",\n", output) >= 0 &&
              fputs("  \"version\": 1,\n", output) >= 0 && fputs("  \"source\": ", output) >= 0 &&
              write_json_string(output, "poryaaaa-driver") && fputs(",\n", output) >= 0 &&
              fputs("  \"trace_format\": ", output) >= 0 && write_json_string(output, "PORYAAAA_AUDIO_TRACE") &&
              fputs(",\n", output) >= 0 && fputs("  \"trace_version\": 1,\n", output) >= 0 &&
              fprintf(output, "  \"clock_hz\": %u,\n", (unsigned)M4A_GBA_CYCLES_PER_SECOND) > 0 &&
              fprintf(output, "  \"trace_begin_cycle\": %" PRIu64 ",\n", trace_begin_cycle) > 0 &&
              fprintf(output, "  \"trace_end_cycle\": %" PRIu64 ",\n", trace_end_cycle) > 0 &&
              fprintf(output, "  \"driver_origin_cycle\": %" PRIu64 ",\n", identity->family->driver_origin_cycle) > 0 &&
              fprintf(output, "  \"logical_vblanks\": %u,\n", scenario->logical_vblanks) > 0 &&
              fprintf(output, "  \"capture_frames\": %u,\n", scenario->capture_frames) > 0 &&
              fprintf(output, "  \"capture_span_cycles\": %" PRIu64 ",\n", trace_end_cycle - trace_begin_cycle) > 0 &&
              fputs("  \"voicegroup_symbol\": ", output) >= 0 && write_json_string(output, voicegroup_symbol) &&
              fputs(",\n", output) >= 0 && fprintf(output, "  \"voice_index\": %u,\n", (unsigned)voice_index) > 0 &&
              fputs("  \"family\": ", output) >= 0 && write_json_string(output, identity->family->name) &&
              fputs(",\n", output) >= 0 && fprintf(output, "  \"resolved_type\": %u,\n", (unsigned)resolved_type) > 0 &&
              fputs("  \"tone_data_sha256\": ", output) >= 0 && write_json_string(output, identity->tone_data_sha256) &&
              fputs(",\n", output) >= 0 &&
              fprintf(output, "  \"family_payload_size\": %zu,\n", identity->payload_size) > 0 &&
              fputs("  \"family_payload_sha256\": ", output) >= 0 &&
              write_json_string(output, identity->family_payload_sha256) && fputs(",\n", output) >= 0 &&
              fputs("  \"rom_sha256\": ", output) >= 0 && write_json_string(output, rom_sha256) &&
              fputs(",\n", output) >= 0 && fputs("  \"elf_sha256\": ", output) >= 0 &&
              write_json_string(output, elf_sha256) && fputs(",\n", output) >= 0;
    if (!ok)
        return false;
    if (identity->family->id == DRIVER_FAMILY_PSW)
    {
        ok = fputs("  \"waveform_size_bytes\": 16,\n", output) >= 0 &&
             fputs("  \"waveform_bytes_hex\": ", output) >= 0 &&
             write_json_hex(output, identity->payload, identity->payload_size) && fputs(",\n", output) >= 0 &&
             fputs("  \"waveform_sha256\": ", output) >= 0 &&
             write_json_string(output, identity->family_payload_sha256) && fputs(",\n", output) >= 0;
    }
    else if (identity->family->id == DRIVER_FAMILY_DIRECTSOUND)
    {
        const WaveData* wave = identity->directsound_wave;
        ok = wave && fprintf(output, "  \"directsound_wave_type\": %u,\n", (unsigned)wave->type) > 0 &&
             fprintf(output, "  \"directsound_wave_status\": %u,\n", (unsigned)wave->status) > 0 &&
             fprintf(output, "  \"directsound_wave_freq\": %" PRIu32 ",\n", wave->freq) > 0 &&
             fprintf(output, "  \"directsound_wave_loop_start\": %" PRIu32 ",\n", wave->loopStart) > 0 &&
             fprintf(output, "  \"directsound_wave_size\": %" PRIu32 ",\n", wave->size) > 0;
    }
    else
    {
        ok = fputs("  \"square_payload_hex\": ", output) >= 0 && write_json_hex(output, identity->payload, 6u) &&
             fputs(",\n", output) >= 0;
    }
    return ok && fputs("  \"scenario\": ", output) >= 0 && write_json_string(output, scenario->name) &&
           fputs(",\n", output) >= 0 && fputs("  \"high_level_action\": ", output) >= 0 &&
           write_json_string(output, scenario->high_level_action) && fputs(",\n", output) >= 0 &&
           fprintf(output, "  \"note\": %u,\n", (unsigned)options->note) > 0 &&
           fprintf(output, "  \"velocity\": %u,\n", (unsigned)options->velocity) > 0 &&
           fprintf(output, "  \"volume\": %u,\n", (unsigned)options->volume) > 0 &&
           fprintf(output, "  \"pan\": %u,\n", (unsigned)options->pan) > 0 &&
           fputs("  \"trace_sha256\": ", output) >= 0 && write_json_string(output, trace_sha256) &&
           fputs("\n}\n", output) >= 0;
}

int main(int argc, char** argv)
{
    Options options;
    if (!parse_options(argc, argv, &options))
    {
        print_usage(argv[0]);
        return 2;
    }
    const DriverScenario* scenario = driver_find_scenario(options.scenario);
    if (!scenario)
    {
        print_usage(argv[0]);
        return 2;
    }

    uint8_t voice_index = 0;
    if (!parse_control_byte(options.voice_index_text, &voice_index))
    {
        fprintf(stderr, "Error: VOICE_INDEX must be a decimal 0..127\n\n");
        print_usage(argv[0]);
        return 2;
    }

    char* voicegroup_owned = NULL;
    const char* voicegroup_bank = NULL;
    const char* voicegroup_symbol = NULL;
    if (!canonicalize_voicegroup(options.voicegroup, &voicegroup_bank, &voicegroup_symbol, &voicegroup_owned))
    {
        fprintf(stderr, "Error: VOICEGROUP must be a non-empty bank name or a 'voicegroup_' symbol\n\n");
        print_usage(argv[0]);
        return 2;
    }

    LoadedVoiceGroup* vg = voicegroup_load(options.project_root, voicegroup_bank);
    if (!vg)
    {
        const char* err = voicegroup_loader_last_error();
        if (err && err[0])
            fprintf(stderr, "%s. Failed to load voicegroup '%s'\n", err, voicegroup_bank);
        else
            fprintf(stderr, "Failed to load voicegroup '%s'\n", voicegroup_bank);
        free(voicegroup_owned);
        return 1;
    }

    ToneData* voices = voicegroup_loaded_voices(vg);
    ToneData* selected = &voices[voice_index];
    const uint8_t resolved_type = selected->type;
    const DriverFamily* family = driver_family_for_type(resolved_type);
    if (!family)
    {
        fprintf(stderr,
                "Error: voice %u in voicegroup '%s' has unsupported type 0x%02X\n",
                (unsigned)voice_index,
                voicegroup_bank,
                (unsigned)resolved_type);
        voicegroup_free(vg);
        free(voicegroup_owned);
        return 1;
    }

    DriverFixtureIdentity identity = {0};
    if (!driver_build_fixture_identity(selected, family, &identity))
    {
        fprintf(stderr,
                "Error: voice %u in voicegroup '%s' has an unresolved or invalid %s payload\n",
                (unsigned)voice_index,
                voicegroup_bank,
                family->name);
        voicegroup_free(vg);
        free(voicegroup_owned);
        return 1;
    }

    DriverOutputStage output_stage = {0};
    M4ADriver* drv = NULL;
    int result = 1;
    char trace_sha256[65];
    char rom_sha256[65];
    char elf_sha256[65];

    if (!driver_hash_decomp_pair(options.project_root, rom_sha256, elf_sha256))
    {
        fprintf(stderr,
                "Error: could not hash pokeemerald-hearth.gba and pokeemerald-hearth.elf in '%s'\n",
                options.project_root);
        goto cleanup;
    }
    output_stage.manifest_path = path_with_suffix(options.trace_output, ".manifest.json");
    if (!output_stage.manifest_path)
    {
        fprintf(stderr, "Error: could not allocate output paths\n");
        goto cleanup;
    }
    DriverOutputPairState output_pair_state = DRIVER_OUTPUT_PAIR_EMPTY;
    if (!driver_require_empty_output_pair(options.trace_output, output_stage.manifest_path, &output_pair_state))
    {
        if (errno != EEXIST)
        {
            fprintf(stderr,
                    "Error: could not inspect candidate outputs '%s' and '%s': %s\n",
                    options.trace_output,
                    output_stage.manifest_path,
                    strerror(errno));
        }
        else if (output_pair_state == DRIVER_OUTPUT_PAIR_COMPLETE)
        {
            fprintf(stderr,
                    "Error: candidate outputs '%s' and '%s' already exist; replacement is not supported\n",
                    options.trace_output,
                    output_stage.manifest_path);
        }
        else if (output_pair_state == DRIVER_OUTPUT_PAIR_TRACE_ORPHAN)
        {
            fprintf(stderr,
                    "Error: refusing to replace trace orphan '%s' without manifest '%s'\n",
                    options.trace_output,
                    output_stage.manifest_path);
        }
        else
        {
            fprintf(stderr,
                    "Error: refusing to replace manifest orphan '%s' without trace '%s'\n",
                    output_stage.manifest_path,
                    options.trace_output);
        }
        goto cleanup;
    }

    drv = m4a_driver_create((float)DRIVER_HOST_RATE_HZ);
    if (!drv)
    {
        fprintf(stderr, "Error: could not create M4ADriver\n");
        goto cleanup;
    }
    if (!m4a_driver_set_initial_cycle(drv, family->driver_origin_cycle))
    {
        fprintf(stderr, "Error: could not rebase driver timeline\n");
        goto cleanup;
    }
    m4a_driver_set_voicegroup(drv, voices);
    m4a_program_change(drv, 0, voice_index);
    const DriverScenarioControls controls = {
        .context = drv,
        .note_on = driver_m4a_note_on,
        .note_off = driver_m4a_note_off,
        .pitch_bend = driver_m4a_pitch_bend,
        .cc = driver_m4a_cc,
    };
    int advanced_frames = 0;
    driver_apply_scenario_actions(scenario, &options, &controls, 0u);
    const uint64_t trace_begin_cycle = 0u;
    const uint64_t trace_end_cycle = scenario->end_cycle;

    const M4ARegisterFile* registers = m4a_get_register_file(drv);
    const uint16_t soundcnt_l =
        (uint16_t)(((uint16_t)registers->master_vol_left << 4u) | registers->master_vol_right |
                   ((uint16_t)registers->pan_mask_left << 12u) | ((uint16_t)registers->pan_mask_right << 8u));
    uint16_t soundcnt_h = (uint16_t)(registers->psg_volume_code & 3u);
    soundcnt_h |= (uint16_t)(registers->dma_a_volume_code & 1u) << 2u;
    soundcnt_h |= (uint16_t)(registers->dma_b_volume_code & 1u) << 3u;
    if (registers->dma_a_enable_right)
        soundcnt_h |= 1u << 8u;
    if (registers->dma_a_enable_left)
        soundcnt_h |= 1u << 9u;
    if (registers->dma_b_enable_right)
        soundcnt_h |= 1u << 12u;
    if (registers->dma_b_enable_left)
        soundcnt_h |= 1u << 13u;

    /* Pre-BEGIN setup gives both replay engines the initialized register file
     * while the first timer-driven DMA refill remains measured. */
    M4ARegWrite setup[4] = {
        {0u, M4A_REG_NR52, 0x0080u, 0u},
        {0u, M4A_REG_NR50, (uint32_t)(((uint16_t)registers->master_vol_left << 4u) | registers->master_vol_right), 1u},
        {0u, M4A_REG_SOUNDCNT_H, (uint32_t)registers->psg_volume_code, 2u},
    };
    size_t setup_count = 3u;
    if (family->id == DRIVER_FAMILY_DIRECTSOUND)
    {
        setup[2].value = soundcnt_h;
        setup[3] = (M4ARegWrite){
            .cycle = 0u,
            .reg = M4A_REG_SOUNDBIAS,
            .value = (uint32_t)registers->bias_level | ((uint32_t)registers->bias_sampling_cycle << 14u),
            .order = 3u,
        };
        setup_count = 4u;
    }

    output_stage.trace_file = driver_open_unique_temp(options.trace_output, &output_stage.trace_temp);
    M4ADriverTraceWriter trace_writer;
    if (!output_stage.trace_file ||
        !m4a_driver_trace_begin_with_setup(
            &trace_writer, output_stage.trace_file, trace_begin_cycle, trace_end_cycle, soundcnt_l, setup, setup_count))
    {
        fprintf(stderr,
                "Error: could not open trace '%s': %s\n",
                output_stage.trace_temp ? output_stage.trace_temp : options.trace_output,
                strerror(errno));
        goto cleanup;
    }

    DriverTraceMerge trace_merge = {
        .writer = &trace_writer,
        .family = family,
        .end_cycle = trace_end_cycle,
        .next_sample_cycle = trace_begin_cycle,
    };
    bool trace_serialized = true;
    for (unsigned tick = 0u; tick < scenario->capture_frames; ++tick)
    {
        if (tick != 0u && driver_is_scenario_action_tick(scenario, tick))
            driver_apply_scenario_actions(scenario, &options, &controls, tick);

        const uint64_t scheduled_vblank = trace_begin_cycle + (uint64_t)(tick + 1u) * M4A_VBLANK_CYCLES;
        const uint64_t target_cycle = tick + 1u == scenario->capture_frames ? trace_end_cycle : scheduled_vblank;
        const int target_frames =
            (int)((target_cycle * DRIVER_HOST_RATE_HZ + M4A_GBA_CYCLES_PER_SECOND - 1u) / M4A_GBA_CYCLES_PER_SECOND);
        const int frames = target_frames - advanced_frames;
        if (frames <= 0)
        {
            fprintf(stderr, "Error: invalid cumulative capture advance at tick %u\n", tick);
            trace_serialized = false;
            break;
        }
        m4a_advance(drv, frames);
        advanced_frames = target_frames;

        const uint64_t current_cycle = m4a_driver_current_cycle(drv);
        if (current_cycle < target_cycle || current_cycle >= target_cycle + M4A_VBLANK_CYCLES)
        {
            m4a_consume_writes(drv);
            fprintf(stderr, "Error: driver did not reach the scheduled capture boundary at tick %u\n", tick);
            trace_serialized = false;
            break;
        }
        const uint32_t dropped = m4a_get_events_dropped(drv);
        if (dropped != 0u)
        {
            m4a_consume_writes(drv);
            fprintf(stderr, "Error: driver dropped %" PRIu32 " queue events during the scenario\n", dropped);
            trace_serialized = false;
            break;
        }
        const M4ARegWriteBatch* batch = m4a_get_pending_writes(drv);
        const bool batch_written = driver_write_merged_batch(&trace_merge, batch);
        m4a_consume_writes(drv);
        if (!batch_written)
        {
            trace_serialized = false;
            break;
        }
    }
    if (trace_serialized)
        trace_serialized = driver_finish_merged_trace(&trace_merge);
    if (!trace_serialized || !m4a_driver_trace_end(&trace_writer))
    {
        fprintf(stderr, "Error: could not serialize driver trace '%s'\n", output_stage.trace_temp);
        goto cleanup;
    }

    if (!driver_sha256_open_stream(output_stage.trace_file, trace_sha256))
    {
        fprintf(stderr, "Error: could not hash trace '%s'\n", output_stage.trace_temp);
        goto cleanup;
    }

    output_stage.manifest_file = driver_open_unique_temp(output_stage.manifest_path, &output_stage.manifest_temp);
    if (!output_stage.manifest_file)
    {
        fprintf(stderr, "Error: could not open manifest '%s': %s\n", output_stage.manifest_path, strerror(errno));
        goto cleanup;
    }
    if (!write_manifest(output_stage.manifest_file,
                        &options,
                        scenario,
                        trace_begin_cycle,
                        trace_end_cycle,
                        voicegroup_symbol,
                        voice_index,
                        resolved_type,
                        &identity,
                        rom_sha256,
                        elf_sha256,
                        trace_sha256))
    {
        fprintf(stderr, "Error: could not write manifest '%s'\n", output_stage.manifest_temp);
        goto cleanup;
    }

    /* The manifest final name is this pair's commit point. Two fixed sibling
     * names cannot switch crash-atomically, so a process crash after the trace
     * publication can leave a trace orphan. Future runs preserve and reject
     * that orphan; ordinary in-process failures roll back only identity-matched
     * streams through cleanup. */
    if (!driver_publish_noreplace(
            output_stage.trace_file, output_stage.trace_temp, options.trace_output, &output_stage.trace_published))
    {
        fprintf(stderr, "Error: could not publish trace '%s': %s\n", options.trace_output, strerror(errno));
        goto cleanup;
    }
    free(output_stage.trace_temp);
    output_stage.trace_temp = NULL;
    if (!driver_publish_noreplace(output_stage.manifest_file,
                                  output_stage.manifest_temp,
                                  output_stage.manifest_path,
                                  &output_stage.manifest_published))
    {
        fprintf(stderr, "Error: could not publish manifest '%s': %s\n", output_stage.manifest_path, strerror(errno));
        goto cleanup;
    }
    free(output_stage.manifest_temp);
    output_stage.manifest_temp = NULL;

    const bool trace_closed = fclose(output_stage.trace_file) == 0;
    output_stage.trace_file = NULL;
    const bool manifest_closed = fclose(output_stage.manifest_file) == 0;
    output_stage.manifest_file = NULL;
    if (!trace_closed || !manifest_closed)
    {
        fprintf(stderr, "Error: could not close committed candidate artifacts: %s\n", strerror(errno));
        goto cleanup;
    }
    output_stage.trace_published = false;
    output_stage.manifest_published = false;
    result = 0;

cleanup:
    if (!driver_output_stage_cleanup(&output_stage, options.trace_output))
    {
        fprintf(stderr, "Error: could not safely clean up unpublished candidate artifacts: %s\n", strerror(errno));
        result = 1;
    }
    if (drv)
        m4a_driver_destroy(drv);
    driver_fixture_identity_cleanup(&identity);
    voicegroup_free(vg);
    free(voicegroup_owned);
    return result;
}
