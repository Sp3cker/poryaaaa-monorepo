#include "test_assert.h"
#include "native_budget_allocator.h"
#include "voicegroup_load_session.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

int tests_run;
int tests_passed;

enum
{
    PCM_COUNT = 64,
    PROG_COUNT = 40,
    ALIAS_SLOT = PCM_COUNT + PROG_COUNT,
    CANDIDATE_COUNT = 3 * PCM_COUNT + PROG_COUNT,
    LOAD_REQUEST_LIMIT = 224
};

typedef struct
{
    char root[VG_MAX_PATH_LEN];
    char sound[VG_MAX_PATH_LEN];
    char dsMap[VG_MAX_PATH_LEN];
    char pwMap[VG_MAX_PATH_LEN];
    char bank[VG_MAX_PATH_LEN];
    char square[VG_MAX_PATH_LEN];
    char pcm[PCM_COUNT][3][VG_MAX_PATH_LEN];
    char prog[PROG_COUNT][VG_MAX_PATH_LEN];
    size_t reads[PCM_COUNT][3];
    size_t progReads[PROG_COUNT];
    size_t batches, populated, released;
} Fixture;

/* Write real encoded assets so the reader exercises production decoders. */
static bool write_bytes(const char* path, const void* data, size_t size)
{
    FILE* file = fopen(path, "wb");
    if (!file)
        return false;
    bool ok = fwrite(data, 1, size, file) == size;
    return fclose(file) == 0 && ok;
}

/* Give every programmable sample distinguishable packed bytes. */
static void prog_bytes(int index, uint8_t bytes[16])
{
    for (int i = 0; i < 16; i++)
        bytes[i] = (uint8_t)(index + 11 * i);
}

