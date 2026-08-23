#include "vg_wav.h"

#include "vg_alloc.h"
#include "vg_log.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static WaveData* alloc_wavedata(uint32_t size)
{
    size_t dataBytes;
    size_t totalBytes;
    if (!vg_size_add((size_t)size, 1, &dataBytes))
        return NULL;
    if (!vg_size_add(sizeof(WaveData), dataBytes, &totalBytes))
        return NULL;

    WaveData* wd = malloc(totalBytes);
    if (!wd)
        return NULL;
    wd->data = (int8_t*)((uint8_t*)wd + sizeof(WaveData));
    return wd;
}
static uint16_t read_u16_be(const uint8_t* p)
{
    return (uint16_t)(((uint16_t)p[0] << 8) | p[1]);
}

static uint32_t read_u32_be(const uint8_t* p)
{
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3];
}

static double read_extended80(const uint8_t* bytes)
{
    int sign = (bytes[0] & 0x80) ? -1 : 1;
    int exponent = ((bytes[0] & 0x7f) << 8) | bytes[1];
    uint64_t mantissa = 0;
    for (int i = 0; i < 8; i++)
        mantissa = (mantissa << 8) | bytes[2 + i];
    if (exponent == 0 && mantissa == 0)
        return 0.0;
    return sign * ldexp((double)mantissa, exponent - 16383 - 63);
}

typedef struct
{
    uint16_t id;
    uint32_t position;
} AiffMarker;

/*
 * Decode the AIFF form emitted by the older pokeemerald sample pipeline.
 * AIFF PCM is signed and big-endian; matching aif2pcm means the final
 * frame is reserved as the GBA repeat byte.
 */
