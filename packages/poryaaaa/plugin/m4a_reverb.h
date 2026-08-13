#pragma once

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    int8_t *buffer;
    int bufferSize;
    int frameSize;
    int pos;
    uint8_t amount;
} M4AReverb;

#define M4A_REVERB_GBA_BUFFER_SAMPLES 1584
#define M4A_REVERB_GBA_SAMPLE_RATE 13379.0f
#define M4A_REVERB_GBA_DMA_PERIOD 7

static inline void m4a_reverb_init(M4AReverb *reverb, float sampleRate, uint8_t amount)
{
    if (!reverb)
        return;
    int delayLen = (int)(M4A_REVERB_GBA_BUFFER_SAMPLES * sampleRate / M4A_REVERB_GBA_SAMPLE_RATE);
    if (delayLen < 1)
        delayLen = 1;
    reverb->bufferSize = delayLen;
    reverb->frameSize = delayLen / M4A_REVERB_GBA_DMA_PERIOD;
    if (reverb->frameSize < 1)
        reverb->frameSize = 1;
    reverb->buffer = (int8_t *)calloc((size_t)delayLen * 2u, sizeof(*reverb->buffer));
    reverb->pos = 0;
    reverb->amount = amount;
}

static inline void m4a_reverb_destroy(M4AReverb *reverb)
{
    if (!reverb)
        return;
    free(reverb->buffer);
    reverb->buffer = NULL;
    reverb->bufferSize = 0;
    reverb->frameSize = 0;
    reverb->pos = 0;
}

static inline void m4a_reverb_reset(M4AReverb *reverb)
{
    if (!reverb)
        return;
    if (reverb->buffer && reverb->bufferSize > 0)
        memset(reverb->buffer, 0, (size_t)reverb->bufferSize * 2u * sizeof(*reverb->buffer));
    reverb->pos = 0;
}

static inline void m4a_reverb_set_amount(M4AReverb *reverb, uint8_t amount)
{
    if (reverb)
        reverb->amount = amount;
}

static inline void m4a_reverb_process(M4AReverb *reverb, int32_t *sampleL, int32_t *sampleR)
{
    if (!reverb || !sampleL || !sampleR || !reverb->buffer || reverb->bufferSize <= 0 || reverb->amount == 0)
        return;
    const int pos = reverb->pos;
    const int idx = pos * 2;
    const int otherPos = (pos + reverb->frameSize) % reverb->bufferSize;
    const int otherIdx = otherPos * 2;
    const int32_t sum = reverb->buffer[idx] + reverb->buffer[idx + 1] + reverb->buffer[otherIdx]
                        + reverb->buffer[otherIdx + 1];
    const int32_t wet = (sum * reverb->amount) >> 9;
    *sampleL += wet;
    *sampleR += wet;
    int32_t writeL = *sampleL;
    int32_t writeR = *sampleR;
    if (writeL > 127)
        writeL = 127;
    else if (writeL < -128)
        writeL = -128;
    if (writeR > 127)
        writeR = 127;
    else if (writeR < -128)
        writeR = -128;
    reverb->buffer[idx] = (int8_t)writeL;
    reverb->buffer[idx + 1] = (int8_t)writeR;
    reverb->pos = (pos + 1) % reverb->bufferSize;
}
