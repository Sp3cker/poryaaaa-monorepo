#if !defined(_WIN32) && !defined(_POSIX_C_SOURCE)
#    define _POSIX_C_SOURCE 200809L
#endif

#include "test_assert.h"

#define main poryaaaa_driver_trace_command_main
#include "../cmd/poryaaaa_driver_trace.c"
#undef main

#if defined(_WIN32)
#    include <direct.h>
#else
#    include <sys/stat.h>
#    include <unistd.h>
#endif

typedef enum
{
    DRIVER_TEST_CC,
    DRIVER_TEST_NOTE_ON,
    DRIVER_TEST_NOTE_OFF,
    DRIVER_TEST_PITCH_BEND,
} DriverTestActionKind;

typedef struct
{
    unsigned tick;
    DriverTestActionKind kind;
    uint8_t first;
    uint8_t second;
    int16_t bend;
} DriverTestAction;

typedef struct
{
    unsigned tick;
    DriverTestAction actions[8];
    size_t count;
} DriverControlSpy;

typedef struct
{
    unsigned tick;
    DriverTestActionKind kind;
    uint8_t first;
    uint8_t second;
    int16_t bend;
} ExpectedAction;

static void spy_record(DriverControlSpy* spy, DriverTestActionKind kind, uint8_t first, uint8_t second, int16_t bend)
{
    spy->actions[spy->count++] = (DriverTestAction){
        .tick = spy->tick,
        .kind = kind,
        .first = first,
        .second = second,
        .bend = bend,
    };
}

static void spy_note_on(void* context, uint8_t note, uint8_t velocity)
{
    spy_record((DriverControlSpy*)context, DRIVER_TEST_NOTE_ON, note, velocity, 0);
}

static void spy_note_off(void* context, uint8_t note)
{
    spy_record((DriverControlSpy*)context, DRIVER_TEST_NOTE_OFF, note, 0u, 0);
}

static void spy_pitch_bend(void* context, int16_t bend)
{
    spy_record((DriverControlSpy*)context, DRIVER_TEST_PITCH_BEND, 0u, 0u, bend);
}

static void spy_cc(void* context, uint8_t cc, uint8_t value)
{
    spy_record((DriverControlSpy*)context, DRIVER_TEST_CC, cc, value, 0);
}

static void test_scenario_schedule(const char* name,
                                   unsigned logical_vblanks,
                                   unsigned capture_frames,
                                   uint64_t end_cycle,
                                   const ExpectedAction* expected,
                                   size_t expected_count)
{
    const DriverScenario* scenario = driver_find_scenario(name);
    ASSERT(scenario != NULL, "known lifecycle scenario is recognized");
    if (!scenario)
        return;
    ASSERT_EQ(scenario->logical_vblanks, logical_vblanks, "scenario has the fixed logical VBlank span");
    ASSERT_EQ(scenario->capture_frames, capture_frames, "scenario has the fixed capture-frame span");
    ASSERT(capture_frames >= logical_vblanks + 8u, "scenario includes the full-ring observation tail");
    ASSERT(driver_is_scenario_action_tick(scenario, logical_vblanks - 1u),
           "last logical VBlank accepts scheduled actions");
    ASSERT(!driver_is_scenario_action_tick(scenario, logical_vblanks),
           "observation-only VBlank accepts no scenario action");
    ASSERT(scenario->end_cycle == end_cycle, "scenario has the fixed trace endpoint");

    const Options options = {
        .note = 60u,
        .velocity = 91u,
        .volume = 87u,
        .pan = 23u,
    };
    DriverControlSpy spy = {0};
    const DriverScenarioControls controls = {
        .context = &spy,
        .note_on = spy_note_on,
        .note_off = spy_note_off,
        .pitch_bend = spy_pitch_bend,
        .cc = spy_cc,
    };
    unsigned advances = 0u;
    for (unsigned tick = 0u; tick < capture_frames; ++tick)
    {
        ++advances;
        if (driver_is_scenario_action_tick(scenario, tick))
        {
            spy.tick = tick;
            driver_apply_scenario_actions(scenario, &options, &controls, tick);
        }
    }
    ASSERT_EQ(advances, capture_frames, "scenario advances through every observation-only VBlank");

    ASSERT(spy.count == expected_count, "scenario emits the expected number of high-level controls");
    const size_t compared_count = spy.count < expected_count ? spy.count : expected_count;
    for (size_t index = 0u; index < compared_count; ++index)
    {
        ASSERT_EQ(spy.actions[index].tick, expected[index].tick, "control is invoked at the intended logical tick");
        ASSERT_EQ(spy.actions[index].kind, expected[index].kind, "control uses the intended public operation");
        ASSERT_EQ(spy.actions[index].first, expected[index].first, "control has the intended first argument");
        ASSERT_EQ(spy.actions[index].second, expected[index].second, "control has the intended second argument");
        ASSERT_EQ(spy.actions[index].bend, expected[index].bend, "control has the intended pitch-bend argument");
    }
}

