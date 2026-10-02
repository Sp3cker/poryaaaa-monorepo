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

    WaveData* vg_asset_decode_wav(const uint8_t* data, size_t size, const char* debugPath, bool* hardFailure);
    WaveData* vg_asset_decode_aiff(const uint8_t* data, size_t size, const char* debugPath, bool* hardFailure);
    WaveData* vg_asset_decode_bin(const uint8_t* data, size_t size, const char* debugPath, bool* hardFailure);
    uint32_t* vg_asset_decode_prog(const uint8_t* data, size_t size, const char* debugPath, bool* hardFailure);

    /* Serial file helpers that read the whole file then delegate to the decoders.
     * hardFailure distinguishes missing/invalid files from allocation or I/O failure. */
    WaveData* vg_asset_load_wav_file(const char* absolutePath, bool* hardFailure);
    WaveData* vg_asset_load_aiff_file(const char* absolutePath, bool* hardFailure);

    /* ---- Generic batch helpers ---- */

    /* Append-only candidate indices; offsets survive text-buffer growth. */
    typedef struct
    {
        size_t* offsets;
        char* text;
        size_t count;
        size_t capacity;
        size_t textSize;
        size_t textCapacity;
    } VgDedup;

    void vg_dedup_init(VgDedup* d);
    void vg_dedup_deinit(VgDedup* d);
    /* Returns the existing/new index, or -1 on failure without adding an entry. */
    int vg_dedup_add(VgDedup* d, const char* path);
    /* Borrowed until the next add/truncate/deinit; NULL for an invalid index. */
    const char* vg_dedup_path(const VgDedup* d, size_t index);
    /* Drops the suffix and rewinds its text, retaining capacity for later planning. */
    void vg_dedup_truncate(VgDedup* d, size_t newCount);

    /*
     * Read a borrowed path span through the adapter; no path ownership transfers.
     * - Zeroes every out blob and validates the span, adapter, and count.
     * - Calls io->readBatch with exactly these paths.
     * - On hard failure, leaves every populated blob for the caller to release and
     *   returns false (error optionally filled). On soft miss/success returns true.
     * Caller must release every blob via vg_batch_release regardless of outcome.
     */
    bool vg_batch_read(const VoicegroupFileIo* io,
                       const char* const* paths,
                       size_t count,
                       VoicegroupFileBlob* outBlobs,
                       char* error,
                       size_t errorCapacity);
    void vg_batch_release(const VoicegroupFileIo* io, VoicegroupFileBlob* blobs, size_t count);

#ifdef __cplusplus
}
#endif

#endif /* VOICEGROUP_ASSET_BATCH_H */
