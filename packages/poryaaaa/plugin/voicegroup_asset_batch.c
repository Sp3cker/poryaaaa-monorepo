#include "voicegroup_asset_batch.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <math.h>
#include <limits.h>

#ifndef VG_MAX_PATH_LEN
#    define VG_MAX_PATH_LEN 512
#endif

/* ---- helpers ---- */

static uint32_t read_u32_le(const uint8_t* p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}
static uint16_t read_u16_le(const uint8_t* p)
{
    return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}
static uint32_t read_u32_be(const uint8_t* p)
{
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3];
}
static uint16_t read_u16_be(const uint8_t* p)
{
    return (uint16_t)((p[0] << 8) | p[1]);
}

/* 80-bit extended float, big-endian (AIFF COMM sampleRate). */
static double read_extended80(const uint8_t* b)
{
    int sign = (b[0] & 0x80) ? -1 : 1;
    int exponent = ((b[0] & 0x7F) << 8) | b[1];
    uint64_t mantissa = 0;
    for (int i = 0; i < 8; i++)
        mantissa = (mantissa << 8) | b[2 + i];
    if (exponent == 0 && mantissa == 0)
        return 0.0;
    return sign * ldexp((double)mantissa, exponent - 16383 - 63);
}

static WaveData* alloc_wavedata(uint32_t size)
{
#if SIZE_MAX <= UINT32_MAX
    if (size > (uint32_t)(SIZE_MAX - sizeof(WaveData) - 1))
        return NULL;
#endif
    size_t need = sizeof(WaveData) + (size_t)size + 1;
    WaveData* wd = (WaveData*)malloc(need);
    if (!wd)
        return NULL;
    wd->data = (int8_t*)((uint8_t*)wd + sizeof(WaveData));
    wd->data[size] = 0;
    return wd;
}

/* Integer PCM: keep the most significant byte of each supported width. */
static int8_t convert_sample_int(const uint8_t* sp, uint32_t bps)
{
    if (bps == 1)
        return (int8_t)((int)sp[0] - 128);
    if (bps == 2)
    {
        int16_t v = (int16_t)(read_u16_le(sp));
        return (int8_t)(v >> 8);
    }
    if (bps == 3)
    {
        uint32_t raw = (uint32_t)sp[0] | ((uint32_t)sp[1] << 8) | ((uint32_t)sp[2] << 16);
        int32_t v = (raw & 0x800000u) ? (int32_t)(raw | 0xFF000000u) : (int32_t)raw;
        return (int8_t)(v >> 16);
    }
    if (bps == 4)
    {
        int32_t v = (int32_t)read_u32_le(sp);
        return (int8_t)(v >> 24);
    }
    return 0;
}

/* Non-finite doubles only reach here as +-infinity; NaN collapses to silence. */
static int8_t saturate_sample(double v)
{
    if (!isinf(v))
        return 0;
    return (v > 0.0) ? 127 : -128;
}

static int8_t convert_sample_float_value(double scaled)
{
    if (!isfinite(scaled))
        return saturate_sample(scaled);
    double floored = floor(scaled);
    if (!isfinite(floored))
        return saturate_sample(floored);
    if (floored < -128.0)
        return -128;
    if (floored > 127.0)
        return 127;
    return (int8_t)(int)floored;
}

/* IEEE float WAV: 32-bit or 64-bit little-endian, scaled and clamped to int8. */
static int8_t convert_sample_float(const uint8_t* sp, uint32_t bps)
{
    double ds;
    if (bps == 4)
    {
        uint32_t bits = read_u32_le(sp);
        float fv;
        memcpy(&fv, &bits, sizeof(fv));
        if (!isfinite(fv))
            return 0;
        ds = (double)fv;
    }
    else
    {
        uint64_t bits = (uint64_t)read_u32_le(sp) | ((uint64_t)read_u32_le(sp + 4) << 32);
        double dv;
        memcpy(&dv, &bits, sizeof(dv));
        if (!isfinite(dv))
            return 0;
        ds = dv;
    }
    return convert_sample_float_value(ds * 128.0);
}

static int8_t convert_sample(const uint8_t* sp, int fmtTag, uint32_t bps)
{
    if (fmtTag == 1)
        return convert_sample_int(sp, bps);
    return convert_sample_float(sp, bps);
}

