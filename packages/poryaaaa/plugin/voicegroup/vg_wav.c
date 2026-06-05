#include "vg_wav.h"
#include "vg_alloc.h"
#include "vg_log.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---- Wave cache ---- */

void vg_wave_cache_init(WaveCache *cache)
{
    cache->count = 0;
}

WaveData *vg_wave_cache_find(const WaveCache *cache, const char *absPath)
{
    for (int i = 0; i < cache->count; i++)
        if (strcmp(cache->entries[i].absPath, absPath) == 0)
            return cache->entries[i].wd;
    return NULL;
}

void vg_wave_cache_insert(WaveCache *cache, const char *absPath, WaveData *wd)
{
    if (cache->count >= VG_WAVE_CACHE_CAPACITY) return;
    strncpy(cache->entries[cache->count].absPath, absPath, VG_MAX_PATH_LEN - 1);
    cache->entries[cache->count].absPath[VG_MAX_PATH_LEN - 1] = '\0';
    cache->entries[cache->count].wd = wd;
    cache->count++;
}

/* ---- Little-endian readers ---- */

static uint32_t read_u32_le(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static uint16_t read_u16_le(const uint8_t *p)
{
    return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}

/* ---- RIFF/WAVE parser ---- */

/*
 * Fields extracted from a single .wav file's chunks, before being
 * turned into a WaveData struct. Keeps the chunk iteration loop
 * flat and the conversion logic below readable.
 */
typedef struct {
    int fmtFound;
    int dataFound;

    /* fmt */
    int fmtTag;
    uint32_t sampleRate;
    uint16_t blockAlign;
    uint16_t bitsPerSample;

    /* smpl */
    uint32_t midiKey;
    uint32_t midiPitchFraction;
    uint32_t smplLoopStart;
    uint32_t smplLoopEnd;
    int loopEnabled;

    /* agbp / agbl custom chunks */
    uint32_t agbPitch;
    uint32_t agbLoopEnd;

    /* data chunk location inside the file */
    long dataOffset;
    uint32_t dataLen;
} WavChunkData;

static void parse_fmt_chunk(FILE *f, uint32_t chunkLen, WavChunkData *out)
{
    if (chunkLen < 16) return;
    uint8_t d[16];
    if (fread(d, 1, 16, f) != 16) return;
    out->fmtTag        = d[0] | (d[1] << 8);
    out->sampleRate    = read_u32_le(d + 4);
    out->blockAlign    = read_u16_le(d + 12);
    out->bitsPerSample = read_u16_le(d + 14);
    out->fmtFound = 1;
}

static void parse_smpl_chunk(FILE *f, uint32_t chunkLen, WavChunkData *out)
{
    if (chunkLen < 32) return;
    uint32_t readLen = chunkLen < 52 ? chunkLen : 52;
    uint8_t d[52];
    if (fread(d, 1, readLen, f) != readLen) return;

    out->midiKey = read_u32_le(d + 12);
    if (out->midiKey > 127) out->midiKey = 127;
    out->midiPitchFraction = read_u32_le(d + 16);

    uint32_t numLoops = read_u32_le(d + 28);
    if (numLoops == 1 && readLen >= 52) {
        out->smplLoopStart = read_u32_le(d + 44);
        uint32_t loopEndIncl = read_u32_le(d + 48);
        out->smplLoopEnd = loopEndIncl + 1;
        out->loopEnabled = 1;
    }
}

/*
 * Walk the RIFF chunk list and fill *out. Any unrecognized chunk is
 * skipped by seeking past its length.
 */
static int read_wav_chunks(FILE *f, const char *absoluteWavPath, WavChunkData *out)
{
    memset(out, 0, sizeof(*out));
    out->midiKey = 60;  /* default MIDI key if no smpl chunk */

    uint8_t riffHdr[12];
    if (fread(riffHdr, 1, 12, f) != 12 ||
        memcmp(riffHdr, "RIFF", 4) != 0 ||
        memcmp(riffHdr + 8, "WAVE", 4) != 0) {
        vg_err("invalid RIFF/WAVE header in %s", absoluteWavPath);
        return 0;
    }
    uint32_t riffSize = read_u32_le(riffHdr + 4);
    long fileEnd = 8 + (long)riffSize;

    while (1) {
        long pos = ftell(f);
        if (pos < 0 || pos + 8 > fileEnd) break;

        uint8_t chunkHdr[8];
        if (fread(chunkHdr, 1, 8, f) != 8) break;

        uint32_t chunkLen = read_u32_le(chunkHdr + 4);
        long chunkDataStart = ftell(f);

        if (memcmp(chunkHdr, "fmt ", 4) == 0) {
            parse_fmt_chunk(f, chunkLen, out);
        } else if (memcmp(chunkHdr, "smpl", 4) == 0) {
            parse_smpl_chunk(f, chunkLen, out);
        } else if (memcmp(chunkHdr, "agbp", 4) == 0 && chunkLen >= 4) {
            uint8_t d[4];
            if (fread(d, 1, 4, f) == 4)
                out->agbPitch = read_u32_le(d);
        } else if (memcmp(chunkHdr, "agbl", 4) == 0 && chunkLen >= 4) {
            uint8_t d[4];
            if (fread(d, 1, 4, f) == 4)
                out->agbLoopEnd = read_u32_le(d);
        } else if (memcmp(chunkHdr, "data", 4) == 0) {
            out->dataOffset = chunkDataStart;
            out->dataLen    = chunkLen;
            out->dataFound  = 1;
        }

        long nextChunk = chunkDataStart + (long)chunkLen;
        if (chunkLen & 1) nextChunk++;  /* chunks align to even bytes */
        if (fseek(f, nextChunk, SEEK_SET) != 0) break;
    }

    if (!out->fmtFound || !out->dataFound) {
        vg_err("missing fmt or data chunk in %s", absoluteWavPath);
        return 0;
    }
    return 1;
}

/*
 * Given fmtTag + blockAlign + bitsPerSample, return the number of
 * bytes per sample frame. Returns 0 for unsupported combinations;
 * the caller logs and bails.
 */
static uint32_t bytes_per_sample(const WavChunkData *c, const char *absoluteWavPath)
{
    if (c->fmtTag == 1) {
        if (c->blockAlign == 1 && c->bitsPerSample == 8)  return 1;
        if (c->blockAlign == 2 && c->bitsPerSample == 16) return 2;
        if (c->blockAlign == 3 && c->bitsPerSample == 24) return 3;
        if (c->blockAlign == 4 && c->bitsPerSample == 32) return 4;
        vg_err("unsupported integer PCM format in %s", absoluteWavPath);
        return 0;
    }
    if (c->fmtTag == 3) {
        if (c->blockAlign == 4 && c->bitsPerSample == 32) return 4;
        if (c->blockAlign == 8 && c->bitsPerSample == 64) return 8;
        vg_err("unsupported float format in %s", absoluteWavPath);
        return 0;
    }
    vg_err("unsupported audio format %d in %s", c->fmtTag, absoluteWavPath);
    return 0;
}

/*
 * Compute the GBA-style frequency word from the .wav file's intrinsic
 * sample rate, its MIDI root key, and any pokeemerald-specific agbp
 * override chunk present.
 */
static uint32_t compute_freq(const WavChunkData *c)
{
    if (c->agbPitch != 0)
        return c->agbPitch;
    if (c->midiKey == 60 && c->midiPitchFraction == 0)
        return (uint32_t)((double)c->sampleRate * 1024.0);
    double tuning = (double)c->midiPitchFraction / (4294967296.0 * 100.0);
    double pitch  = (double)c->sampleRate *
                    pow(2.0, (60.0 - (double)c->midiKey) / 12.0 + tuning / 1200.0);
    return (uint32_t)(pitch * 1024.0);
}

/* Convert a single raw sample frame at sp to signed 8-bit. */
static int8_t convert_sample(const uint8_t *sp, int fmtTag, uint32_t bps)
{
    if (fmtTag == 1) {
        /* Integer PCM. */
        if (bps == 1) {
            return (int8_t)((int)sp[0] - 128);
        }
        if (bps == 2) {
            int16_t v = (int16_t)read_u16_le(sp);
            return (int8_t)(v >> 8);
        }
        if (bps == 3) {
            uint32_t raw = (uint32_t)sp[0] | ((uint32_t)sp[1] << 8) | ((uint32_t)sp[2] << 16);
            int32_t v = (raw & 0x800000u) ? (int32_t)(raw | 0xFF000000u) : (int32_t)raw;
            return (int8_t)(v >> 16);
        }
        /* 4 bytes. */
        int32_t v = (int32_t)read_u32_le(sp);
        return (int8_t)(v >> 24);
    }

    /* Float (fmtTag == 3). */
    double ds;
    if (bps == 4) {
        uint32_t bits = read_u32_le(sp);
        float fv;
        memcpy(&fv, &bits, sizeof(fv));
        ds = (double)fv;
    } else {
        uint64_t bits = 0;
        for (int i = 0; i < 8; i++)
            bits |= (uint64_t)sp[i] << (i * 8);
        double dv;
        memcpy(&dv, &bits, sizeof(dv));
        ds = dv;
    }
    int si = (int)floor(ds * 128.0);
    if (si < -128) si = -128;
    if (si >  127) si =  127;
    return (int8_t)si;
}

static WaveData *alloc_wavedata(uint32_t size)
{
    size_t dataBytes;
    size_t totalBytes;
    if (!vg_size_add((size_t)size, 1, &dataBytes))
        return NULL;
    if (!vg_size_add(sizeof(WaveData), dataBytes, &totalBytes))
        return NULL;

    WaveData *wd = malloc(totalBytes);
    if (!wd)
        return NULL;
    wd->data = (int8_t *)((uint8_t *)wd + sizeof(WaveData));
    return wd;
}

WaveData *vg_load_wav_file(const char *absoluteWavPath)
{
    FILE *f = fopen(absoluteWavPath, "rb");
    if (!f) return NULL;

    WavChunkData c;
    if (!read_wav_chunks(f, absoluteWavPath, &c)) {
        fclose(f);
        return NULL;
    }

    uint32_t bps = bytes_per_sample(&c, absoluteWavPath);
    if (bps == 0) {
        fclose(f);
        return NULL;
    }

    uint32_t numSamples = c.dataLen / bps;
    uint32_t loopEnd = c.loopEnabled ? c.smplLoopEnd : numSamples;
    if (loopEnd > numSamples) loopEnd = numSamples;
    if (c.agbLoopEnd != 0) loopEnd = c.agbLoopEnd;
    uint32_t size = loopEnd;

    uint32_t freq = compute_freq(&c);

    WaveData *wd = alloc_wavedata(size);
    if (!wd) {
        fclose(f);
        return NULL;
    }
    wd->type      = 0;
    wd->status    = c.loopEnabled ? 0x4000 : 0;
    wd->freq      = freq;
    wd->loopStart = c.smplLoopStart;
    wd->size      = size;

    /* Pull the raw sample bytes and convert. */
    size_t rawBytes;
    if (!vg_size_mul((size_t)size, (size_t)bps, &rawBytes)) {
        free(wd);
        fclose(f);
        return NULL;
    }
    uint8_t *rawData = NULL;
    if (rawBytes > 0) {
        rawData = malloc(rawBytes);
        if (!rawData) {
            free(wd);
            fclose(f);
            return NULL;
        }
        if (fseek(f, c.dataOffset, SEEK_SET) != 0) {
            free(rawData);
            free(wd);
            fclose(f);
            return NULL;
        }
        size_t bytesRead = fread(rawData, 1, rawBytes, f);
        if (bytesRead < rawBytes)
            memset(rawData + bytesRead, 0, rawBytes - bytesRead);
    }
    fclose(f);

    for (uint32_t i = 0; i < size; i++)
        wd->data[i] = convert_sample(rawData + (size_t)i * bps, c.fmtTag, bps);

    free(rawData);
    wd->data[size] = (size > 0) ? wd->data[size - 1] : 0;
    return wd;
}

WaveData *vg_load_bin_sample(const char *projectRoot, const char *relativePath)
{
    char fullPath[VG_MAX_PATH_LEN];
    vg_build_path(fullPath, sizeof(fullPath), projectRoot, relativePath);

    FILE *f = fopen(fullPath, "rb");
    if (!f) {
        vg_err("cannot open sample %s", fullPath);
        return NULL;
    }

    uint8_t header[16];
    if (fread(header, 1, 16, f) != 16) {
        vg_err("short read on header %s", fullPath);
        fclose(f);
        return NULL;
    }

    uint16_t type      = read_u16_le(header);
    uint16_t status    = read_u16_le(header + 2);
    uint32_t freq      = read_u32_le(header + 4);
    uint32_t loopStart = read_u32_le(header + 8);
    uint32_t size      = read_u32_le(header + 12);

    WaveData *wd = alloc_wavedata(size);
    if (!wd) {
        fclose(f);
        return NULL;
    }
    wd->type      = type;
    wd->status    = status;
    wd->freq      = freq;
    wd->loopStart = loopStart;
    wd->size      = size;

    size_t bytesRead = fread(wd->data, 1, size, f);
    if (bytesRead < size)
        memset(wd->data + bytesRead, 0, size - bytesRead);
    wd->data[size] = wd->data[size > 0 ? size - 1 : 0];

    fclose(f);
    return wd;
}

WaveData *vg_load_sample(const char *projectRoot, const char *relativeBinPath)
{
    /* Compose a sibling .wav path by swapping the .bin extension. */
    char relativeWavPath[VG_MAX_PATH_LEN];
    strncpy(relativeWavPath, relativeBinPath, VG_MAX_PATH_LEN - 1);
    relativeWavPath[VG_MAX_PATH_LEN - 1] = '\0';

    size_t pathLen = strlen(relativeWavPath);
    char *ext = NULL;
    if (pathLen >= 4 && strcmp(relativeWavPath + pathLen - 4, ".bin") == 0)
        ext = relativeWavPath + pathLen - 4;

    if (!ext)
        return vg_load_bin_sample(projectRoot, relativeBinPath);

    ext[1] = 'w'; ext[2] = 'a'; ext[3] = 'v';

    char fullPath[VG_MAX_PATH_LEN];
    vg_build_path(fullPath, sizeof(fullPath), projectRoot, relativeWavPath);

    WaveData *wd = vg_load_wav_file(fullPath);
    if (wd) return wd;

    return vg_load_bin_sample(projectRoot, relativeBinPath);
}

uint32_t *vg_load_prog_wave(const char *projectRoot, const char *relativePath)
{
    char fullPath[VG_MAX_PATH_LEN];
    vg_build_path(fullPath, sizeof(fullPath), projectRoot, relativePath);

    FILE *f = fopen(fullPath, "rb");
    if (!f) {
        vg_err("cannot open wave %s", fullPath);
        return NULL;
    }

    uint32_t *data = vg_malloc_array(4, sizeof(*data));
    if (!data) {
        fclose(f);
        return NULL;
    }

    if (fread(data, 1, 16, f) != 16) {
        vg_err("short read on wave %s", fullPath);
        free(data);
        fclose(f);
        return NULL;
    }

    fclose(f);
    return data;
}
