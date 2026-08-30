#ifndef VOICEGROUP_LOADER_H
#define VOICEGROUP_LOADER_H

#include "voicegroup/voicegroup_types.h"
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#define VOICEGROUP_SIZE 128
#define VG_MAX_PATH_LEN 512
#define VG_VOICE_NAME_LEN 48
#define VG_CONFIG_PATH_CAP 8
#define VG_MAX_SYMBOL_LEN 256
#ifndef MAX_SYMBOL_LEN
#    define MAX_SYMBOL_LEN VG_MAX_SYMBOL_LEN
#endif

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Optional configuration for the voicegroup loader.
 * All paths are relative to the project root directory.
 * Zero-initialized config means "auto-discover everything".
 */
typedef struct
{
    char soundDataPaths[VG_CONFIG_PATH_CAP][VG_MAX_PATH_LEN]; /* extra .inc files with sample symbol definitions */
    int soundDataPathCount;
    char voicegroupPaths[VG_CONFIG_PATH_CAP][VG_MAX_PATH_LEN]; /* extra voicegroup directories or files */
    int voicegroupPathCount;
    char sampleDirs[VG_CONFIG_PATH_CAP][VG_MAX_PATH_LEN]; /* extra directories with .wav sample files */
    int sampleDirCount;
} VoicegroupLoaderConfig;

/*
 * Loaded voicegroup data - holds all allocated resources.
 * Must be freed with voicegroup_free() when done.
 */
typedef struct
{
    ToneData voices[VOICEGROUP_SIZE];

    /* Human-readable name per top-level voice, taken from the symbol on the
     * voice's line in the voicegroup file (sample symbol for DirectSound
     * voices, wave symbol for programmable-wave voices, sub-voicegroup symbol
     * for keysplits/drumkits), with common prefixes stripped.  Empty string
     * when no meaningful symbol exists (e.g. square/noise voices). */
    char voiceNames[VOICEGROUP_SIZE][VG_VOICE_NAME_LEN];

    /* Loaded wave data (samples) */
    WaveData** waveDatas;
    int waveDataCount;
    int waveDataCapacity;

    /* Loaded programmable wave data */
    uint32_t** progWaves;
    int progWaveCount;
    int progWaveCapacity;

    /* Sub-voicegroups (keysplits, drumsets) */
    ToneData** subGroups;
    int subGroupCount;
    int subGroupCapacity;

    /* Keysplit tables */
    uint8_t** keySplitTables;
    int keySplitTableCount;
    int keySplitTableCapacity;
} LoadedVoiceGroup;

/*
 * Load a voicegroup from a project.
 *
 * projectRoot: path to the project root directory
 * voicegroupName: name of the voicegroup (e.g. "petalburg", "voicegroup000")
 * config: optional loader configuration (NULL for pure auto-discovery)
 *
 * The loader auto-discovers project structure (pokeemerald, pokefirered,
 * and forks with custom sound directories). Config overrides can
 * specify additional search paths.
 *
 * Returns a LoadedVoiceGroup on success, or NULL on failure.
 * The caller must free the result with voicegroup_free().
 */
LoadedVoiceGroup*
voicegroup_load(const char* projectRoot, const char* voicegroupName, const VoicegroupLoaderConfig* config);

/*
 * Free all resources associated with a loaded voicegroup.
 */
void voicegroup_free(LoadedVoiceGroup* vg);

/*
 * A keysplit instrument loaded by symbol: the sub-voicegroup's ToneData
 * array (VOICEGROUP_SIZE entries) and the 128-byte key-to-index table,
 * exactly as a voice_keysplit line would resolve them. Either pointer is
 * NULL when its symbol didn't resolve.
 */
typedef struct
{
    ToneData* subGroup;
    uint8_t* table;
} LoadedKeysplit;

/*
 * A batch of instruments loaded by symbol, outside any voicegroup — the
 * same data the voicegroup loader would hand the engine (identical
 * resolution and header math). Each array is parallel to the corresponding
 * symbol list from the load call, with NULL entries where a symbol didn't
 * resolve. All memory is owned by the set; free with
 * voicegroup_free_samples().
 */
typedef struct
{
    WaveData** waves; /* DirectSound samples */
    int count;
    uint32_t** progWaves; /* programmable waves (16 packed bytes each) */
    int progWaveCount;
    LoadedKeysplit* keysplits;
    int keysplitCount;
    LoadedVoiceGroup* container; /* internal ownership holder */
} LoadedSampleSet;

/*
 * Load DirectSound samples, programmable waves, and keysplit instruments by
 * symbol name, sharing one project discovery and sound-data parse across
 * the whole batch. keysplitTableSymbols pairs with keysplitSymbols (the two
 * symbols of a voice_keysplit line). Symbols that fail to resolve get a
 * NULL entry rather than failing the batch. Any list may be empty.
 *
 * Returns NULL only on allocation failure.
 */
LoadedSampleSet* voicegroup_load_samples(const char* projectRoot,
                                         const char* const* sampleSymbols,
                                         int sampleCount,
                                         const char* const* waveSymbols,
                                         int waveCount,
                                         const char* const* keysplitSymbols,
                                         const char* const* keysplitTableSymbols,
                                         int keysplitCount,
                                         const VoicegroupLoaderConfig* config);