/* Rate*1024 clamped into uint32; out-of-range or NaN rates collapse to 0. */
static uint32_t clamp_freq(double d)
{
    if (!isfinite(d) || d < 0.0)
        return 0;
    if (d > (double)UINT32_MAX)
        return UINT32_MAX;
    return (uint32_t)d;
}

/* Header fields shared by the RIFF and AIFF decoders; the tail lands after materialization. */
static void wavedata_init(WaveData* wd, uint32_t freq, uint32_t loopStart, uint32_t wsize, int loopEnabled)
{
    wd->type = 0;
    wd->status = loopEnabled ? 0x4000 : 0;
    wd->freq = freq;
    wd->loopStart = loopStart;
    wd->size = wsize;
}

/* ---- WAV decoder (memory) ---- */

/* Stack view of the RIFF chunks seen during the scan. */
typedef struct
{
    int fmtFound, dataFound;
    int fmtTag;
    uint32_t sampleRate;
    uint16_t blockAlign, bitsPerSample;
    uint32_t midiKey, midiPitchFraction;
    uint32_t smplLoopStart, smplLoopEnd;
    int loopEnabled;
    uint32_t agbPitch, agbLoopEnd;
    size_t dataOffset;
    uint32_t dataLen;
} WavChunkInfo;

static void wav_report_invalid_header(const char* debugPath)
{
    if (debugPath)
        fprintf(stderr, "voicegroup_loader: invalid RIFF/WAVE header in %s\n", debugPath);
}

static void wav_parse_fmt(const uint8_t* d, WavChunkInfo* info)
{
    info->fmtTag = d[0] | (d[1] << 8);
    info->sampleRate = read_u32_le(d + 4);
    info->blockAlign = read_u16_le(d + 12);
    info->bitsPerSample = read_u16_le(d + 14);
    info->fmtFound = 1;
}

/* smpl chunk: midi keynote/fraction plus the first loop definition, inclusive end. */
static void wav_parse_smpl(
    const uint8_t* data, uint32_t chunkLen, size_t chunkDataStart, size_t chunkDataEnd, size_t size, WavChunkInfo* info)
{
    if (chunkLen < 32 || size - chunkDataStart < 32)
        return;
    size_t avail = chunkDataEnd - chunkDataStart;
    size_t readLen = avail < 52 ? avail : 52;
    if (readLen < 32)
        return;
    const uint8_t* d = data + chunkDataStart;
    info->midiKey = read_u32_le(d + 12);
    if (info->midiKey > 127)
        info->midiKey = 127;
    info->midiPitchFraction = read_u32_le(d + 16);
    uint32_t numLoops = read_u32_le(d + 28);
    if (numLoops == 1 && readLen >= 52)
    {
        info->smplLoopStart = read_u32_le(d + 44);
        uint32_t loopEndIncl = read_u32_le(d + 48);
        info->smplLoopEnd = loopEndIncl + 1;
        info->loopEnabled = 1;
    }
}

/* One RIFF chunk; availability checks use the full buffer, walking uses the limit. */
static void wav_parse_chunk(const uint8_t* data,
                            size_t size,
                            const uint8_t* hdr,
                            uint32_t chunkLen,
                            size_t chunkDataStart,
                            size_t chunkDataEnd,
                            WavChunkInfo* info)
{
    if (memcmp(hdr, "fmt ", 4) == 0)
    {
        if (chunkLen >= 16 && size - chunkDataStart >= 16)
            wav_parse_fmt(data + chunkDataStart, info);
    }
    else if (memcmp(hdr, "smpl", 4) == 0)
    {
        wav_parse_smpl(data, chunkLen, chunkDataStart, chunkDataEnd, size, info);
    }
    else if (memcmp(hdr, "agbp", 4) == 0)
    {
        if (chunkLen >= 4 && size - chunkDataStart >= 4)
            info->agbPitch = read_u32_le(data + chunkDataStart);
    }
    else if (memcmp(hdr, "agbl", 4) == 0)
    {
        if (chunkLen >= 4 && size - chunkDataStart >= 4)
            info->agbLoopEnd = read_u32_le(data + chunkDataStart);
    }
    else if (memcmp(hdr, "data", 4) == 0)
    {
        info->dataOffset = chunkDataStart;
        info->dataLen = chunkLen;
        info->dataFound = 1;
    }
}

