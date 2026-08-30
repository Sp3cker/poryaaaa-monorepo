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

static int8_t convert_sample(const uint8_t* sp, int fmtTag, uint32_t bps)
{
    if (fmtTag == 1)
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
    }
    else
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
        double scaled = ds * 128.0;
        if (!isfinite(scaled))
        {
            if (isinf(scaled))
                return (scaled > 0.0) ? 127 : -128;
            return 0;
        }
        double floored = floor(scaled);
        if (!isfinite(floored))
        {
            if (isinf(floored))
                return (floored > 0.0) ? 127 : -128;
            return 0;
        }
        if (floored < -128.0)
            return -128;
        if (floored > 127.0)
            return 127;
        return (int8_t)(int)floored;
    }
    return 0;
}

/* ---- WAV decoder (memory) ---- */

WaveData* vg_asset_decode_wav(
    const uint8_t* data, size_t size, const char* debugPath, bool* hardFailure)
{
    if (hardFailure)
        *hardFailure = false;
    if (!data || size < 12)
    {
        if (debugPath)
            fprintf(stderr, "voicegroup_loader: invalid RIFF/WAVE header in %s\n", debugPath);
        return NULL;
    }
    if (memcmp(data, "RIFF", 4) != 0 || memcmp(data + 8, "WAVE", 4) != 0)
    {
        if (debugPath)
            fprintf(stderr, "voicegroup_loader: invalid RIFF/WAVE header in %s\n", debugPath);
        return NULL;
    }
    uint32_t riffSize = read_u32_le(data + 4);
    size_t fileEnd = size;
    uint64_t claimed = UINT64_C(8) + riffSize;
    if (claimed < (uint64_t)fileEnd)
        fileEnd = (size_t)claimed;

    int fmtFound = 0, dataFound = 0;
    int fmtTag = 0;
    uint32_t sampleRate = 0;
    uint16_t blockAlign = 0, bitsPerSample = 0;
    uint32_t midiKey = 60, midiPitchFraction = 0;
    uint32_t smplLoopStart = 0, smplLoopEnd = 0;
    int loopEnabled = 0;
    uint32_t agbPitch = 0, agbLoopEnd = 0;
    size_t dataOffset = 0;
    uint32_t dataLen = 0;

    size_t offset = 12;
    size_t limit = fileEnd;
    while (offset <= limit && limit - offset >= 8)
    {
        const uint8_t* hdr = data + offset;
        uint32_t chunkLen = read_u32_le(hdr + 4);
        size_t chunkDataStart = offset + 8;
        if ((size_t)chunkLen > limit - chunkDataStart)
            break;
        size_t chunkDataEnd = chunkDataStart + (size_t)chunkLen;

        if (memcmp(hdr, "fmt ", 4) == 0 && chunkLen >= 16 && size - chunkDataStart >= 16)
        {
            const uint8_t* d = data + chunkDataStart;
            fmtTag = d[0] | (d[1] << 8);
            sampleRate = read_u32_le(d + 4);
            blockAlign = read_u16_le(d + 12);
            bitsPerSample = read_u16_le(d + 14);
            fmtFound = 1;
        }
        else if (memcmp(hdr, "smpl", 4) == 0 && chunkLen >= 32 && size - chunkDataStart >= 32)
        {
            size_t avail = chunkDataEnd - chunkDataStart;
            size_t readLen = avail < 52 ? avail : 52;
            if (readLen >= 32)
            {
                const uint8_t* d = data + chunkDataStart;
                midiKey = read_u32_le(d + 12);
                if (midiKey > 127)
                    midiKey = 127;
                midiPitchFraction = read_u32_le(d + 16);
                uint32_t numLoops = read_u32_le(d + 28);
                if (numLoops == 1 && readLen >= 52)
                {
                    smplLoopStart = read_u32_le(d + 44);
                    uint32_t loopEndIncl = read_u32_le(d + 48);
                    smplLoopEnd = loopEndIncl + 1;
                    loopEnabled = 1;
                }
            }
        }
        else if (memcmp(hdr, "agbp", 4) == 0 && chunkLen >= 4 && size - chunkDataStart >= 4)
        {
            agbPitch = read_u32_le(data + chunkDataStart);
        }
        else if (memcmp(hdr, "agbl", 4) == 0 && chunkLen >= 4 && size - chunkDataStart >= 4)
        {
            agbLoopEnd = read_u32_le(data + chunkDataStart);
        }
        else if (memcmp(hdr, "data", 4) == 0)
        {
            dataOffset = chunkDataStart;
            dataLen = chunkLen;
            dataFound = 1;
        }

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

    if (!fmtFound || !dataFound)
    {
        if (debugPath)
            fprintf(stderr, "voicegroup_loader: missing fmt or data chunk in %s\n", debugPath);
        return NULL;
    }

    uint32_t bytesPerSample;
    if (fmtTag == 1)
    {
        if (blockAlign == 1 && bitsPerSample == 8)
            bytesPerSample = 1;
        else if (blockAlign == 2 && bitsPerSample == 16)
            bytesPerSample = 2;
        else if (blockAlign == 3 && bitsPerSample == 24)
            bytesPerSample = 3;
        else if (blockAlign == 4 && bitsPerSample == 32)
            bytesPerSample = 4;
        else
        {
            if (debugPath)
                fprintf(stderr, "voicegroup_loader: unsupported integer PCM format in %s\n", debugPath);
            return NULL;
        }
    }
    else if (fmtTag == 3)
    {
        if (blockAlign == 4 && bitsPerSample == 32)
            bytesPerSample = 4;
        else if (blockAlign == 8 && bitsPerSample == 64)
            bytesPerSample = 8;
        else
        {
            if (debugPath)
                fprintf(stderr, "voicegroup_loader: unsupported float format in %s\n", debugPath);
            return NULL;
        }
    }
    else
    {
        if (debugPath)
            fprintf(stderr, "voicegroup_loader: unsupported audio format %d in %s\n", fmtTag, debugPath);
        return NULL;
    }

    uint32_t numSamples = dataLen / bytesPerSample;
    uint32_t loopEnd;
    if (loopEnabled)
        loopEnd = smplLoopEnd;
    else
        loopEnd = numSamples;
    if (loopEnd > numSamples)
        loopEnd = numSamples;
    if (agbLoopEnd != 0)
    {
        if (agbLoopEnd > numSamples)
            agbLoopEnd = numSamples;
        loopEnd = agbLoopEnd;
    }
    uint32_t wsize = loopEnd;

    uint32_t freq;
    if (agbPitch != 0)
        freq = agbPitch;
    else if (midiKey == 60 && midiPitchFraction == 0)
    {
        double d = (double)sampleRate * 1024.0;
        if (!isfinite(d) || d < 0.0)
            freq = 0;
        else if (d > (double)UINT32_MAX)
            freq = UINT32_MAX;
        else
            freq = (uint32_t)d;
    }
    else
    {
        double tuning = (double)midiPitchFraction / (4294967296.0 * 100.0);
        double pitch = (double)sampleRate * pow(2.0, (60.0 - (double)midiKey) / 12.0 + tuning / 1200.0);
        double d = pitch * 1024.0;
        if (!isfinite(d) || d < 0.0)
            freq = 0;
        else if (d > (double)UINT32_MAX)
            freq = UINT32_MAX;
        else
            freq = (uint32_t)d;
    }
    WaveData* wd = alloc_wavedata(wsize);
    if (!wd)
    {
        if (hardFailure)
            *hardFailure = true;
        return NULL;
    }
    wd->type = 0;
    wd->status = loopEnabled ? 0x4000 : 0;
    wd->freq = freq;
    wd->loopStart = smplLoopStart;
    wd->size = wsize;

    for (uint32_t i = 0; i < wsize; i++)
    {
        size_t off = dataOffset + (size_t)i * bytesPerSample;
        if (off + bytesPerSample <= dataOffset + dataLen && off + bytesPerSample <= size)
            wd->data[i] = convert_sample(data + off, fmtTag, bytesPerSample);
        else
            wd->data[i] = 0;
    }
    wd->data[wsize] = (wsize > 0) ? wd->data[wsize - 1] : 0;
    return wd;
}

/* ---- AIFF decoder (memory) ---- */

WaveData* vg_asset_decode_aiff(
    const uint8_t* data, size_t size, const char* debugPath, bool* hardFailure)
{
    if (hardFailure)
        *hardFailure = false;
    if (!data || size < 12 || memcmp(data, "FORM", 4) != 0 || memcmp(data + 8, "AIFF", 4) != 0)
    {
        if (debugPath)
            fprintf(stderr, "voicegroup_loader: invalid FORM/AIFF header in %s\n", debugPath);
        return NULL;
    }
    int commFound = 0, ssndFound = 0;
    uint32_t numFrames = 0;
    int sampleSize = 0;
    double sampleRate = 0.0;
    int haveSustainLoop = 0;
    uint16_t loopStartId = 0, loopEndId = 0;
    struct
    {
        uint16_t id;
        uint32_t position;
    }* markers = NULL;
    uint16_t numMarkers = 0;
    size_t ssndDataOffset = 0;
    uint32_t ssndDataBytes = 0;

    size_t offset = 12;
    while (offset <= size && size - offset >= 8)
    {
        const uint8_t* hdr = data + offset;
        uint32_t chunkLen = read_u32_be(hdr + 4);
        size_t chunkDataStart = offset + 8;
        if ((size_t)chunkLen > size - chunkDataStart)
            break;
        size_t chunkDataEnd = chunkDataStart + (size_t)chunkLen;

        if (memcmp(hdr, "COMM", 4) == 0 && chunkLen >= 18 && size - chunkDataStart >= 18)
        {
            const uint8_t* d = data + chunkDataStart;
            int numChannels = read_u16_be(d);
            numFrames = read_u32_be(d + 2);
            sampleSize = read_u16_be(d + 6);
            sampleRate = read_extended80(d + 8);
            if (numChannels != 1)
            {
                if (debugPath)
                    fprintf(stderr, "voicegroup_loader: %s has %d channels, must be mono\n", debugPath, numChannels);
                free(markers);
                return NULL;
            }
            if (sampleSize != 8 && sampleSize != 16)
            {
                if (debugPath)
                    fprintf(
                        stderr, "voicegroup_loader: unsupported AIFF sample size %d in %s\n", sampleSize, debugPath);
                free(markers);
                return NULL;
            }
            commFound = 1;
        }
        else if (memcmp(hdr, "MARK", 4) == 0 && chunkLen >= 2 && !markers && size - chunkDataStart >= 2)
        {
            uint16_t n = read_u16_be(data + chunkDataStart);
            if (n > 0)
            {
                markers = (void*)calloc(n, sizeof(*markers));
                if (!markers)
                {
                    if (hardFailure)
                        *hardFailure = true;
                    return NULL;
                }
                numMarkers = n;
                size_t pos = chunkDataStart + 2;
                for (uint16_t i = 0; i < n; i++)
                {
                    if (pos > chunkDataEnd || chunkDataEnd - pos < 6)
                        break;
                    markers[i].id = read_u16_be(data + pos);
                    markers[i].position = read_u32_be(data + pos + 2);
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
        }
        else if (memcmp(hdr, "INST", 4) == 0 && chunkLen >= 20 && size - chunkDataStart >= 20)
        {
            const uint8_t* d = data + chunkDataStart;
            int loopType = read_u16_be(d + 8);
            if (loopType)
            {
                loopStartId = read_u16_be(d + 10);
                loopEndId = read_u16_be(d + 12);
                haveSustainLoop = 1;
            }
        }
        else if (memcmp(hdr, "SSND", 4) == 0 && chunkLen >= 8 && size - chunkDataStart >= 8)
        {
            ssndDataOffset = chunkDataStart + 8;
            ssndDataBytes = chunkLen - 8;
            if (ssndDataBytes > size || ssndDataOffset > size - ssndDataBytes)
            {
                if (ssndDataOffset < size)
                    ssndDataBytes = (uint32_t)(size - ssndDataOffset);
                else
                    ssndDataBytes = 0;
            }
            ssndFound = 1;
        }

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

    if (!commFound || !ssndFound)
    {
        if (debugPath)
            fprintf(stderr, "voicegroup_loader: missing COMM or SSND chunk in %s\n", debugPath);
        free(markers);
        return NULL;
    }

    int loopEnabled = 0;
    uint32_t loopStart = 0;
    uint32_t numSamples = numFrames;
    if (haveSustainLoop)
    {
        for (uint16_t i = 0; i < numMarkers; i++)
            if (markers[i].id == loopStartId)
            {
                loopStart = markers[i].position;
                loopEnabled = 1;
                break;
            }
        for (uint16_t i = 0; i < numMarkers; i++)
            if (markers[i].id == loopEndId)
            {
                if (markers[i].position < loopStart || !loopEnabled)
                {
                    loopStart = markers[i].position;
                    loopEnabled = 1;
                }
                numSamples = markers[i].position;
                break;
            }
    }
    free(markers);

    uint32_t wsize = (numSamples > 0) ? numSamples - 1 : 0;
    uint32_t bytesPerSample = (sampleSize == 16) ? 2 : 1;
    uint32_t availableSamples = ssndDataBytes / bytesPerSample;
    if (wsize > availableSamples)
        wsize = availableSamples;

    WaveData* wd = alloc_wavedata(wsize);
    if (!wd)
    {
        if (hardFailure)
            *hardFailure = true;
        return NULL;
    }
    wd->type = 0;
    wd->status = loopEnabled ? 0x4000 : 0;
    {
        double d = sampleRate * 1024.0;
        uint32_t f;
        if (!isfinite(d) || d < 0.0)
            f = 0;
        else if (d > (double)UINT32_MAX)
            f = UINT32_MAX;
        else
            f = (uint32_t)d;
        wd->freq = f;
    }
    wd->loopStart = loopStart;
    wd->size = wsize;

    for (uint32_t i = 0; i < wsize; i++)
    {
        size_t off = ssndDataOffset + (size_t)i * bytesPerSample;
        if (off + bytesPerSample <= size)
            wd->data[i] = (int8_t)data[off]; /* 8-bit direct; 16-bit big-endian high byte */
        else
            wd->data[i] = 0;
    }
    wd->data[wsize] = (wsize > 0) ? wd->data[wsize - 1] : 0;
    return wd;
}

/* ---- BIN decoder (memory) ---- */

WaveData* vg_asset_decode_bin(
    const uint8_t* data, size_t size, const char* debugPath, bool* hardFailure)
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

uint32_t* vg_asset_decode_prog(
    const uint8_t* data, size_t size, const char* debugPath, bool* hardFailure)
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

static bool
read_file_to_blob(const char* absPath, uint8_t** outData, size_t* outSize, bool* hardFailure)
{
    if (hardFailure)
        *hardFailure = false;
    if (!absPath || !outData || !outSize)
    {
        if (hardFailure)
            *hardFailure = true;
        return false;
    }
    *outData = NULL;
    *outSize = 0;
    FILE* f = fopen(absPath, "rb");
    if (!f)
    {
        if (hardFailure && errno != ENOENT && errno != ENOTDIR)
            *hardFailure = true;
        return false;
    }
    if (fseek(f, 0, SEEK_END) != 0)
    {
        if (hardFailure)
            *hardFailure = true;
        fclose(f);
        return false;
    }
    long sz = ftell(f);
    if (sz < 0 || (uintmax_t)sz > SIZE_MAX)
    {
        if (hardFailure)
            *hardFailure = true;
        fclose(f);
        return false;
    }
    if (fseek(f, 0, SEEK_SET) != 0)
    {
        if (hardFailure)
            *hardFailure = true;
        fclose(f);
        return false;
    }
    size_t usize = (size_t)sz;
    uint8_t* buf = (uint8_t*)malloc(usize ? usize : 1);
    if (!buf)
    {
        if (hardFailure)
            *hardFailure = true;
        fclose(f);
        return false;
    }
    size_t read = usize ? fread(buf, 1, usize, f) : 0;
    if (read != usize || ferror(f))
    {
        if (hardFailure)
            *hardFailure = true;
        free(buf);
        fclose(f);
        return false;
    }
    fclose(f);
    *outData = buf;
    *outSize = usize;
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

bool vg_batch_read(
    const VoicegroupFileIo* io, const VgDedup* dedup, VoicegroupFileBlob* outBlobs, char* error, size_t errorCapacity)
{
    if (!dedup)
    {
        if (error && errorCapacity)
            snprintf(error, errorCapacity, "vg_batch_read: null dedup");
        return false;
    }
    size_t count = dedup->count;
    if (!outBlobs && count != 0)
    {
        if (error && errorCapacity)
            snprintf(error, errorCapacity, "vg_batch_read: null out");
        return false;
    }
    if (!io || !io->readBatch || !io->releaseBatch)
    {
        if (error && errorCapacity)
            snprintf(error, errorCapacity, "vg_batch_read: incomplete fileIo");
        return false;
    }
    if (count == 0)
        return true;
    if (count > (size_t)INT_MAX)
    {
        if (error && errorCapacity)
            snprintf(error, errorCapacity, "vg_batch_read: count too large");
        return false;
    }
    for (size_t i = 0; i < count; i++)
        outBlobs[i] = (VoicegroupFileBlob){0};
    if (error && errorCapacity)
        error[0] = '\0';
    const char* const* paths = (const char* const*)dedup->paths;
    bool ok = io->readBatch(io->user, paths, count, outBlobs, error, errorCapacity);
    return ok;
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