static void test_all_lifecycle_scenarios(void)
{
    static const ExpectedAction start[] = {
        {0u, DRIVER_TEST_CC, 0x07u, 87u, 0},
        {0u, DRIVER_TEST_CC, 0x0Au, 23u, 0},
        {0u, DRIVER_TEST_NOTE_ON, 60u, 91u, 0},
    };
    static const ExpectedAction pitch[] = {
        {0u, DRIVER_TEST_CC, 0x07u, 87u, 0},
        {0u, DRIVER_TEST_CC, 0x0Au, 23u, 0},
        {0u, DRIVER_TEST_NOTE_ON, 60u, 91u, 0},
        {2u, DRIVER_TEST_PITCH_BEND, 0u, 0u, 2048},
    };
    static const ExpectedAction volume_pan[] = {
        {0u, DRIVER_TEST_CC, 0x07u, 87u, 0},
        {0u, DRIVER_TEST_CC, 0x0Au, 23u, 0},
        {0u, DRIVER_TEST_NOTE_ON, 60u, 91u, 0},
        {2u, DRIVER_TEST_CC, 0x07u, 32u, 0},
        {2u, DRIVER_TEST_CC, 0x0Au, 127u, 0},
    };
    static const ExpectedAction retrigger[] = {
        {0u, DRIVER_TEST_CC, 0x07u, 87u, 0},
        {0u, DRIVER_TEST_CC, 0x0Au, 23u, 0},
        {0u, DRIVER_TEST_NOTE_ON, 60u, 91u, 0},
        {2u, DRIVER_TEST_NOTE_OFF, 60u, 0u, 0},
        {3u, DRIVER_TEST_NOTE_ON, 60u, 91u, 0},
    };
    static const ExpectedAction release[] = {
        {0u, DRIVER_TEST_CC, 0x07u, 87u, 0},
        {0u, DRIVER_TEST_CC, 0x0Au, 23u, 0},
        {0u, DRIVER_TEST_NOTE_ON, 60u, 91u, 0},
        {2u, DRIVER_TEST_NOTE_OFF, 60u, 0u, 0},
    };

    test_scenario_schedule("start", 1u, 9u, 2536960u, start, sizeof(start) / sizeof(start[0]));
    test_scenario_schedule("envelope", 6u, 15u, 4222464u, start, sizeof(start) / sizeof(start[0]));
    test_scenario_schedule("pitch", 4u, 12u, 3379712u, pitch, sizeof(pitch) / sizeof(pitch[0]));
    test_scenario_schedule("volume-pan", 4u, 12u, 3379712u, volume_pan, sizeof(volume_pan) / sizeof(volume_pan[0]));
    test_scenario_schedule("retrigger", 5u, 15u, 4222464u, retrigger, sizeof(retrigger) / sizeof(retrigger[0]));
    test_scenario_schedule("release", 6u, 14u, 3941376u, release, sizeof(release) / sizeof(release[0]));
}

static void test_unknown_scenario_fails(void)
{
    char* argv[] = {
        "poryaaaa_driver_trace",
        ".",
        "voicegroup",
        "0",
        "--scenario",
        "unknown",
        "--trace-output",
        "candidate.trace",
    };
    ASSERT_EQ(poryaaaa_driver_trace_command_main((int)(sizeof(argv) / sizeof(argv[0])), argv),
              2,
              "unknown scenario is rejected as invalid CLI");
}

/* Remove a directory created by a portable publication test. */
static bool test_remove_directory(const char* path)
{
#if defined(_WIN32)
    return _rmdir(path) == 0;
#else
    return rmdir(path) == 0;
#endif
}

/* Create one isolated output directory on the active host platform. */
static char* test_make_temp_dir(char* out, size_t out_size)
{
#if defined(_WIN32)
    char temp_root[MAX_PATH];
    char temp_dir[MAX_PATH];
    const DWORD root_length = GetTempPathA((DWORD)sizeof(temp_root), temp_root);
    if (root_length == 0u || root_length >= (DWORD)sizeof(temp_root) ||
        GetTempFileNameA(temp_root, "pdt", 0u, temp_dir) == 0u || !DeleteFileA(temp_dir) ||
        !CreateDirectoryA(temp_dir, NULL))
    {
        return NULL;
    }
    const size_t length = strlen(temp_dir);
    if (length + 1u > out_size)
    {
        RemoveDirectoryA(temp_dir);
        return NULL;
    }
    memcpy(out, temp_dir, length + 1u);
    return out;
#else
    char template_path[] = "/tmp/poryaaaa-driver-publish.XXXXXX";
    const int descriptor = mkstemp(template_path);
    if (descriptor < 0)
        return NULL;
    const bool ready = close(descriptor) == 0 && unlink(template_path) == 0 && mkdir(template_path, 0700) == 0;
    if (!ready)
        return NULL;
    const size_t length = strlen(template_path);
    if (length + 1u > out_size)
    {
        rmdir(template_path);
        return NULL;
    }
    memcpy(out, template_path, length + 1u);
    return out;
#endif
}