static void wav_scan_chunks(const uint8_t* data, size_t size, size_t limit, WavChunkInfo* info)
{
    size_t offset = 12;
    while (offset <= limit && limit - offset >= 8)
    {
        const uint8_t* hdr = data + offset;
        uint32_t chunkLen = read_u32_le(hdr + 4);
        size_t chunkDataStart = offset + 8;
        if ((size_t)chunkLen > limit - chunkDataStart)
            break;
        size_t chunkDataEnd = chunkDataStart + (size_t)chunkLen;
        wav_parse_chunk(data, size, hdr, chunkLen, chunkDataStart, chunkDataEnd, info);
        size_t nextChunk = chunkDataEnd;
        if (chunkLen & 1)
        {
            if (nextChunk >= limit)
                break;
            nextChunk += 1;
        }
        if (nextChunk <= offset)
            break;
        offset = nextChunk;
    }
}

static uint32_t wav_pcm_bytes_per_sample(uint16_t blockAlign, uint16_t bitsPerSample, const char* debugPath)
{
    if (blockAlign == 1 && bitsPerSample == 8)
        return 1;
    if (blockAlign == 2 && bitsPerSample == 16)
        return 2;
    if (blockAlign == 3 && bitsPerSample == 24)
        return 3;
    if (blockAlign == 4 && bitsPerSample == 32)
        return 4;
    if (debugPath)
        fprintf(stderr, "voicegroup_loader: unsupported integer PCM format in %s\n", debugPath);
    return 0;
}

static uint32_t wav_float_bytes_per_sample(uint16_t blockAlign, uint16_t bitsPerSample, const char* debugPath)
{
    if (blockAlign == 4 && bitsPerSample == 32)
        return 4;
    if (blockAlign == 8 && bitsPerSample == 64)
        return 8;
    if (debugPath)
        fprintf(stderr, "voicegroup_loader: unsupported float format in %s\n", debugPath);
    return 0;
}

/* Returns 0 for unsupported formats; the message names the rejected family. */
static uint32_t
wav_format_bytes_per_sample(int fmtTag, uint16_t blockAlign, uint16_t bitsPerSample, const char* debugPath)
{
    if (fmtTag == 1)
        return wav_pcm_bytes_per_sample(blockAlign, bitsPerSample, debugPath);
    if (fmtTag == 3)
        return wav_float_bytes_per_sample(blockAlign, bitsPerSample, debugPath);
    if (debugPath)
        fprintf(stderr, "voicegroup_loader: unsupported audio format %d in %s\n", fmtTag, debugPath);
    return 0;
}

/* smpl loop wins over the open length, agbl wins over both; ends clamp to numSamples. */
static uint32_t wav_resolve_loop_end(const WavChunkInfo* info, uint32_t numSamples)
{
    uint32_t loopEnd = info->loopEnabled ? info->smplLoopEnd : numSamples;
    if (loopEnd > numSamples)
        loopEnd = numSamples;
    if (info->agbLoopEnd != 0)
    {
        uint32_t agbEnd = info->agbLoopEnd;
        if (agbEnd > numSamples)
            agbEnd = numSamples;
        loopEnd = agbEnd;
    }
    return loopEnd;
}

/* agbp overrides, then keynote 60 with no fraction uses the raw rate, else MIDI retuning. */
static uint32_t wav_resolve_freq(const WavChunkInfo* info, uint32_t sampleRate)
{
    if (info->agbPitch != 0)
        return info->agbPitch;
    if (info->midiKey == 60 && info->midiPitchFraction == 0)
        return clamp_freq((double)sampleRate * 1024.0);
    double tuning = (double)info->midiPitchFraction / (4294967296.0 * 100.0);
    double pitch = (double)sampleRate * pow(2.0, (60.0 - (double)info->midiKey) / 12.0 + tuning / 1200.0);
    return clamp_freq(pitch * 1024.0);
}

static void wav_materialize_samples(
    WaveData* wd, const uint8_t* data, size_t size, const WavChunkInfo* info, uint32_t bytesPerSample, uint32_t wsize)
{
    size_t dataEnd = (size_t)info->dataOffset + info->dataLen;
    for (uint32_t i = 0; i < wsize; i++)
    {
        size_t off = info->dataOffset + (size_t)i * bytesPerSample;
        if (off + bytesPerSample <= dataEnd && off + bytesPerSample <= size)
            wd->data[i] = convert_sample(data + off, info->fmtTag, bytesPerSample);
        else
            wd->data[i] = 0;
    }
    wd->data[wsize] = (wsize > 0) ? wd->data[wsize - 1] : 0;
}