/* Cross storage-growth boundaries with mixed formats, aliases, and soft misses. */
static bool make_fixture(Fixture* f)
{
    strcpy(f->root, "/tmp/poryaaaa-allocation-XXXXXX");
    if (!mkdtemp(f->root))
        return false;
    snprintf(f->sound, sizeof(f->sound), "%s/sound", f->root);
    snprintf(f->dsMap, sizeof(f->dsMap), "%s/direct_sound_data.inc", f->sound);
    snprintf(f->pwMap, sizeof(f->pwMap), "%s/programmable_wave_data.inc", f->sound);
    snprintf(f->bank, sizeof(f->bank), "%s/growth.inc", f->sound);
    snprintf(f->square, sizeof(f->square), "%s/square.inc", f->sound);
    if (mkdir(f->sound, 0700) != 0)
        return false;
    char padding[151];
    memset(padding, 'x', sizeof(padding) - 1);
    padding[sizeof(padding) - 1] = '\0';
    const char* extensions[] = {"wav", "aif", "bin"};
    for (int i = 0; i < PCM_COUNT; i++)
        for (int format = 0; format < 3; format++)
            snprintf(f->pcm[i][format],
                     VG_MAX_PATH_LEN,
                     "%s/sample_%02d_%.*s.%s",
                     f->root,
                     i,
                     1 + (i * 37) % 150,
                     padding,
                     extensions[format]);
    for (int i = 0; i < PROG_COUNT; i++)
        snprintf(f->prog[i], VG_MAX_PATH_LEN, "%s/prog_%02d_%.*s.pcm", f->root, i, 1 + (i * 53) % 150, padding);
    bool ok = true;
    for (int i = 0; ok && i < PCM_COUNT; i++)
    {
        uint8_t wav[] = {'R', 'I', 'F', 'F', 40,  0,   0,   0,   'W',  'A',  'V', 'E', 'f',  'm',  't', ' ',
                         16,  0,   0,   0,   1,   0,   1,   0,   0x22, 0x56, 0,   0,   0x22, 0x56, 0,   0,
                         1,   0,   8,   0,   'd', 'a', 't', 'a', 4,    0,    0,   0,   0,    0,    0,   0};
        uint8_t bin[] = {0, 0, 0, 0x40, 0, 0, 0, 1, 1, 0, 0, 0, 4, 0, 0, 0, 0, 0, 0, 0};
        for (int j = 0; j < 4; j++)
        {
            wav[44 + j] = (uint8_t)(128 + i + j + 1);
            bin[16 + j] = (uint8_t)(i + j + 1);
        }
        ok = i % 2 ? write_bytes(f->pcm[i][2], bin, sizeof(bin)) : write_bytes(f->pcm[i][0], wav, sizeof(wav));
    }
    for (int i = 0; ok && i < PROG_COUNT; i++)
    {
        uint8_t bytes[16];
        prog_bytes(i, bytes);
        ok = write_bytes(f->prog[i], bytes, sizeof(bytes));
    }
    FILE* ds = fopen(f->dsMap, "w");
    FILE* pw = fopen(f->pwMap, "w");
    FILE* bank = fopen(f->bank, "w");
    ok = ds && pw && bank && ok;
    if (ok)
    {
        fprintf(bank, "voice_group growth\n");
        for (int i = 0; i < PCM_COUNT; i++)
        {
            fprintf(ds, "Sample%d::\n.incbin \"%s\"\n", i, f->pcm[i][2] + strlen(f->root) + 1);
            fprintf(bank, "voice_directsound 60, 0, Sample%d, 255, 252, 0, 115\n", i);
        }
        for (int i = 0; i < PROG_COUNT; i++)
        {
            fprintf(pw, "Prog%d::\n.incbin \"%s\"\n", i, f->prog[i] + strlen(f->root) + 1);
            fprintf(bank, "voice_programmable_wave 60, 0, Prog%d, 5, 2, 15, 3\n", i);
        }
        fprintf(ds,
                "Alias::\n.incbin \"%s\"\nSample0::\n.incbin \"%s\"\n",
                f->pcm[0][2] + strlen(f->root) + 1,
                f->pcm[PCM_COUNT - 1][2] + strlen(f->root) + 1);
        fprintf(pw,
                "Alias::\n.incbin \"%s\"\nProg0::\n.incbin \"%s\"\n",
                f->prog[0] + strlen(f->root) + 1,
                f->prog[PROG_COUNT - 1] + strlen(f->root) + 1);
        fprintf(bank,
                "voice_directsound 60, 0, Sample0, 255, 252, 0, 115\n"
                "voice_directsound 60, 0, Alias, 255, 252, 0, 115\n"
                "voice_programmable_wave 60, 0, Prog0, 5, 2, 15, 3\n"
                "voice_programmable_wave 60, 0, Alias, 5, 2, 15, 3\n");
        ok = !ferror(ds) && !ferror(pw) && !ferror(bank);
    }
    if (ds)
        ok = fclose(ds) == 0 && ok;
    if (pw)
        ok = fclose(pw) == 0 && ok;
    if (bank)
        ok = fclose(bank) == 0 && ok;
    const char square[] = "voice_group square\nvoice_square_1 60, 0, 3, 2, 5, 2, 15, 3\n";
    return write_bytes(f->square, square, sizeof(square) - 1) && ok;
}

/* Remove only the isolated fixture's files, including partial setup output. */
static void remove_fixture(Fixture* f)
{
    for (int i = 0; i < PCM_COUNT; i++)
        for (int format = 0; format < 3; format++)
            if (f->pcm[i][format][0])
                remove(f->pcm[i][format]);
    for (int i = 0; i < PROG_COUNT; i++)
        if (f->prog[i][0])
            remove(f->prog[i]);
    remove(f->bank);
    remove(f->square);
    remove(f->dsMap);
    remove(f->pwMap);
    if (f->sound[0])
        ASSERT(rmdir(f->sound) == 0 || errno == ENOENT, "remove all temporary assembly files");
    if (f->root[0])
        ASSERT(rmdir(f->root) == 0 || errno == ENOENT, "remove all temporary encoded files and fixture root");
}

/* Validate requested paths while preserving the real batch reader ownership contract. */
static bool read_batch(
    void* user, const char* const* paths, size_t count, VoicegroupFileBlob* out, char* error, size_t errorCapacity)
{
    Fixture* f = user;
    f->batches++;
    for (size_t i = 0; i < count; i++)
    {
        bool known = false;
        for (int sample = 0; sample < PCM_COUNT; sample++)
            for (int format = 0; format < 3; format++)
                if (strcmp(paths[i], f->pcm[sample][format]) == 0)
                {
                    f->reads[sample][format]++;
                    known = true;
                }
        for (int sample = 0; sample < PROG_COUNT; sample++)
            if (strcmp(paths[i], f->prog[sample]) == 0)
            {
                f->progReads[sample]++;
                known = true;
            }
        ASSERT(known, "candidate text resolves to an exact fixture path");
        if (!known)
            return false;
        FILE* file = fopen(paths[i], "rb");
        if (!file)
        {
            if (errno == ENOENT)
                continue;
            snprintf(error, errorCapacity, "fixture read failed: %s", paths[i]);
            return false;
        }
        uint8_t bytes[64];
        size_t size = fread(bytes, 1, sizeof(bytes), file);
        bool ok = !ferror(file) && feof(file);
        ok = fclose(file) == 0 && ok;
        if (!ok)
            return false;
        uint8_t* data = malloc(size); /* Adapter allocations deliberately are not engine requests. */
        if (!data)
            return false;
        memcpy(data, bytes, size);
        out[i] = (VoicegroupFileBlob){data, size, true};
        f->populated++;
    }
    return true;
}