/* Prove exclusive staging names remain distinct for concurrent publishers. */
static void test_unique_exclusive_staging(void)
{
    char directory[512];
    char* dir = test_make_temp_dir(directory, sizeof(directory));
    ASSERT(dir != NULL, "creates an isolated staging directory");
    if (!dir)
        return;
    char* trace_output = path_with_suffix(dir, "/candidate.trace");
    ASSERT(trace_output != NULL, "allocates a staging destination path");
    if (!trace_output)
    {
        test_remove_directory(dir);
        return;
    }

    char* first_path = NULL;
    char* second_path = NULL;
    FILE* first = driver_open_unique_temp(trace_output, &first_path);
    FILE* second = driver_open_unique_temp(trace_output, &second_path);
    ASSERT(first && second, "opens independently created staging files");
    ASSERT(first_path && second_path && strcmp(first_path, second_path) != 0,
           "independent publishers receive distinct staging names");
    if (first)
    {
        ASSERT(fputs("first", first) >= 0, "writes only to the first exclusive staging stream");
        fclose(first);
    }
    if (second)
    {
        ASSERT(fputs("second", second) >= 0, "writes only to the second exclusive staging stream");
        fclose(second);
    }

    if (first_path)
        remove(first_path);
    if (second_path)
        remove(second_path);
    free(first_path);
    free(second_path);
    free(trace_output);
    test_remove_directory(dir);
}

/* Compare a complete small publication artifact with expected bytes. */
static bool test_file_equals(const char* path, const char* expected)
{
    FILE* input = fopen(path, "rb");
    if (!input)
        return false;
    const size_t expected_size = strlen(expected);
    char actual[64] = {0};
    const size_t read = fread(actual, 1u, sizeof(actual), input);
    const bool closed = fclose(input) == 0;
    return closed && read == expected_size && memcmp(actual, expected, expected_size) == 0;
}
/* Refuse every occupied public-name state without changing its owner bytes. */
static void test_output_pair_preflight_preserves_existing_entries(void)
{
    char directory[512];
    char* dir = test_make_temp_dir(directory, sizeof(directory));
    ASSERT(dir != NULL, "creates output-pair preflight directory");
    if (!dir)
        return;
    char* final = path_with_suffix(dir, "/candidate.trace");
    char* manifest = final ? path_with_suffix(final, ".manifest.json") : NULL;
    ASSERT(final && manifest, "allocates output-pair preflight paths");
    if (!final || !manifest)
    {
        free(final);
        free(manifest);
        test_remove_directory(dir);
        return;
    }

    DriverOutputPairState state = DRIVER_OUTPUT_PAIR_COMPLETE;
    errno = 0;
    ASSERT(driver_require_empty_output_pair(final, manifest, &state) && state == DRIVER_OUTPUT_PAIR_EMPTY,
           "empty sibling names are accepted for a new pair");

    FILE* existing_trace = fopen(final, "wb");
    ASSERT(existing_trace != NULL, "creates an unowned trace orphan");
    if (existing_trace)
    {
        ASSERT(fputs("existing-trace", existing_trace) >= 0, "writes the unowned trace orphan");
        fclose(existing_trace);
    }
    errno = 0;
    ASSERT(!driver_require_empty_output_pair(final, manifest, &state) && state == DRIVER_OUTPUT_PAIR_TRACE_ORPHAN &&
               errno == EEXIST,
           "trace orphan is diagnosed and rejected before staging");
    ASSERT(test_file_equals(final, "existing-trace"), "trace orphan bytes are preserved");

    FILE* existing_manifest = fopen(manifest, "wb");
    ASSERT(existing_manifest != NULL, "creates an unowned manifest");
    if (existing_manifest)
    {
        ASSERT(fputs("existing-manifest", existing_manifest) >= 0, "writes the unowned manifest");
        fclose(existing_manifest);
    }
    errno = 0;
    ASSERT(!driver_require_empty_output_pair(final, manifest, &state) && state == DRIVER_OUTPUT_PAIR_COMPLETE &&
               errno == EEXIST,
           "complete existing pair is rejected without replacement");
    ASSERT(test_file_equals(final, "existing-trace") && test_file_equals(manifest, "existing-manifest"),
           "complete existing pair bytes are preserved");

    ASSERT(remove(final) == 0, "removes only the test-owned trace to form a manifest orphan");
    errno = 0;
    ASSERT(!driver_require_empty_output_pair(final, manifest, &state) && state == DRIVER_OUTPUT_PAIR_MANIFEST_ORPHAN &&
               errno == EEXIST,
           "manifest orphan is diagnosed and rejected before staging");
    ASSERT(test_file_equals(manifest, "existing-manifest"), "manifest orphan bytes are preserved");

    remove(final);
    remove(manifest);
    free(final);
    free(manifest);
    test_remove_directory(dir);
}