WaveData* vg_asset_decode_wav(const uint8_t* data, size_t size, const char* debugPath, bool* hardFailure)
{
    if (hardFailure)
        *hardFailure = false;
    if (!data || size < 12)
    {
        wav_report_invalid_header(debugPath);
        return NULL;
    }
    if (memcmp(data, "RIFF", 4) != 0 || memcmp(data + 8, "WAVE", 4) != 0)
    {
        wav_report_invalid_header(debugPath);
        return NULL;
    }
    uint32_t riffSize = read_u32_le(data + 4);
    size_t fileEnd = size;
    uint64_t claimed = UINT64_C(8) + riffSize;
    if (claimed < (uint64_t)fileEnd)
        fileEnd = (size_t)claimed;

    WavChunkInfo info = {0};
    info.midiKey = 60;
    wav_scan_chunks(data, size, fileEnd, &info);
    if (!info.fmtFound || !info.dataFound)
    {
        if (debugPath)
            fprintf(stderr, "voicegroup_loader: missing fmt or data chunk in %s\n", debugPath);
        return NULL;
    }

    uint32_t bytesPerSample = wav_format_bytes_per_sample(info.fmtTag, info.blockAlign, info.bitsPerSample, debugPath);
    if (bytesPerSample == 0)
        return NULL;
    uint32_t numSamples = info.dataLen / bytesPerSample;
    uint32_t wsize = wav_resolve_loop_end(&info, numSamples);
    uint32_t freq = wav_resolve_freq(&info, info.sampleRate);

    WaveData* wd = alloc_wavedata(wsize);
    if (!wd)
    {
        if (hardFailure)
            *hardFailure = true;
        return NULL;
    }
    wavedata_init(wd, freq, info.smplLoopStart, wsize, info.loopEnabled);
    wav_materialize_samples(wd, data, size, &info, bytesPerSample, wsize);
    return wd;
}

/* ---- AIFF decoder (memory) ---- */

/* One parsed MARK entry: marker id plus sample position. */
typedef struct
{
    uint16_t id;
    uint32_t position;
} AiffMarker;

/* Stack view of the AIFF chunks seen during the scan. */
typedef struct
{
    int commFound, ssndFound;
    uint32_t numFrames;
    int sampleSize;
    double sampleRate;
    int haveSustainLoop;
    uint16_t loopStartId, loopEndId;
    AiffMarker* markers;
    uint16_t numMarkers;
    size_t ssndDataOffset;
    uint32_t ssndDataBytes;
} AiffChunkInfo;

/* COMM payload; rejects non-mono and non-8/16-bit files (soft miss). */
static bool
aiff_parse_comm(const uint8_t* d, uint32_t chunkLen, size_t avail, const char* debugPath, AiffChunkInfo* info)
{
    if (chunkLen < 18 || avail < 18)
        return true;
    int numChannels = read_u16_be(d);
    info->numFrames = read_u32_be(d + 2);
    info->sampleSize = read_u16_be(d + 6);
    info->sampleRate = read_extended80(d + 8);
    if (numChannels != 1)
    {
        if (debugPath)
            fprintf(stderr, "voicegroup_loader: %s has %d channels, must be mono\n", debugPath, numChannels);
        free(info->markers);
        info->markers = NULL;
        return false;
    }
    if (info->sampleSize != 8 && info->sampleSize != 16)
    {
        if (debugPath)
            fprintf(stderr, "voicegroup_loader: unsupported AIFF sample size %d in %s\n", info->sampleSize, debugPath);
        free(info->markers);
        info->markers = NULL;
        return false;
    }
    info->commFound = 1;
    return true;
}

/* MARK payload: count, then per marker id/position followed by a padded pascal string. */
static void aiff_read_markers(const uint8_t* data, size_t pos, size_t chunkDataEnd, AiffChunkInfo* info)
{
    for (uint16_t i = 0; i < info->numMarkers; i++)
    {
        if (pos > chunkDataEnd || chunkDataEnd - pos < 6)
            break;
        info->markers[i].id = read_u16_be(data + pos);
        info->markers[i].position = read_u32_be(data + pos + 2);
        pos += 6;
        if (pos >= chunkDataEnd)
            break;
        int nameSize = data[pos];
        if ((size_t)nameSize > chunkDataEnd - pos - 1)
            break;
        size_t adv = 1 + (size_t)nameSize + !(nameSize & 1);
        if (adv > chunkDataEnd - pos)
            break;
        pos += adv;
        if (pos > chunkDataEnd)
            break;
    }
}