WaveData* vg_load_aif_file(const char* absoluteAifPath)
{
    FILE* f = fopen(absoluteAifPath, "rb");
    if (!f)
        return NULL;

    uint8_t formHeader[12];
    if (fread(formHeader, 1, sizeof(formHeader), f) != sizeof(formHeader) || memcmp(formHeader, "FORM", 4) != 0 ||
        memcmp(formHeader + 8, "AIFF", 4) != 0)
    {
        vg_err("invalid FORM/AIFF header in %s", absoluteAifPath);
        fclose(f);
        return NULL;
    }

    const long formEnd = 8 + (long)read_u32_be(formHeader + 4);
    int commFound = 0;
    int ssndFound = 0;
    uint32_t frameCount = 0;
    int sampleSize = 0;
    double sampleRate = 0.0;
    int haveSustainLoop = 0;
    uint16_t loopStartId = 0;
    uint16_t loopEndId = 0;
    AiffMarker* markers = NULL;
    size_t markerCount = 0;
    long ssndDataOffset = 0;
    uint32_t ssndDataBytes = 0;

    for (;;)
    {
        long chunkPosition = ftell(f);
        if (chunkPosition < 0 || chunkPosition + 8 > formEnd)
            break;
        uint8_t chunkHeader[8];
        if (fread(chunkHeader, 1, sizeof(chunkHeader), f) != sizeof(chunkHeader))
            break;
        uint32_t chunkLength = read_u32_be(chunkHeader + 4);
        long chunkDataStart = ftell(f);
        if (chunkDataStart < 0)
            break;

        if (memcmp(chunkHeader, "COMM", 4) == 0 && chunkLength >= 18)
        {
            uint8_t data[18];
            if (fread(data, 1, sizeof(data), f) == sizeof(data))
            {
                if (read_u16_be(data) != 1)
                {
                    vg_err("AIFF sample is not mono: %s", absoluteAifPath);
                    free(markers);
                    fclose(f);
                    return NULL;
                }
                sampleSize = (int)read_u16_be(data + 6);
                if (sampleSize != 8 && sampleSize != 16)
                {
                    vg_err("unsupported AIFF sample size in %s", absoluteAifPath);
                    free(markers);
                    fclose(f);
                    return NULL;
                }
                frameCount = read_u32_be(data + 2);
                sampleRate = read_extended80(data + 8);
                commFound = 1;
            }
        }
        else if (memcmp(chunkHeader, "MARK", 4) == 0 && chunkLength >= 2)
        {
            uint8_t countBytes[2];
            if (fread(countBytes, 1, sizeof(countBytes), f) == sizeof(countBytes))
            {
                uint16_t count = read_u16_be(countBytes);
                AiffMarker* parsed = NULL;
                if (count > 0)
                {
                    parsed = calloc(count, sizeof(*parsed));
                    if (!parsed)
                    {
                        free(markers);
                        fclose(f);
                        return NULL;
                    }
                }
                int complete = 1;
                for (uint16_t i = 0; i < count; i++)
                {
                    uint8_t markerData[6];
                    if (fread(markerData, 1, sizeof(markerData), f) != sizeof(markerData))
                    {
                        complete = 0;
                        break;
                    }
                    int nameLength = fgetc(f);
                    if (nameLength == EOF)
                    {
                        complete = 0;
                        break;
                    }
                    parsed[i].id = read_u16_be(markerData);
                    parsed[i].position = read_u32_be(markerData + 2);
                    if (fseek(f, nameLength + (nameLength & 1 ? 0 : 1), SEEK_CUR) != 0)
                    {
                        complete = 0;
                        break;
                    }
                }
                if (complete)
                {
                    free(markers);
                    markers = parsed;
                    markerCount = count;
                }
                else
                    free(parsed);
            }
        }
        else if (memcmp(chunkHeader, "INST", 4) == 0 && chunkLength >= 20)
        {
            uint8_t data[20];
            if (fread(data, 1, sizeof(data), f) == sizeof(data))
            {
                uint16_t loopType = read_u16_be(data + 8);
                if (loopType != 0)
                {
                    loopStartId = read_u16_be(data + 10);
                    loopEndId = read_u16_be(data + 12);
                    haveSustainLoop = 1;
                }
            }
        }
        else if (memcmp(chunkHeader, "SSND", 4) == 0 && chunkLength >= 8)
        {
            ssndDataOffset = chunkDataStart + 8;
            ssndDataBytes = chunkLength - 8;
            ssndFound = 1;
        }

        long nextChunk = chunkDataStart + (long)chunkLength;
        if (chunkLength & 1)
            nextChunk++;
        if (fseek(f, nextChunk, SEEK_SET) != 0)
            break;
    }

    if (!commFound || !ssndFound)
    {
        vg_err("missing COMM or SSND chunk in %s", absoluteAifPath);
        free(markers);
        fclose(f);
        return NULL;
    }

    int loopEnabled = 0;
    uint32_t loopStart = 0;
    uint32_t sampleCount = frameCount;
    if (haveSustainLoop)
    {
        for (size_t i = 0; i < markerCount; i++)
            if (markers[i].id == loopStartId)
            {
                loopStart = markers[i].position;
                loopEnabled = 1;
                break;
            }
        for (size_t i = 0; i < markerCount; i++)
            if (markers[i].id == loopEndId)
            {
                if (!loopEnabled || markers[i].position < loopStart)
                    loopStart = markers[i].position;
                sampleCount = markers[i].position;
                loopEnabled = 1;
                break;
            }
    }
    free(markers);

    uint32_t size = sampleCount > 0 ? sampleCount - 1 : 0;
    uint32_t bytesPerSample = sampleSize == 16 ? 2 : 1;
    uint32_t availableSamples = ssndDataBytes / bytesPerSample;
    if (size > availableSamples)
        size = availableSamples;
    if (size == 0)
    {
        fclose(f);
        return NULL;
    }
    WaveData* wd = alloc_wavedata(size);
    if (!wd)
    {
        fclose(f);
        return NULL;
    }
    wd->type = 0;
    wd->status = loopEnabled ? 0x4000 : 0;
    wd->freq = (uint32_t)(sampleRate * 1024.0);
    wd->loopStart = loopStart;
    wd->size = size;

    size_t rawBytes;
    if (!vg_size_mul((size_t)size, bytesPerSample, &rawBytes))
    {
        free(wd);
        fclose(f);
        return NULL;
    }
    uint8_t* raw = NULL;
    if (rawBytes > 0)
    {
        raw = malloc(rawBytes);
        if (!raw)
        {
            free(wd);
            fclose(f);
            return NULL;
        }
        size_t bytesRead = 0;
        if (fseek(f, ssndDataOffset, SEEK_SET) == 0)
            bytesRead = fread(raw, 1, rawBytes, f);
        if (bytesRead < rawBytes)
            memset(raw + bytesRead, 0, rawBytes - bytesRead);
        if (bytesPerSample == 1)
            memcpy(wd->data, raw, size);
        else
            for (uint32_t i = 0; i < size; i++)
                wd->data[i] = (int8_t)raw[(size_t)i * 2];
        free(raw);
    }
    wd->data[size] = size > 0 ? wd->data[size - 1] : 0;
    fclose(f);
    return wd;
}