/* Commit an open trace and manifest stream with no-replace semantics. */
static void test_publish_success(void)
{
    char directory[512];
    char* dir = test_make_temp_dir(directory, sizeof(directory));
    ASSERT(dir != NULL, "creates publish test directory");
    if (!dir)
        return;
    char* final = path_with_suffix(dir, "/candidate.trace");
    char* final_manifest = final ? path_with_suffix(final, ".manifest.json") : NULL;
    ASSERT(final && final_manifest, "allocates final paths");
    if (!final || !final_manifest)
    {
        free(final);
        free(final_manifest);
        test_remove_directory(dir);
        return;
    }

    char* trace_temp = NULL;
    FILE* trace = driver_open_unique_temp(final, &trace_temp);
    ASSERT(trace != NULL, "opens trace staging file");
    bool trace_published = false;
    if (trace)
    {
        ASSERT(fputs("trace-bytes", trace) >= 0, "writes the staged trace");
        char hash[65];
        ASSERT(driver_sha256_open_stream(trace, hash), "hashes the live staged trace descriptor");
        char expected[65];
        driver_sha256_bytes((const uint8_t*)"trace-bytes", 11u, expected);
        ASSERT(strcmp(hash, expected) == 0, "staged descriptor hash matches memory hash");
        ASSERT(driver_publish_noreplace(trace, trace_temp, final, &trace_published) && trace_published,
               "publishes trace with create-once semantics while its descriptor remains open");
        if (trace_published)
        {
            free(trace_temp);
            trace_temp = NULL;
        }
    }

    char* manifest_temp = NULL;
    FILE* manifest = driver_open_unique_temp(final_manifest, &manifest_temp);
    ASSERT(manifest != NULL, "opens manifest staging file");
    bool manifest_published = false;
    if (manifest)
    {
        ASSERT(fputs("{manifest}", manifest) >= 0, "writes the staged manifest");
        ASSERT(driver_publish_noreplace(manifest, manifest_temp, final_manifest, &manifest_published) &&
                   manifest_published,
               "publishes manifest with create-once semantics while its descriptor remains open");
        if (manifest_published)
        {
            free(manifest_temp);
            manifest_temp = NULL;
        }
    }
    if (trace)
        fclose(trace);
    if (manifest)
        fclose(manifest);
    ASSERT(test_file_equals(final, "trace-bytes"), "published trace contains staged bytes");
    ASSERT(test_file_equals(final_manifest, "{manifest}"), "published manifest contains staged bytes");

    if (trace_temp)
        remove(trace_temp);
    if (manifest_temp)
        remove(manifest_temp);
    remove(final);
    remove(final_manifest);
    free(trace_temp);
    free(manifest_temp);
    free(final);
    free(final_manifest);
    test_remove_directory(dir);
}

/* Reject a final-name conflict without changing the original artifact. */
static void test_publish_trace_conflict_preserves_existing(void)
{
    char directory[512];
    char* dir = test_make_temp_dir(directory, sizeof(directory));
    ASSERT(dir != NULL, "creates conflict test directory");
    if (!dir)
        return;
    char* final = path_with_suffix(dir, "/candidate.trace");
    ASSERT(final != NULL, "allocates conflict path");
    if (!final)
    {
        test_remove_directory(dir);
        return;
    }
    FILE* existing = fopen(final, "wb");
    ASSERT(existing != NULL, "pre-creates existing trace");
    if (existing)
    {
        fputs("existing", existing);
        fclose(existing);
    }

    char* trace_temp = NULL;
    FILE* trace = driver_open_unique_temp(final, &trace_temp);
    ASSERT(trace != NULL, "opens competing trace staging file");
    bool trace_published = false;
    if (trace)
    {
        ASSERT(fputs("new-trace", trace) >= 0, "writes competing staged trace");
        errno = 0;
        ASSERT(!driver_publish_noreplace(trace, trace_temp, final, &trace_published) && !trace_published &&
                   errno == EEXIST,
               "portable no-replace publication rejects an existing final");
        ASSERT(test_file_equals(final, "existing"), "existing trace is unchanged after conflict");
        fclose(trace);
    }
    if (trace_temp)
        remove(trace_temp);
    free(trace_temp);
    remove(final);
    free(final);
    test_remove_directory(dir);
}

/* Roll back this process's trace when a manifest collision appears after preflight. */
static void test_publish_manifest_conflict_rolls_back_trace(void)
{
    char directory[512];
    char* dir = test_make_temp_dir(directory, sizeof(directory));
    ASSERT(dir != NULL, "creates rollback test directory");
    if (!dir)
        return;
    char* final = path_with_suffix(dir, "/candidate.trace");
    char* manifest = final ? path_with_suffix(final, ".manifest.json") : NULL;
    char* manifest_check = final ? path_with_suffix(final, ".manifest.json") : NULL;
    ASSERT(final && manifest && manifest_check, "allocates rollback paths");
    if (!final || !manifest || !manifest_check)
    {
        free(final);
        free(manifest);
        free(manifest_check);
        test_remove_directory(dir);
        return;
    }
    DriverOutputPairState initial_state = DRIVER_OUTPUT_PAIR_COMPLETE;
    ASSERT(driver_require_empty_output_pair(final, manifest, &initial_state) &&
               initial_state == DRIVER_OUTPUT_PAIR_EMPTY,
           "preflight admits the empty sibling names before a competing publisher arrives");

    FILE* existing_manifest = fopen(manifest, "wb");
    ASSERT(existing_manifest != NULL, "pre-creates existing manifest for rollback case");
    if (existing_manifest)
    {
        fputs("existing-manifest", existing_manifest);
        fclose(existing_manifest);
    }

    DriverOutputStage stage = {.manifest_path = manifest};
    manifest = NULL;
    stage.trace_file = driver_open_unique_temp(final, &stage.trace_temp);
    ASSERT(stage.trace_file != NULL, "opens trace staging for rollback test");
    if (stage.trace_file)
    {
        ASSERT(fputs("new-trace", stage.trace_file) >= 0, "writes rollback trace");
        ASSERT(driver_publish_noreplace(stage.trace_file, stage.trace_temp, final, &stage.trace_published) &&
                   stage.trace_published,
               "trace publishes before manifest conflict");
        if (stage.trace_published)
        {
            free(stage.trace_temp);
            stage.trace_temp = NULL;
        }
        stage.manifest_file = driver_open_unique_temp(stage.manifest_path, &stage.manifest_temp);
        ASSERT(stage.manifest_file != NULL, "opens competing manifest staging file");
        if (stage.manifest_file)
        {
            ASSERT(fputs("new-manifest", stage.manifest_file) >= 0, "writes competing manifest");
            errno = 0;
            ASSERT(!driver_publish_noreplace(
                       stage.manifest_file, stage.manifest_temp, stage.manifest_path, &stage.manifest_published) &&
                       !stage.manifest_published && errno == EEXIST,
                   "manifest publication reports the portable existing-name conflict");
        }
        ASSERT(driver_output_stage_cleanup(&stage, final),
               "manifest conflict safely removes the process's already-published trace and staging files");
        ASSERT(!test_file_equals(final, "new-trace"), "newly published trace is rolled back on manifest conflict");
        ASSERT(test_file_equals(manifest_check, "existing-manifest"), "existing manifest is unchanged after rollback");
    }
    else
    {
        driver_output_stage_cleanup(&stage, final);
    }
    remove(final);
    remove(manifest_check);
    free(final);
    free(manifest);
    free(manifest_check);
    test_remove_directory(dir);
}