/* MARK chunk; only the first marker chunk is honored, allocation failure is hard. */
static bool aiff_parse_mark(const uint8_t* data,
                            uint32_t chunkLen,
                            size_t chunkDataStart,
                            size_t chunkDataEnd,
                            size_t avail,
                            bool* hardFailure,
                            AiffChunkInfo* info)
{
    if (chunkLen < 2 || info->markers || avail < 2)
        return true;
    uint16_t n = read_u16_be(data + chunkDataStart);
    if (n == 0)
        return true;
    info->markers = (AiffMarker*)calloc(n, sizeof(*info->markers));
    if (!info->markers)
    {
        if (hardFailure)
            *hardFailure = true;
        return false;
    }
    info->numMarkers = n;
    aiff_read_markers(data, chunkDataStart + 2, chunkDataEnd, info);
    return true;
}

static void aiff_parse_inst(const uint8_t* d, AiffChunkInfo* info)
{
    int loopType = read_u16_be(d + 8);
    if (loopType)
    {
        info->loopStartId = read_u16_be(d + 10);
        info->loopEndId = read_u16_be(d + 12);
        info->haveSustainLoop = 1;
    }
}

/* SSND payload: 8-byte data offset block, then the audio bytes clamped to the file. */
static void aiff_parse_ssnd(size_t chunkDataStart, uint32_t chunkLen, size_t size, AiffChunkInfo* info)
{
    info->ssndDataOffset = chunkDataStart + 8;
    info->ssndDataBytes = chunkLen - 8;
    if (info->ssndDataBytes > size || info->ssndDataOffset > size - info->ssndDataBytes)
    {
        if (info->ssndDataOffset < size)
            info->ssndDataBytes = (uint32_t)(size - info->ssndDataOffset);
        else
            info->ssndDataBytes = 0;
    }
    info->ssndFound = 1;
}

/* Returns false only when the scan must abort and the caller return NULL. */
static bool aiff_parse_chunk(const uint8_t* data,
                             size_t size,
                             const uint8_t* hdr,
                             uint32_t chunkLen,
                             size_t chunkDataStart,
                             size_t chunkDataEnd,
                             const char* debugPath,
                             bool* hardFailure,
                             AiffChunkInfo* info)
{
    if (memcmp(hdr, "COMM", 4) == 0)
        return aiff_parse_comm(data + chunkDataStart, chunkLen, size - chunkDataStart, debugPath, info);
    if (memcmp(hdr, "MARK", 4) == 0)
        return aiff_parse_mark(data, chunkLen, chunkDataStart, chunkDataEnd, size - chunkDataStart, hardFailure, info);
    if (memcmp(hdr, "INST", 4) == 0)
    {
        if (chunkLen >= 20 && size - chunkDataStart >= 20)
            aiff_parse_inst(data + chunkDataStart, info);
        return true;
    }
    if (memcmp(hdr, "SSND", 4) == 0 && chunkLen >= 8 && size - chunkDataStart >= 8)
        aiff_parse_ssnd(chunkDataStart, chunkLen, size, info);
    return true;
}

static bool
aiff_scan_chunks(const uint8_t* data, size_t size, const char* debugPath, bool* hardFailure, AiffChunkInfo* info)
{
    size_t offset = 12;
    while (offset <= size && size - offset >= 8)
    {
        const uint8_t* hdr = data + offset;
        uint32_t chunkLen = read_u32_be(hdr + 4);
        size_t chunkDataStart = offset + 8;
        if ((size_t)chunkLen > size - chunkDataStart)
            break;
        size_t chunkDataEnd = chunkDataStart + (size_t)chunkLen;
        if (!aiff_parse_chunk(data, size, hdr, chunkLen, chunkDataStart, chunkDataEnd, debugPath, hardFailure, info))
            return false;
        size_t nextChunk = chunkDataEnd;
        if (chunkLen & 1)
        {
            if (nextChunk >= size)
                break;
            nextChunk += 1;
        }
        if (nextChunk <= offset)
            break;
        offset = nextChunk;
    }
    return true;
}