/* Track transport cleanup independently of instrumented engine allocations. */
static void release_batch(void* user, VoicegroupFileBlob* blobs, size_t count)
{
    Fixture* f = user;
    for (size_t i = 0; i < count; i++)
    {
        if (blobs[i].data)
        {
            free(blobs[i].data);
            f->released++;
        }
        blobs[i] = (VoicegroupFileBlob){0};
    }
}

/* Compare outstanding ownership, not allocator traffic that may grow geometrically. */
static void assert_ownership(NativeBudget expected)
{
    NativeBudget actual = native_budget_snapshot();
    ASSERT(actual.liveAllocations == expected.liveAllocations, "all transient engine allocations reclaimed");
    ASSERT(actual.liveBytes == expected.liveBytes, "all transient engine bytes reclaimed");
}

/* Inspect returned payloads and aliases after their project and session are gone. */
static void check_bank(const LoadedVoiceGroup* bank)
{
    ASSERT(bank != NULL, "complete bank survives project/session teardown");
    if (!bank)
        return;
    ASSERT(bank->waveDataCount == PCM_COUNT && bank->progWaveCount == PROG_COUNT,
           "bank owns exactly one decoded allocation per distinct resolved path");
    for (int i = 0; i < PCM_COUNT; i++)
    {
        const WaveData* wave = bank->voices[i].wav;
        int8_t expected[4] = {(int8_t)(i + 1), (int8_t)(i + 2), (int8_t)(i + 3), (int8_t)(i + 4)};
        ASSERT(wave && wave->data && wave->size == 4 && memcmp(wave->data, expected, 4) == 0,
               "each stable PCM candidate index selects its own decoded bytes");
        if (wave)
            ASSERT(wave->freq == (i % 2 ? 16384u : 22050u) * 1024u && wave->status == (i % 2 ? 0x4000 : 0) &&
                       wave->loopStart == (i % 2 ? 1u : 0u),
                   "sparse fallback retains frequency and loop metadata");
    }
    for (int i = 0; i < PROG_COUNT; i++)
    {
        uint8_t expected[16];
        prog_bytes(i, expected);
        ASSERT(bank->voices[PCM_COUNT + i].wavePointer &&
                   memcmp(bank->voices[PCM_COUNT + i].wavePointer, expected, sizeof(expected)) == 0,
               "each programmable candidate retains its own packed data");
    }
    ASSERT(bank->voices[ALIAS_SLOT].wav == bank->voices[0].wav &&
               bank->voices[ALIAS_SLOT + 1].wav == bank->voices[0].wav,
           "duplicate symbols and distinct aliases share PCM ownership");
    ASSERT(bank->voices[ALIAS_SLOT + 2].wavePointer == bank->voices[PCM_COUNT].wavePointer &&
               bank->voices[ALIAS_SLOT + 3].wavePointer == bank->voices[PCM_COUNT].wavePointer,
           "duplicate symbols and distinct aliases share programmable ownership");
}

/* Keep discovery requests outside each measured or fault-injected bank load. */
static VoicegroupProject* open_project(Fixture* f, const VoicegroupFileIo* io)
{
    native_budget_begin(0);
    VoicegroupProject* project = voicegroup_project_open(f->root, NULL, io);
    ASSERT(project != NULL, "open project before measuring/injecting bank requests");
    return project;
}