/* Preserve a replacement that no longer belongs to this publisher. */
static void test_rollback_preserves_replaced_final(void)
{
    char directory[512];
    char* dir = test_make_temp_dir(directory, sizeof(directory));
    ASSERT(dir != NULL, "creates replacement rollback test directory");
    if (!dir)
        return;
    char* final = path_with_suffix(dir, "/candidate.trace");
    ASSERT(final != NULL, "allocates replacement rollback path");
    if (!final)
    {
        test_remove_directory(dir);
        return;
    }
    DriverOutputStage stage = {0};
    stage.trace_file = driver_open_unique_temp(final, &stage.trace_temp);
    ASSERT(stage.trace_file != NULL, "opens trace staging for replacement rollback");
    if (stage.trace_file)
    {
        ASSERT(fputs("owned-trace", stage.trace_file) >= 0, "writes the owned trace");
        ASSERT(driver_publish_noreplace(stage.trace_file, stage.trace_temp, final, &stage.trace_published) &&
                   stage.trace_published,
               "publishes the owned trace");
        if (stage.trace_published)
        {
            free(stage.trace_temp);
            stage.trace_temp = NULL;
#if defined(_WIN32)
            ASSERT(remove(final) != 0, "retained Windows handle prevents replacement before rollback");
            ASSERT(driver_output_stage_cleanup(&stage, final),
                   "handle-directed Windows rollback removes only the committed stream");
            ASSERT(!test_file_equals(final, "owned-trace"), "Windows rollback removes the owned final");
#else
            ASSERT(remove(final) == 0, "simulates another publisher removing the owned final");
            FILE* replacement = fopen(final, "wb");
            ASSERT(replacement != NULL, "creates replacement final before rollback");
            if (replacement)
            {
                fputs("replacement", replacement);
                fclose(replacement);
            }
            ASSERT(!driver_output_stage_cleanup(&stage, final),
                   "rollback rejects a final path that no longer identifies this process's stream");
            ASSERT(test_file_equals(final, "replacement"), "rollback never deletes a replaced final path");
#endif
        }
    }
    remove(final);
    free(final);
    test_remove_directory(dir);
}

/* Remove an owned publication and any POSIX staging hardlink left behind. */
static void test_rollback_cleans_owned_publication(void)
{
    char directory[512];
    char* dir = test_make_temp_dir(directory, sizeof(directory));
    ASSERT(dir != NULL, "creates owned cleanup test directory");
    if (!dir)
        return;
    char* final = path_with_suffix(dir, "/candidate.trace");
    ASSERT(final != NULL, "allocates owned cleanup path");
    if (!final)
    {
        test_remove_directory(dir);
        return;
    }
    DriverOutputStage stage = {0};
    stage.trace_file = driver_open_unique_temp(final, &stage.trace_temp);
    ASSERT(stage.trace_file != NULL, "opens owned cleanup staging file");
    if (stage.trace_file)
    {
        ASSERT(fputs("owned-trace", stage.trace_file) >= 0, "writes owned cleanup trace");
        char* staged_name = stage.trace_temp ? path_with_suffix(stage.trace_temp, "") : NULL;
        ASSERT(staged_name != NULL, "copies the staged name before cleanup");
#if defined(_WIN32)
        ASSERT(driver_publish_noreplace(stage.trace_file, stage.trace_temp, final, &stage.trace_published) &&
                   stage.trace_published,
               "Windows publishes the owned file with its no-replace primitive");
        if (stage.trace_published)
        {
            free(stage.trace_temp);
            stage.trace_temp = NULL;
        }
#else
        ASSERT(fflush(stage.trace_file) == 0 && link(stage.trace_temp, final) == 0,
               "simulates a POSIX publication whose staged hardlink cleanup failed");
        stage.trace_published = true;
#endif
        ASSERT(driver_output_stage_cleanup(&stage, final),
               "safe rollback removes both the owned final and any leaked staged hardlink");
        ASSERT(!test_file_equals(final, "owned-trace"), "owned final is absent after rollback");
        if (staged_name)
        {
            FILE* staged = fopen(staged_name, "rb");
            ASSERT(staged == NULL, "no staged hardlink remains after rollback");
            if (staged)
                fclose(staged);
        }
        free(staged_name);
    }
    remove(final);
    free(final);
    test_remove_directory(dir);
}