/* First marker carrying the requested id; false when absent. */
static bool aiff_find_marker(const AiffChunkInfo* info, uint16_t id, uint32_t* position)
{
    for (uint16_t i = 0; i < info->numMarkers; i++)
    {
        if (info->markers[i].id == id)
        {
            *position = info->markers[i].position;
            return true;
        }
    }
    return false;
}

/* INST sustain loop: start marker sets the loop, end marker wins when earlier or unset. */
static void
aiff_resolve_loop(const AiffChunkInfo* info, uint32_t* outLoopStart, uint32_t* outNumSamples, int* outLoopEnabled)
{
    *outLoopStart = 0;
    *outLoopEnabled = 0;
    *outNumSamples = info->numFrames;
    if (!info->haveSustainLoop)
        return;
    uint32_t loopStart = 0;
    int loopEnabled = aiff_find_marker(info, info->loopStartId, &loopStart);
    uint32_t endPosition = 0;
    if (aiff_find_marker(info, info->loopEndId, &endPosition))
    {
        if (endPosition < loopStart || !loopEnabled)
        {
            loopStart = endPosition;
            loopEnabled = 1;
        }
        *outNumSamples = endPosition;
    }
    *outLoopStart = loopStart;
    *outLoopEnabled = loopEnabled;
}

/* AIFF frame counts are inclusive: one trailing sample is dropped, clamped to SSND. */
static uint32_t aiff_resolve_wsize(uint32_t numSamples, int sampleSize, uint32_t ssndDataBytes)
{
    uint32_t wsize = (numSamples > 0) ? numSamples - 1 : 0;
    uint32_t availableSamples = ssndDataBytes / ((sampleSize == 16) ? 2u : 1u);
    if (wsize > availableSamples)
        wsize = availableSamples;
    return wsize;
}

static void aiff_materialize_samples(
    WaveData* wd, const uint8_t* data, size_t size, size_t ssndDataOffset, uint32_t bytesPerSample, uint32_t wsize)
{
    for (uint32_t i = 0; i < wsize; i++)
    {
        size_t off = ssndDataOffset + (size_t)i * bytesPerSample;
        if (off + bytesPerSample <= size)
            wd->data[i] = (int8_t)data[off]; /* 8-bit direct; 16-bit big-endian high byte */
        else
            wd->data[i] = 0;
    }
    wd->data[wsize] = (wsize > 0) ? wd->data[wsize - 1] : 0;
}

WaveData* vg_asset_decode_aiff(const uint8_t* data, size_t size, const char* debugPath, bool* hardFailure)
{
    if (hardFailure)
        *hardFailure = false;
    if (!data || size < 12 || memcmp(data, "FORM", 4) != 0 || memcmp(data + 8, "AIFF", 4) != 0)
    {
        if (debugPath)
            fprintf(stderr, "voicegroup_loader: invalid FORM/AIFF header in %s\n", debugPath);
        return NULL;
    }
    AiffChunkInfo info = {0};
    if (!aiff_scan_chunks(data, size, debugPath, hardFailure, &info))
        return NULL;
    if (!info.commFound || !info.ssndFound)
    {
        if (debugPath)
            fprintf(stderr, "voicegroup_loader: missing COMM or SSND chunk in %s\n", debugPath);
        free(info.markers);
        return NULL;
    }
    uint32_t loopStart, numSamples;
    int loopEnabled;
    aiff_resolve_loop(&info, &loopStart, &numSamples, &loopEnabled);
    free(info.markers);

    uint32_t wsize = aiff_resolve_wsize(numSamples, info.sampleSize, info.ssndDataBytes);
    WaveData* wd = alloc_wavedata(wsize);
    if (!wd)
    {
        if (hardFailure)
            *hardFailure = true;
        return NULL;
    }
    wavedata_init(wd, clamp_freq(info.sampleRate * 1024.0), loopStart, wsize, loopEnabled);
    aiff_materialize_samples(wd, data, size, info.ssndDataOffset, (info.sampleSize == 16) ? 2u : 1u, wsize);
    return wd;
}

/* ---- BIN decoder (memory) ---- */