/* Reject every discovery allocation failure without publishing partial context. */
static void test_project_open_failures(Fixture* f, const VoicegroupFileIo* io)
{
    VoicegroupProject* project = open_project(f, io);
    if (!project)
        return;
    size_t requests = native_budget_snapshot().requests;
    voicegroup_project_free(project);
    assert_ownership((NativeBudget){0});
    for (size_t failure = 1; failure <= requests; failure++)
    {
        native_budget_begin(failure);
        project = voicegroup_project_open(f->root, NULL, io);
        ASSERT(native_budget_snapshot().injectedFailures == 1, "reach each project-open allocation request");
        ASSERT(project == NULL, "failed discovery or compact map growth publishes no partial context");
        voicegroup_project_free(project);
        assert_ownership((NativeBudget){0});
    }
    native_budget_begin(0);
    printf("Swept %zu project-open allocation failures\n", requests);
}

/* Gate the empty-sample fast path and exclude per-candidate allocation policy. */
static size_t test_public_loads(Fixture* f, const VoicegroupFileIo* io)
{
    VoicegroupProject* project = open_project(f, io);
    if (!project)
        return 0;
    NativeBudget opened = native_budget_snapshot();
    printf("Project-open engine requests: %zu (excluded from bank budget)\n", opened.requests);
    VoicegroupTarget target = {f->square, NULL};
    native_budget_begin(0);
    LoadedVoiceGroup* bank = voicegroup_project_load(project, &target);
    ASSERT(native_budget_snapshot().requests == 1, "square-only bank needs exactly its owner allocation");
    ASSERT(f->batches == 0 && f->populated == 0, "square-only bank performs zero sample IO");
    ASSERT(bank && bank->voices[0].type == VOICE_SQUARE_1 && bank->voices[0].key == 60 &&
               bank->voices[0].panSweep == 3 && (uintptr_t)bank->voices[0].wavePointer == 2 &&
               bank->voices[0].attack == 5 && bank->voices[0].decay == 2 && bank->voices[0].sustain == 15 &&
               bank->voices[0].release == 3 && bank->waveDataCount == 0 && bank->progWaveCount == 0,
           "square-only bank preserves tone content without sample ownership");
    voicegroup_free(bank);
    assert_ownership(opened);
    target.filePath = f->bank;
    native_budget_begin(0);
    bank = voicegroup_project_load(project, &target);
    size_t requests = native_budget_snapshot().requests;
    printf("Growth-bank engine requests: %zu; budget %d; unique candidates %d\n",
           requests,
           LOAD_REQUEST_LIMIT,
           CANDIDATE_COUNT);
    /* Even path allocations alone exceeded this bound under the old policy (232).
     * Leave room for 104 decoded outputs and geometric metadata/text growth. */
    ASSERT(requests <= LOAD_REQUEST_LIMIT, "growth budget excludes one allocation per candidate path");
    ASSERT(f->populated == f->released, "successful transport releases every encoded buffer");
    for (int i = 0; i < PCM_COUNT; i++)
        ASSERT(f->reads[i][0] == 1 && f->reads[i][1] == (size_t)(i % 2) && f->reads[i][2] == (size_t)(i % 2),
               "fallback rounds deduplicate and skip resolved paths");
    for (int i = 0; i < PROG_COUNT; i++)
        ASSERT(f->progReads[i] == 1, "programmable aliases read their shared path once");
    voicegroup_project_free(project);
    check_bank(bank);
    voicegroup_free(bank);
    assert_ownership((NativeBudget){0});
    return requests;
}

/* Every bank allocation must fail atomically and permit retry on the same context. */
static void test_failure_sweep(Fixture* f, const VoicegroupFileIo* io, size_t requests)
{
    VoicegroupTarget target = {f->bank, NULL};
    for (size_t failure = 1; failure <= requests; failure++)
    {
        VoicegroupProject* project = open_project(f, io);
        if (!project)
            return;
        NativeBudget opened = native_budget_snapshot();
        native_budget_begin(failure);
        LoadedVoiceGroup* bank = voicegroup_project_load(project, &target);
        ASSERT(native_budget_snapshot().injectedFailures == 1, "sweep reaches each real bank allocation request");
        ASSERT(bank == NULL, "allocation failure never publishes a partially loaded bank");
        voicegroup_free(bank);
        assert_ownership(opened);
        ASSERT(f->populated == f->released, "failure releases even previously populated transport buffers");
        native_budget_begin(0);
        bank = voicegroup_project_load(project, &target);
        voicegroup_project_free(project);
        check_bank(bank);
        voicegroup_free(bank);
        ASSERT(f->populated == f->released, "same-context retry releases all transport buffers");
        assert_ownership((NativeBudget){0});
    }
    printf("Swept %zu bank allocation failures with same-context retries\n", requests);
}