#if !defined(_WIN32)
/* Make the POSIX staged hardlink cleanup fail after link publication. */
static void test_posix_staged_cleanup_failure_is_not_success(void)
{
    char directory[512];
    char* dir = test_make_temp_dir(directory, sizeof(directory));
    ASSERT(dir != NULL, "creates POSIX staged-cleanup test directory");
    if (!dir)
        return;
    char* staging_dir = path_with_suffix(dir, "/staging");
    char* final = path_with_suffix(dir, "/candidate.trace");
    char* staging_base = staging_dir ? path_with_suffix(staging_dir, "/candidate.trace") : NULL;
    const bool staging_ready = staging_dir && final && staging_base && mkdir(staging_dir, 0700) == 0;
    ASSERT(staging_ready, "creates a separate writable staging directory");
    if (!staging_ready)
    {
        free(staging_dir);
        free(final);
        free(staging_base);
        test_remove_directory(dir);
        return;
    }
    DriverOutputStage stage = {0};
    stage.trace_file = driver_open_unique_temp(staging_base, &stage.trace_temp);
    ASSERT(stage.trace_file != NULL, "opens the separately staged trace");
    if (stage.trace_file)
    {
        ASSERT(fputs("owned-trace", stage.trace_file) >= 0 && chmod(staging_dir, 0500) == 0,
               "makes only staged-name removal unavailable");
        const bool published =
            driver_publish_noreplace(stage.trace_file, stage.trace_temp, final, &stage.trace_published);
        ASSERT(!published && stage.trace_published,
               "POSIX staged-name removal failure returns publication failure after the final hardlink exists");
        ASSERT(test_file_equals(final, "owned-trace"), "failed publication's final entry identifies the owned trace");
        ASSERT(chmod(staging_dir, 0700) == 0, "restores cleanup permission after the explicit failure");
        char* staged_name = stage.trace_temp ? path_with_suffix(stage.trace_temp, "") : NULL;
        ASSERT(driver_output_stage_cleanup(&stage, final),
               "safe rollback removes the final and formerly leaked staged hardlink");
        ASSERT(!test_file_equals(final, "owned-trace"), "failed publication leaves no final trace after rollback");
        if (staged_name)
        {
            FILE* staged = fopen(staged_name, "rb");
            ASSERT(staged == NULL, "failed publication leaves no staged hardlink after rollback");
            if (staged)
                fclose(staged);
        }
        free(staged_name);
    }
    chmod(staging_dir, 0700);
    remove(final);
    free(staging_base);
    rmdir(staging_dir);
    free(staging_dir);
    free(final);
    test_remove_directory(dir);
}
#endif

/* Make two live publishers contend for one no-replace final path. */
static void test_competing_publishers(void)
{
    char directory[512];
    char* dir = test_make_temp_dir(directory, sizeof(directory));
    ASSERT(dir != NULL, "creates competing publishers directory");
    if (!dir)
        return;
    char* final = path_with_suffix(dir, "/candidate.trace");
    ASSERT(final != NULL, "allocates final for competing test");
    if (!final)
    {
        test_remove_directory(dir);
        return;
    }
    char* temp_a = NULL;
    char* temp_b = NULL;
    FILE* first = driver_open_unique_temp(final, &temp_a);
    FILE* second = driver_open_unique_temp(final, &temp_b);
    ASSERT(first && second, "both publishers obtain exclusive staging files");
    bool first_published = false;
    bool second_published = false;
    if (first)
    {
        ASSERT(fputs("publisher-a", first) >= 0, "writes first publisher trace");
        ASSERT(driver_publish_noreplace(first, temp_a, final, &first_published) && first_published,
               "first publisher acquires the final name");
        if (first_published)
        {
            free(temp_a);
            temp_a = NULL;
        }
    }
    if (second)
    {
        ASSERT(fputs("publisher-b", second) >= 0, "writes second publisher trace");
        errno = 0;
        ASSERT(!driver_publish_noreplace(second, temp_b, final, &second_published) && !second_published &&
                   errno == EEXIST,
               "second publisher sees a portable no-replace conflict");
    }
    if (first)
        fclose(first);
    if (second)
        fclose(second);
    ASSERT(test_file_equals(final, "publisher-a"), "final contains exactly the winning publisher bytes");
    if (temp_a)
        remove(temp_a);
    if (temp_b)
        remove(temp_b);
    free(temp_a);
    free(temp_b);
    remove(final);
    free(final);
    test_remove_directory(dir);
}