WaveData* vg_asset_decode_bin(const uint8_t* data, size_t size, const char* debugPath, bool* hardFailure)
{
    if (hardFailure)
        *hardFailure = false;
    if (!data || size < 16)
    {
        if (debugPath)
            fprintf(stderr, "voicegroup_loader: short read on header %s\n", debugPath ? debugPath : "(blob)");
        return NULL;
    }
    uint16_t type = read_u16_le(data);
    uint16_t status = read_u16_le(data + 2);
    uint32_t freq = read_u32_le(data + 4);
    uint32_t loopStart = read_u32_le(data + 8);
    uint32_t wsize = read_u32_le(data + 12);
    size_t payloadAvail = size - 16;
    uint32_t allocSize;
    size_t copySize;
    if (wsize == 0)
    {
        copySize = payloadAvail > 16 ? 16 : payloadAvail;
        allocSize = (uint32_t)copySize;
    }
    else
    {
        if (wsize > payloadAvail)
            return NULL;
        allocSize = wsize;
        copySize = wsize;
    }
    WaveData* wd = alloc_wavedata(allocSize);
    if (!wd)
    {
        if (hardFailure)
            *hardFailure = true;
        return NULL;
    }
    wd->type = type;
    wd->status = status;
    wd->freq = freq;
    wd->loopStart = loopStart;
    wd->size = wsize;
    if (copySize > 0)
        memcpy(wd->data, data + 16, copySize);
    wd->data[allocSize] = (allocSize > 0) ? wd->data[allocSize - 1] : 0;
    return wd;
}

/* ---- PROG decoder (memory) ---- */

uint32_t* vg_asset_decode_prog(const uint8_t* data, size_t size, const char* debugPath, bool* hardFailure)
{
    if (hardFailure)
        *hardFailure = false;
    if (!data || size < 16)
    {
        if (debugPath)
            fprintf(stderr, "voicegroup_loader: short read on wave %s\n", debugPath ? debugPath : "(blob)");
        return NULL;
    }
    uint32_t* out = (uint32_t*)malloc(16);
    if (!out)
    {
        if (hardFailure)
            *hardFailure = true;
        return NULL;
    }
    memcpy(out, data, 16);
    return out;
}

/* ---- Serial file helpers (single-decoder reuse) ---- */

/* Marks a transport or allocation failure and reports failure to the caller. */
static bool blob_set_hard(bool* hardFailure)
{
    if (hardFailure)
        *hardFailure = true;
    return false;
}

/* fopen with the ENOENT/ENOTDIR soft-miss distinction; other errors are hard. */
static FILE* blob_open_file(const char* absPath, bool* hardFailure)
{
    FILE* f = fopen(absPath, "rb");
    if (!f && hardFailure && errno != ENOENT && errno != ENOTDIR)
        *hardFailure = true;
    return f;
}

/* Sizing requires seek-end, tell, then seek back to the start. */
static bool blob_seek_size(FILE* f, long* outSize)
{
    if (fseek(f, 0, SEEK_END) != 0)
        return false;
    long sz = ftell(f);
    if (sz < 0 || (uintmax_t)sz > SIZE_MAX)
        return false;
    if (fseek(f, 0, SEEK_SET) != 0)
        return false;
    *outSize = sz;
    return true;
}

/* Whole-file read; the caller owns the buffer. Never reads zero-size files. */
static uint8_t* blob_read_all(FILE* f, size_t usize)
{
    uint8_t* buf = (uint8_t*)malloc(usize ? usize : 1);
    if (!buf)
        return NULL;
    size_t got = usize ? fread(buf, 1, usize, f) : 0;
    if (got != usize || ferror(f))
    {
        free(buf);
        return NULL;
    }
    return buf;
}

static bool read_file_to_blob(const char* absPath, uint8_t** outData, size_t* outSize, bool* hardFailure)
{
    if (hardFailure)
        *hardFailure = false;
    if (!absPath || !outData || !outSize)
        return blob_set_hard(hardFailure);
    *outData = NULL;
    *outSize = 0;
    FILE* f = blob_open_file(absPath, hardFailure);
    if (!f)
        return false;
    long sz;
    if (!blob_seek_size(f, &sz))
    {
        fclose(f);
        return blob_set_hard(hardFailure);
    }
    uint8_t* buf = blob_read_all(f, (size_t)sz);
    fclose(f);
    if (!buf)
        return blob_set_hard(hardFailure);
    *outData = buf;
    *outSize = (size_t)sz;
    return true;
}

WaveData* vg_asset_load_wav_file(const char* absolutePath, bool* hardFailure)
{
    uint8_t* data = NULL;
    size_t size = 0;
    if (!read_file_to_blob(absolutePath, &data, &size, hardFailure))
        return NULL;
    WaveData* wd = vg_asset_decode_wav(data, size, absolutePath, hardFailure);
    free(data);
    return wd;
}