void voicegroup_free_samples(LoadedSampleSet* set);

/*
 * Set an optional file path for diagnostic logging inside the voicegroup loader.
 * Pass NULL to disable. The same path used by the plugin's "log=" config key works.
 * Call before voicegroup_load() for the output to be useful.
 */
void voicegroup_loader_set_log_path(const char* path);

/*
 * Project-scoped loading context.
 *
 * A VoicegroupProject owns one project's discovery state plus the parsed
 * DirectSound, programmable-wave, and keysplit maps. Building it once and
 * loading many voicegroups from it skips repeated discovery and global-map
 * parsing. The context is worker-confined: never call it concurrently from
 * more than one thread.
 *
 * Mapped sample and programmable-wave asset reads go through the
 * caller-supplied VoicegroupFileIo adapter in deduplicated batches;
 * directory enumeration, discovery, voicegroup text, and global map reads
 * remain internal. The recursive sound/ deep scan for nonstandard project
 * layouts stays lazy and runs at most once per context. Banks and sample
 * sets returned by the context are fully self-contained and stay valid after
 * the context is freed; free them with voicegroup_free()/voicegroup_free_samples().
 */
typedef struct VoicegroupProject VoicegroupProject;

/*
 * An exact voicegroup location. filePath is used verbatim as the file to
 * parse (pass an absolute path); find_voicegroup/alias probing is skipped
 * and the target is attempted exactly once. sectionLabel pins a section
 * inside a monolithic voicegroup file; NULL or empty means the whole file.
 */
typedef struct
{
    const char* filePath;
    const char* sectionLabel;
} VoicegroupTarget;

/*
 * One file read produced by the adapter. data/size hold the whole file
 * content (malloc'd by the adapter); found is 0 for a soft miss (file does
 * not exist), which callers treat as an empty result, not an error.
 */
typedef struct
{
    uint8_t* data;
    size_t size;
    bool found;
} VoicegroupFileBlob;

/*
 * Batch read callback. Performs the count independent file reads and fills
 * every out[i] (entries not read must stay zeroed). Return true when the
 * transport itself worked; return false only on a hard adapter failure
 * (allocation or I/O transport error, described in error). The loader
 * releases every out entry after either outcome, so a failing batch may
 * leave earlier entries populated.
 *
 * releaseBatch frees the data of every populated blob in the array. Paths
 * handed to readBatch are independent: no entry depends on another, and the
 * loader parses/decodes only after the whole batch returns.
 */
typedef bool (*VoicegroupReadBatchFn)(
    void* user, const char* const* paths, size_t count, VoicegroupFileBlob* out, char* error, size_t errorCapacity);

typedef void (*VoicegroupReleaseBatchFn)(void* user, VoicegroupFileBlob* blobs, size_t count);

typedef struct
{
    void* user;
    VoicegroupReadBatchFn readBatch;
    VoicegroupReleaseBatchFn releaseBatch;
} VoicegroupFileIo;

/*
 * Open a project context: discover the project layout and parse the global
 * DirectSound/programmable-wave/keysplit maps. projectRoot and config (when
 * non-NULL; NULL means pure auto-discovery, same as voicegroup_load) and the
 * adapter are copied into the context. Discovery and global-map parsing use
 * internal stdio; the adapter is only used later for deduplicated asset
 * batches. Returns NULL on bad arguments or allocation failure.
 */
VoicegroupProject* voicegroup_project_open(const char* projectRoot,
                                           const VoicegroupLoaderConfig* config,
                                           const VoicegroupFileIo* fileIo);

/*
 * Load the exact voicegroup named by target. The file at filePath is used
 * verbatim exactly once; a missing target file or a missing requested
 * section (when sectionLabel is non-NULL and not present) is a hard failure
 * and returns NULL. Mapped asset file misses are soft as in voicegroup_load
 * (unresolved voices keep NULL samples); only allocation or adapter hard
 * failure returns NULL. The project remains reusable after a hard failure;
 * the returned bank is self-contained or NULL.
 */
LoadedVoiceGroup* voicegroup_project_load(VoicegroupProject* project, const VoicegroupTarget* target);

/*
 * Context-based counterpart of voicegroup_load_samples: same symbol
 * resolution and results, reusing the context's discovery and maps.
 * Returns a self-contained LoadedSampleSet or NULL.
 */
LoadedSampleSet* voicegroup_project_load_samples(VoicegroupProject* project,
                                                 const char* const* sampleSymbols,
                                                 int sampleCount,
                                                 const char* const* waveSymbols,
                                                 int waveCount,
                                                 const char* const* keysplitSymbols,
                                                 const char* const* keysplitTableSymbols,
                                                 int keysplitCount);

/* Free the context. Loaded banks/sample sets remain valid. */
void voicegroup_project_free(VoicegroupProject* project);

#ifdef __cplusplus
} /* extern "C" */
#endif
#endif /* VOICEGROUP_LOADER_H */
