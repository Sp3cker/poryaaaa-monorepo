#ifndef VOICEGROUP_ASSET_BATCH_H
#define VOICEGROUP_ASSET_BATCH_H

#include "voicegroup_loader.h"

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C"
{
#endif

    /* ---- Complete-byte-span decoders (single implementation) ----
     * hardFailure distinguishes allocation/size failure from malformed input. */

    WaveData* vg_asset_decode_wav(
        const uint8_t* data, size_t size, const char* debugPath, bool* hardFailure);
    WaveData* vg_asset_decode_aiff(
        const uint8_t* data, size_t size, const char* debugPath, bool* hardFailure);
    WaveData* vg_asset_decode_bin(
        const uint8_t* data, size_t size, const char* debugPath, bool* hardFailure);
    uint32_t* vg_asset_decode_prog(
        const uint8_t* data, size_t size, const char* debugPath, bool* hardFailure);

    /* Serial file helpers that read the whole file then delegate to the decoders.
     * hardFailure distinguishes missing/invalid files from allocation or I/O failure. */
    WaveData* vg_asset_load_wav_file(const char* absolutePath, bool* hardFailure);
    WaveData* vg_asset_load_aiff_file(const char* absolutePath, bool* hardFailure);

    /* ---- Generic batch helpers ---- */

    typedef struct
    {
        char** paths;
        size_t count;
        size_t capacity;
    } VgDedup;

    void vg_dedup_init(VgDedup* d);
    void vg_dedup_deinit(VgDedup* d);
    bool vg_dedup_add(VgDedup* d, const char* path);
    int vg_dedup_find(const VgDedup* d, const char* path);
    bool vg_dedup_contains(const VgDedup* d, const char* path);

    /*
     * Execute one deduped round through the adapter.
     * - Zeroes every out blob, guards null/size/error buffers and overflow.
     * - Calls io->readBatch with the deduped paths.
     * - On hard failure, leaves every populated blob for the caller to release and
     *   returns false (error optionally filled). On soft miss/success returns true.
     * Caller must release every blob via vg_batch_release regardless of outcome.
     */
    bool vg_batch_read(const VoicegroupFileIo* io,
                       const VgDedup* dedup,
                       VoicegroupFileBlob* outBlobs,
                       char* error,
                       size_t errorCapacity);
    void vg_batch_release(const VoicegroupFileIo* io, VoicegroupFileBlob* blobs, size_t count);

#ifdef __cplusplus
}
#endif

#endif /* VOICEGROUP_ASSET_BATCH_H */