/* Expand one fixture sample into its ordered decoder candidates. */
static bool plan_wave(VgLoadSession* session, Fixture* f, WaveData** slot, int index)
{
    return vg_load_session_add_wave(session, slot, f->pcm[index][0], f->pcm[index][1], f->pcm[index][2]);
}

/* The public parser only rolls back when a subgroup hard-fails, which aborts
 * the whole bank. Reappend is therefore tested at the real planning seam. */
static void test_rollback(Fixture* f, const VoicegroupFileIo* io)
{
    native_budget_begin(0);
    LoadedVoiceGroup* bank = poryaaaa_budget_calloc(1, sizeof(*bank));
    ASSERT(bank != NULL, "allocate checkpoint test owner");
    if (!bank)
        return;
    VgLoadSession session;
    WaveCache cache;
    wave_cache_init(&cache);
    vg_load_session_init(&session, io, bank, &cache);
    WaveData* discardedPcm[PCM_COUNT] = {0};
    uint32_t* discardedProg[PROG_COUNT] = {0};
    bool ok = true;
    for (int i = 0; ok && i < 8; i++)
        ok = plan_wave(&session, f, &bank->voices[i].wav, i) &&
             vg_load_session_add_prog(&session, &bank->voices[PCM_COUNT + i].wavePointer, f->prog[i]);
    VgLoadSessionCheckpoint checkpoint = vg_load_session_checkpoint(&session);
    for (int i = 8; ok && i < PCM_COUNT; i++)
        ok = plan_wave(&session, f, &discardedPcm[i], i);
    for (int i = 8; ok && i < PROG_COUNT; i++)
        ok = vg_load_session_add_prog(&session, &discardedProg[i], f->prog[i]);
    vg_load_session_rollback(&session, checkpoint);
    for (int i = PCM_COUNT - 1; ok && i >= 8; i--)
        ok = plan_wave(&session, f, &bank->voices[i].wav, i);
    for (int i = PROG_COUNT - 1; ok && i >= 8; i--)
        ok = vg_load_session_add_prog(&session, &bank->voices[PCM_COUNT + i].wavePointer, f->prog[i]);
    for (int i = 0; ok && i < 2; i++)
        ok = plan_wave(&session, f, &bank->voices[ALIAS_SLOT + i].wav, 0) &&
             vg_load_session_add_prog(&session, &bank->voices[ALIAS_SLOT + 2 + i].wavePointer, f->prog[0]);
    /* Multiple candidates for one destination must preserve its first success. */
    ok = ok && vg_load_session_add_prog(&session, &bank->voices[PCM_COUNT].wavePointer, f->prog[1]);
    ASSERT(ok, "plan retained, removed, reverse-reappended, and duplicate candidates");
    if (ok)
        ASSERT(vg_load_session_execute(&session), "execute rolled-back and reappended candidate plan");
    vg_load_session_deinit(&session);
    check_bank(bank);
    for (int i = 0; i < PCM_COUNT; i++)
        ASSERT(discardedPcm[i] == NULL, "rolled-back PCM destinations are never written");
    for (int i = 0; i < PROG_COUNT; i++)
        ASSERT(discardedProg[i] == NULL, "rolled-back programmable destinations are never written");
    voicegroup_free(bank);
    ASSERT(f->populated == f->released, "rollback scenario releases every encoded buffer");
    assert_ownership((NativeBudget){0});
}

/* Run isolated ownership, budget, and rollback regressions through one allocator ledger. */
int main(void)
{
    Fixture fixture = {0};
    bool ready = make_fixture(&fixture);
    ASSERT(ready, "create isolated assembly and encoded asset fixtures");
    if (ready)
    {
        VoicegroupFileIo io = {&fixture, read_batch, release_batch};
        test_project_open_failures(&fixture, &io);
        size_t requests = test_public_loads(&fixture, &io);
        ASSERT(requests > 0, "successful growth load supplies failure sweep extent");
        test_failure_sweep(&fixture, &io, requests);
        test_rollback(&fixture, &io);
    }
    remove_fixture(&fixture);
    printf("Native allocation tests: %d/%d assertions passed\n", tests_passed, tests_run);
    return tests_passed == tests_run ? 0 : 1;
}