WaveData* vg_asset_load_aiff_file(const char* absolutePath, bool* hardFailure)
{
    uint8_t* data = NULL;
    size_t size = 0;
    if (!read_file_to_blob(absolutePath, &data, &size, hardFailure))
        return NULL;
    WaveData* wd = vg_asset_decode_aiff(data, size, absolutePath, hardFailure);
    free(data);
    return wd;
}

/* ---- Dedup helpers ---- */

void vg_dedup_init(VgDedup* d)
{
    if (!d)
        return;
    d->paths = NULL;
    d->count = 0;
    d->capacity = 0;
}
void vg_dedup_deinit(VgDedup* d)
{
    if (!d)
        return;
    for (size_t i = 0; i < d->count; i++)
        free(d->paths[i]);
    free(d->paths);
    d->paths = NULL;
    d->count = d->capacity = 0;
}
bool vg_dedup_add(VgDedup* d, const char* path)
{
    if (!d || !path || !path[0])
        return false;
    for (size_t i = 0; i < d->count; i++)
        if (strcmp(d->paths[i], path) == 0)
            return true;
    if (d->count >= d->capacity)
    {
        size_t nc;
        if (d->capacity == 0)
            nc = 8;
        else
        {
            if (d->capacity > SIZE_MAX / 2)
                return false;
            nc = d->capacity * 2;
        }
        if (nc > SIZE_MAX / sizeof(char*))
            return false;
        char** np = (char**)realloc(d->paths, nc * sizeof(char*));
        if (!np)
            return false;
        d->paths = np;
        d->capacity = nc;
    }
    size_t len = strlen(path);
    char* copy = (char*)malloc(len + 1);
    if (!copy)
        return false;
    memcpy(copy, path, len + 1);
    d->paths[d->count++] = copy;
    return true;
}
int vg_dedup_find(const VgDedup* d, const char* path)
{
    if (!d || !path)
        return -1;
    for (size_t i = 0; i < d->count; i++)
        if (strcmp(d->paths[i], path) == 0)
            return (int)i;
    return -1;
}
bool vg_dedup_contains(const VgDedup* d, const char* path)
{
    return vg_dedup_find(d, path) >= 0;
}

/* ---- Batch read ---- */

static bool batch_report_error(char* error, size_t errorCapacity, const char* message)
{
    if (error && errorCapacity)
        snprintf(error, errorCapacity, "%s", message);
    return false;
}

/* Argument and transport checks in reporting order; count==0 is handled by the caller. */
static bool batch_validate_args(const VoicegroupFileIo* io,
                                const VgDedup* dedup,
                                const VoicegroupFileBlob* outBlobs,
                                size_t count,
                                char* error,
                                size_t errorCapacity)
{
    if (!dedup)
        return batch_report_error(error, errorCapacity, "vg_batch_read: null dedup");
    if (!outBlobs && count != 0)
        return batch_report_error(error, errorCapacity, "vg_batch_read: null out");
    if (!io || !io->readBatch || !io->releaseBatch)
        return batch_report_error(error, errorCapacity, "vg_batch_read: incomplete fileIo");
    if (count > (size_t)INT_MAX)
        return batch_report_error(error, errorCapacity, "vg_batch_read: count too large");
    return true;
}

bool vg_batch_read(
    const VoicegroupFileIo* io, const VgDedup* dedup, VoicegroupFileBlob* outBlobs, char* error, size_t errorCapacity)
{
    size_t count = dedup ? dedup->count : 0;
    if (!batch_validate_args(io, dedup, outBlobs, count, error, errorCapacity))
        return false;
    if (count == 0)
        return true;
    for (size_t i = 0; i < count; i++)
        outBlobs[i] = (VoicegroupFileBlob){0};
    if (error && errorCapacity)
        error[0] = '\0';
    const char* const* paths = (const char* const*)dedup->paths;
    return io->readBatch(io->user, paths, count, outBlobs, error, errorCapacity);
}
void vg_batch_release(const VoicegroupFileIo* io, VoicegroupFileBlob* blobs, size_t count)
{
    if (!blobs || count == 0)
        return;
    if (io && io->releaseBatch)
        io->releaseBatch(io->user, blobs, count);
    else
    {
        for (size_t i = 0; i < count; i++)
        {
            free(blobs[i].data);
            blobs[i] = (VoicegroupFileBlob){0};
        }
    }
}