/* Assert that a selected family exposes only its documented bus projection. */
static void test_family_register_projection(void)
{
    const DriverFamily* psw = driver_family_for_type(VOICE_PROGRAMMABLE_WAVE);
    const DriverFamily* sq1 = driver_family_for_type(VOICE_SQUARE_1);
    const DriverFamily* sq2 = driver_family_for_type(VOICE_SQUARE_2);
    const DriverFamily* directsound = driver_family_for_type(VOICE_DIRECTSOUND);
    ASSERT(psw && sq1 && sq2 && directsound, "all supported family descriptors resolve");
    if (!psw || !sq1 || !sq2 || !directsound)
        return;
    ASSERT(driver_retain_register(psw, M4A_REG_NR30) && driver_retain_register(psw, M4A_REG_WAVE_RAM_WORD_3),
           "PSW retains wave control and RAM writes");
    ASSERT(!driver_retain_register(psw, M4A_REG_NR10) && !driver_retain_register(psw, M4A_REG_FIFO_A),
           "PSW excludes square and FIFO writes");
    ASSERT(driver_retain_register(sq1, M4A_REG_NR10) && driver_retain_register(sq1, M4A_REG_NR14) &&
               driver_retain_register(sq1, M4A_REG_NR51),
           "Sq1 retains its complete register projection");
    ASSERT(!driver_retain_register(sq1, M4A_REG_NR21) && !driver_retain_register(sq1, M4A_REG_WAVE_RAM_BYTE),
           "Sq1 excludes other PSG families");
    ASSERT(driver_retain_register(sq2, M4A_REG_NR21) && driver_retain_register(sq2, M4A_REG_NR24) &&
               driver_retain_register(sq2, M4A_REG_NR50) && driver_retain_register(sq2, M4A_REG_NR51),
           "Sq2 retains its complete register projection");
    ASSERT(!driver_retain_register(sq2, M4A_REG_NR10) && !driver_retain_register(sq2, M4A_REG_FIFO_B),
           "Sq2 excludes Sq1 and FIFO writes");
    ASSERT(driver_retain_register(directsound, M4A_REG_SOUNDCNT_H) &&
               driver_retain_register(directsound, M4A_REG_SOUNDBIAS) &&
               driver_retain_register(directsound, M4A_REG_FIFO_A) &&
               driver_retain_register(directsound, M4A_REG_FIFO_B) &&
               driver_retain_register(directsound, M4A_REG_TIMER_0) &&
               driver_retain_register(directsound, M4A_REG_TIMER_1),
           "DirectSound retains routing, bias, FIFO, and timer events");
    ASSERT(!driver_retain_register(directsound, M4A_REG_NR30) && !driver_retain_register(directsound, M4A_REG_NR50),
           "DirectSound excludes PSG bus writes");
}

static void test_family_identity_and_manifest(void)
{
    static const struct
    {
        uint8_t type;
        DriverFamilyId id;
        uint64_t sample_period;
        uint64_t driver_origin_cycle;
    } cases[] = {
        {VOICE_PROGRAMMABLE_WAVE, DRIVER_FAMILY_PSW, 512u, 0u},
        {VOICE_PROGRAMMABLE_WAVE_ALT, DRIVER_FAMILY_PSW, 512u, 0u},
        {VOICE_SQUARE_1, DRIVER_FAMILY_SQ1, 512u, 0u},
        {VOICE_SQUARE_2, DRIVER_FAMILY_SQ2, 512u, 0u},
        {VOICE_DIRECTSOUND, DRIVER_FAMILY_DIRECTSOUND, 256u, DRIVER_DIRECTSOUND_ORIGIN_CYCLES},
    };
    for (size_t index = 0u; index < sizeof(cases) / sizeof(cases[0]); ++index)
    {
        const DriverFamily* family = driver_family_for_type(cases[index].type);
        ASSERT(family != NULL, "supported lifecycle type resolves to a family");
        if (family)
        {
            ASSERT_EQ(family->id, cases[index].id, "type resolves to the intended family");
            ASSERT_EQ(family->sample_period_cycles, cases[index].sample_period, "family has its native sample cadence");
            ASSERT_EQ(family->driver_origin_cycle,
                      cases[index].driver_origin_cycle,
                      "family has its measured adapter timeline origin");
        }
    }
    ASSERT(driver_family_for_type(VOICE_DIRECTSOUND_NO_RESAMPLE) == NULL &&
               driver_family_for_type(VOICE_SQUARE_1_ALT) == NULL &&
               driver_family_for_type(VOICE_SQUARE_2_ALT) == NULL &&
               driver_family_for_type(VOICE_DIRECTSOUND_ALT) == NULL,
           "unverified alternative types fail closed");
    ASSERT(driver_family_for_type(VOICE_NOISE) == NULL, "unsupported family fails closed");

    uint8_t wave_bytes[DRIVER_WAVEFORM_SIZE] = {0};
    ToneData wave_voice = {
        .type = VOICE_PROGRAMMABLE_WAVE,
        .key = 60u,
        .length = 3u,
        .panSweep = 0x80u,
        .wavePointer = (uint32_t*)wave_bytes,
        .attack = 1u,
        .decay = 2u,
        .sustain = 3u,
        .release = 4u,
    };
    DriverFixtureIdentity identity = {0};
    ASSERT(driver_build_fixture_identity(&wave_voice, driver_family_for_type(wave_voice.type), &identity),
           "programmable-wave fixture identity is serializable");
    ASSERT_EQ(identity.payload_size, DRIVER_WAVEFORM_SIZE, "programmable-wave identity uses sixteen source bytes");
    ASSERT(identity.normalized_tone[4] == 0u && identity.normalized_tone[7] == 0u,
           "normalized ToneData never exposes a host pointer");

    ToneData square = {
        .type = VOICE_SQUARE_1,
        .key = 60u,
        .length = 1u,
        .panSweep = 2u,
        .wavePointer = (uint32_t*)(uintptr_t)3u,
        .attack = 4u,
        .decay = 5u,
        .sustain = 6u,
        .release = 7u,
    };
    DriverFixtureIdentity square_identity = {0};
    ASSERT(driver_build_fixture_identity(&square, driver_family_for_type(square.type), &square_identity),
           "square fixture identity is serializable");
    ASSERT_EQ(square_identity.payload_size, 6u, "square identity contains only sweep duty and ADSR");
    ASSERT(square_identity.payload[0] == 2u && square_identity.payload[1] == 3u && square_identity.payload[5] == 7u,
           "square identity captures source-derived sweep duty and ADSR");
    driver_fixture_identity_cleanup(&square_identity);

    int8_t direct_samples[] = {-1, 0, 1, 2};
    WaveData direct_wave = {
        .type = 0u,
        .status = 1u,
        .freq = 0x12345678u,
        .loopStart = 1u,
        .size = sizeof(direct_samples),
        .data = direct_samples,
    };
    ToneData direct = {
        .type = VOICE_DIRECTSOUND,
        .key = 60u,
        .wav = &direct_wave,
        .attack = 4u,
        .decay = 5u,
        .sustain = 6u,
        .release = 7u,
    };
    DriverFixtureIdentity direct_identity = {0};
    ASSERT(driver_build_fixture_identity(&direct, driver_family_for_type(direct.type), &direct_identity),
           "DirectSound fixture identity is serializable");
    ASSERT_EQ(direct_identity.payload_size, 20u, "DirectSound identity includes GBA header and exact PCM bytes");
    ASSERT(direct_identity.payload[0] == 0u && direct_identity.payload[2] == 1u && direct_identity.payload[16] == 0xFFu,
           "DirectSound identity uses canonical little-endian header and PCM bytes");
    driver_fixture_identity_cleanup(&direct_identity);

    FILE* manifest = tmpfile();
    char provenance[65];
    memset(provenance, 'a', 64u);
    provenance[64] = '\0';
    const Options options = {.note = 60u, .velocity = 127u, .volume = 127u, .pan = 64u};
    ASSERT(manifest != NULL, "opens temporary manifest stream");
    if (manifest)
    {
        ASSERT(write_manifest(manifest,
                              &options,
                              driver_find_scenario("start"),
                              0u,
                              2536960u,
                              "voicegroup_fixture",
                              4u,
                              wave_voice.type,
                              &identity,
                              provenance,
                              provenance,
                              provenance),
               "writes deterministic generic candidate manifest");
        fflush(manifest);
        rewind(manifest);
        char text[4096] = {0};
        fread(text, 1u, sizeof(text) - 1u, manifest);
        ASSERT(strstr(text, "\"family_payload_size\": 16") != NULL, "manifest commits the exact family payload size");
        ASSERT(strstr(text, "\"format\": \"poryaaaa-driver-candidate-trace\"") != NULL,
               "manifest identifies the generic candidate adapter");
        ASSERT(strstr(text, "\"family\": \"psw\"") != NULL, "manifest records derived family");
        ASSERT(strstr(text, "\"trace_begin_cycle\": 0") != NULL && strstr(text, "\"trace_end_cycle\": 2536960") != NULL,
               "manifest commits the measured trace interval");
        ASSERT(strstr(text, "\"driver_origin_cycle\": 0") != NULL,
               "manifest commits the family-specific driver timeline origin");
        ASSERT(strstr(text, "\"tone_data_sha256\"") != NULL, "manifest commits normalized ToneData identity");
        ASSERT(strstr(text, "\"family_payload_sha256\"") != NULL, "manifest commits family payload identity");
        ASSERT(strstr(text, "\"rom_sha256\"") != NULL && strstr(text, "\"elf_sha256\"") != NULL,
               "manifest commits ROM and ELF provenance");
        fclose(manifest);
    }
    driver_fixture_identity_cleanup(&identity);
}

int tests_run = 0;
int tests_passed = 0;

int main(void)
{
    test_family_register_projection();
    test_all_lifecycle_scenarios();
    test_unknown_scenario_fails();
    test_family_identity_and_manifest();
    test_unique_exclusive_staging();
    test_publish_success();
    test_output_pair_preflight_preserves_existing_entries();
    test_publish_trace_conflict_preserves_existing();
    test_publish_manifest_conflict_rolls_back_trace();
    test_rollback_preserves_replaced_final();
    test_rollback_cleans_owned_publication();
#if !defined(_WIN32)
    test_posix_staged_cleanup_failure_is_not_success();
#endif
    test_competing_publishers();
    printf("Driver scenario controls: %d/%d tests passed\n", tests_passed, tests_run);
    return tests_passed == tests_run ? 0 : 1;
}
