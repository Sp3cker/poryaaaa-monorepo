#include "m4a_internal.h"

#include <string.h>

/* SoundMainRAM (vanilla Sappy m4a) — pokeemerald m4a_1.s.
 *
 * Per VBlank: walk every active PCM channel, fetch + interpolate samples
 * from the voice's WaveData using the channel's 23-bit fractional
 * accumulator, multiply by envelopeVolumeLeft/Right, sum into an int16
 * stereo mix buffer of M4A_PCM_SAMPLES_PER_VBLANK frames at PCM_RATE_HZ
 * (≈ 13379 Hz for Pokemon Emerald's sound mode).  After mix:
 *   1) SoundMainRAM_Reverb runs in-place on the int16 buffer.
 *   2) The buffer is clamped to int8 and written into the public
 *      M4APcmRing at write_cursor.
 * Routing of L/R to the two ring sides (FIFO_A / FIFO_B) follows the
 * dma_a_enable_left/right + dma_b_enable_left/right bits in the
 * register file. */

/* PCM channel start — pokeemerald m4a.c equivalent. */
void m4a_drv_pcm_start(M4ADriverPcmChan* ch, WaveData* wav, uint8_t type, uint32_t startOffset)
{
    /* xcmd 0D starts DirectSound playback inside the source sample.  Reverse
     * voices still keep m4a's one-past cursor convention, so the same offset
     * is measured back from the sample end. */
    if (startOffset > wav->size)
        startOffset = wav->size;

    ch->wav = wav;
    ch->type = type;
    ch->currentPointer = (type & VOICE_TYPE_REV) ? wav->data + wav->size - startOffset : wav->data + startOffset;
    ch->sampleStored = 0;
    ch->count = (int32_t)(wav->size - startOffset);
    ch->fw = 0;
    ch->envelopeVolume = 0;

    /* Loop bits: WaveData status 0xC000 indicates a looping sample. */
    ch->isLoop = (wav->status & 0xC000) != 0;
    if (ch->isLoop)
    {
        ch->loopStart = wav->data + wav->loopStart;
        ch->loopLen = (int32_t)wav->size - (int32_t)wav->loopStart;
        if (ch->loopLen <= 0)
        {
            ch->isLoop = false;
            ch->loopLen = 0;
        }
    }
    else
    {
        ch->loopStart = NULL;
        ch->loopLen = 0;
    }

    ch->status = M4A_CHN_ON | M4A_CHN_ENV_ATTACK;
    if (ch->isLoop)
        ch->status |= M4A_CHN_LOOP;

    /* Pre-compute attack-step-1 envelope so the very first vblank's mix
     * has audible signal — same trick as v1, justified by the gap
     * between m4a's vblank cadence and the chip's per-sample read. */
    uint32_t envVol = ch->attack;
    if (envVol >= 0xFF)
    {
        envVol = 0xFF;
        ch->status = M4A_CHN_ON | M4A_CHN_ENV_DECAY | (ch->isLoop ? M4A_CHN_LOOP : 0);
    }
    ch->envelopeVolume = (uint8_t)envVol;
}

static bool pcm_can_pseudo_echo(const M4ADriverPcmChan* ch)
{
    return ch->pseudoEchoVolume != 0 && ch->pseudoEchoLength != 0;
}

/* Per-vblank envelope tick for a PCM channel.  Mirrors v1
 * m4a_pcm_channel_tick / pokeemerald SoundMainRAM envelope state machine.
 * Updates envelopeVolume + envelopeVolumeLeft/Right; the mixer reads the
 * derived L/R values per output sample. */
static void pcm_channel_tick(M4ADriverPcmChan* ch, uint8_t masterVolume)
{
    if (!(ch->status & M4A_CHN_ON))
        return;

    uint8_t envVol = ch->envelopeVolume;

    if (ch->status & M4A_CHN_START)
    {
        if (ch->status & M4A_CHN_STOP)
        {
            ch->status = 0;
            return;
        }
        ch->status = M4A_CHN_ON | M4A_CHN_ENV_ATTACK;
        if (ch->isLoop)
            ch->status |= M4A_CHN_LOOP;
        envVol = 0;
        ch->sampleStored = 0;
        ch->fw = 0;
    }

    if (ch->status & M4A_CHN_IEC)
    {
        if (ch->pseudoEchoLength == 0)
        {
            ch->status = 0;
            return;
        }
        ch->pseudoEchoLength--;
        if (ch->pseudoEchoLength == 0)
        {
            ch->status = 0;
            return;
        }
    }
    else if (ch->status & M4A_CHN_STOP)
    {
        envVol = (uint8_t)(((uint32_t)envVol * ch->release) >> 8);
        if (envVol <= ch->pseudoEchoVolume)
        {
            if (!pcm_can_pseudo_echo(ch))
            {
                ch->status = 0;
                return;
            }
            envVol = ch->pseudoEchoVolume;
            ch->status |= M4A_CHN_IEC;
        }
    }
    else
    {
        uint8_t envState = ch->status & M4A_CHN_ENV_MASK;
        if (envState == M4A_CHN_ENV_DECAY)
        {
            envVol = (uint8_t)(((uint32_t)envVol * ch->decay) >> 8);
            if (envVol <= ch->sustain)
            {
                envVol = ch->sustain;
                if (envVol == 0)
                {
                    if (!pcm_can_pseudo_echo(ch))
                    {
                        ch->status = 0;
                        return;
                    }
                    envVol = ch->pseudoEchoVolume;
                    ch->status = (ch->status & ~M4A_CHN_ENV_MASK) | M4A_CHN_IEC;
                }
                else
                {
                    ch->status--; /* DECAY → SUSTAIN */
                }
            }
        }
        else if (envState == M4A_CHN_ENV_ATTACK)
        {
            uint32_t sum = (uint32_t)envVol + ch->attack;
            if (sum >= 0xFF)
            {
                envVol = 0xFF;
                ch->status--; /* ATTACK → DECAY */
            }
            else
            {
                envVol = (uint8_t)sum;
            }
        }
        /* SUSTAIN: envVol stays put. */
    }

    ch->envelopeVolume = envVol;

    uint32_t vol = ((uint32_t)(masterVolume + 1) * envVol) >> 4;
    ch->envelopeVolumeRight = (uint8_t)(((uint32_t)ch->rightVolume * vol) >> 8);
    ch->envelopeVolumeLeft = (uint8_t)(((uint32_t)ch->leftVolume * vol) >> 8);
}

/* SoundMainRAM_Reverb (vanilla Sappy m4a) — separate post-pass on the
 * int16 mix buffer.  4-tap algorithm:
 *   sum = L[pos] + R[pos] + L[pos+frameSize] + R[pos+frameSize]
 *   wet = (sum * amount) >> 9
 *   L[pos] += wet;  R[pos] += wet;
 *   write L[pos], R[pos] back into the delay line.
 * frameSize = M4A_PCM_SAMPLES_PER_VBLANK (one vblank ahead in the
 * circular buffer = canonical 1-frame "other" tap from m4a_1.s). */
static void sound_main_ram_reverb(M4ADriver* drv)
{
    if (drv->reverb_amount == 0)
        return;

    const int frameSize = M4A_PCM_SAMPLES_PER_VBLANK;
    const int bufSize = M4A_PCM_DMA_BUF_SIZE;
    uint8_t amount = drv->reverb_amount;
    uint16_t pos = drv->reverbPos;

    for (int i = 0; i < frameSize; i++)
    {
        uint16_t otherPos = (uint16_t)((pos + frameSize) % bufSize);

        /* int8 → int32 sign extension on the four taps. */
        int32_t sum = (int32_t)drv->reverbBufL[pos] + (int32_t)drv->reverbBufR[pos] +
                      (int32_t)drv->reverbBufL[otherPos] + (int32_t)drv->reverbBufR[otherPos];

        int32_t wet = (sum * amount) >> 9;

        int32_t outL = (int32_t)drv->pcmMixL[i] + wet;
        int32_t outR = (int32_t)drv->pcmMixR[i] + wet;
        /* int16 saturation for the mix-buffer staging — keeps
         * headroom available if downstream code ever wants to
         * accumulate more.  Today this is a near-no-op since per-
         * channel contributions are already int8-range, but the
         * type stays int16 to stay consistent with v1's int32 mix. */
        if (outL > 32767)
            outL = 32767;
        else if (outL < -32768)
            outL = -32768;
        if (outR > 32767)
            outR = 32767;
        else if (outR < -32768)
            outR = -32768;
        drv->pcmMixL[i] = (int16_t)outL;
        drv->pcmMixR[i] = (int16_t)outR;

        /* Delay line: clamp to int8 RANGE before writeback (v1
         * parity).  Real m4a's reverb buffer IS the int8 FIFO buffer,
         * so future tap reads see the same int8-clamped values that
         * would have been DMA'd to the chip.  Storing int16 here
         * would diverge on heavy mixes where pcmMix briefly exceeds
         * [-128, 127] before the final clamp-to-int8 stage. */
        int32_t delayL = outL;
        int32_t delayR = outR;
        if (delayL > 127)
            delayL = 127;
        else if (delayL < -128)
            delayL = -128;
        if (delayR > 127)
            delayR = 127;
        else if (delayR < -128)
            delayR = -128;
        drv->reverbBufL[pos] = (int8_t)delayL;
        drv->reverbBufR[pos] = (int8_t)delayR;

        pos++;
        if (pos >= bufSize)
            pos = 0;
    }
    drv->reverbPos = pos;
}

static void mix_pcm_sample(int32_t sample, int32_t envR, int32_t envL, int16_t* mixL, int16_t* mixR, int index)
{
    /* Sum scaled sample (×env >> 8) into the int16 mix buffer with
     * saturation so loud chords don't wrap. */
    int32_t addR = (sample * envR) >> 8;
    int32_t addL = (sample * envL) >> 8;
    int32_t newR = (int32_t)mixR[index] + addR;
    int32_t newL = (int32_t)mixL[index] + addL;
    if (newR > 32767)
        newR = 32767;
    else if (newR < -32768)
        newR = -32768;
    if (newL > 32767)
        newL = 32767;
    else if (newL < -32768)
        newL = -32768;
    mixR[index] = (int16_t)newR;
    mixL[index] = (int16_t)newL;
}

typedef struct
{
    int8_t* ptr;       /* current source byte in playback order */
    int8_t* loopFirst; /* first source byte after loop wrap */
    int32_t count;     /* playback-order samples remaining before stop/wrap */
    int32_t countBase; /* converts playback-order count back to ch->count */
    int32_t loopLen;
    int32_t step;
    int32_t pointerBias; /* converts current source byte back to ch->currentPointer */
    int8_t loopPrevious; /* source byte immediately before loopFirst in playback order */
    int8_t endPaddingSample;
    bool looped;
    bool padEnd;
    bool endedWithPadding;
} PcmSource;

/* Normalize forward and reverse DirectSound channels into playback order so
 * start offsets, loop wrap, and interpolation share one update path. */
static PcmSource pcm_source_from_channel(M4ADriverPcmChan* ch)
{
    PcmSource source;
    source.ptr = ch->currentPointer;
    source.loopFirst = ch->loopStart;
    source.count = ch->count;
    source.countBase = 0;
    source.loopLen = ch->loopLen;
    source.step = 1;
    source.pointerBias = 0;
    source.loopPrevious = ch->wav ? ch->wav->data[ch->wav->size - 1] : 0;
    source.endPaddingSample = 0;
    source.looped = ch->isLoop && ch->loopLen > 0 && ch->wav;
    source.padEnd = false;
    source.endedWithPadding = false;

    if (ch->type & VOICE_TYPE_REV)
    {
        int32_t loopStartOffset = source.looped ? (int32_t)(ch->loopStart - ch->wav->data) : 0;
        source.ptr = ch->currentPointer - 1;
        source.loopFirst = ch->wav->data + ch->wav->size - 1;
        source.count = ch->count - loopStartOffset;
        source.countBase = loopStartOffset;
        source.step = -1;
        source.pointerBias = 1;
        source.loopPrevious = source.looped ? ch->loopStart[0] : 0;
        source.padEnd = true;
    }

    return source;
}

static int32_t pcm_source_current_sample(const PcmSource* source)
{
    return source->ptr[0];
}

static bool pcm_source_advance(PcmSource* source, uint32_t advance, bool fixed, int32_t* sampleStored, bool* stopped)
{
    if (advance == 0)
        return true;

    source->count -= (int32_t)advance;
    if (source->count <= 0)
    {
        if (source->looped)
        {
            while (source->count <= 0)
                source->count += source->loopLen;
            source->ptr = source->loopFirst + (source->loopLen - source->count) * source->step;
            if (!fixed)
                *sampleStored = source->count == source->loopLen ? source->loopPrevious : source->ptr[-source->step];
        }
        else
        {
            *stopped = true;
            if (source->padEnd && !fixed)
            {
                /* A resampled reverse voice needs one synthetic zero byte past
                 * the sample start so interpolation can emit its final source
                 * byte before the channel stops. */
                *sampleStored = source->ptr[0];
                source->ptr = &source->endPaddingSample;
                source->step = 0;
                source->count = 0x3FFFFFFF;
                source->endedWithPadding = true;
                return true;
            }
            return false;
        }
    }
    else
    {
        source->ptr += (int32_t)advance * source->step;
        if (!fixed)
            *sampleStored = source->ptr[-source->step];
    }
    return true;
}

/* Render one PCM channel into the mix buffer.  Reverse voices are mapped
 * into a playback-order cursor before the common mixer sees them, keeping
 * interpolation, loop/end handling, and sample-store updates in one path. */
static void render_channel(M4ADriverPcmChan* ch, int16_t* mixL, int16_t* mixR)
{
    if (!(ch->status & M4A_CHN_ON) || (ch->status & M4A_CHN_START))
        return;
    if (!ch->wav || !ch->currentPointer || ch->count <= 0)
        return;

    PcmSource source = pcm_source_from_channel(ch);
    int32_t sampleStored = ch->sampleStored;
    uint32_t fw = ch->fw;
    bool fixed = (ch->type & VOICE_TYPE_FIX) != 0;
    uint32_t freq = ch->frequency;
    int32_t envR = ch->envelopeVolumeRight;
    int32_t envL = ch->envelopeVolumeLeft;
    bool stopped = false;

    if (source.count <= 0)
        return;

    for (int i = 0; i < M4A_PCM_SAMPLES_PER_VBLANK; i++)
    {
        int32_t sourceSample = pcm_source_current_sample(&source);
        int32_t sample;
        if (fixed)
        {
            sample = sourceSample;
        }
        else
        {
            int32_t s0 = sampleStored;
            int32_t diff = sourceSample - s0;
            sample = s0 + (int32_t)(((int64_t)diff * (int32_t)fw) >> 23);
        }

        mix_pcm_sample(sample, envR, envL, mixL, mixR, i);

        fw += freq;
        uint32_t advance = fw >> 23;
        if (advance)
        {
            fw &= 0x7FFFFF;
            if (!pcm_source_advance(&source, advance, fixed, &sampleStored, &stopped))
            {
                break;
            }
        }
    }

    ch->currentPointer = source.endedWithPadding ? ch->wav->data : source.ptr + source.pointerBias;
    ch->sampleStored = (int8_t)sampleStored;
    ch->fw = fw;
    ch->count = source.endedWithPadding ? 0 : source.countBase + source.count;
    if (stopped)
        ch->status = 0;
}

/* SoundMainRAM — vanilla Sappy m4a per-vblank PCM mixer. */
void m4a_sound_main_ram(M4ADriver* drv)
{
    if (!drv)
        return;

    /* 1. Envelope tick on every active channel. */
    for (int i = 0; i < M4A_MAX_PCM_CHANNELS; i++)
    {
        M4ADriverPcmChan* ch = &drv->pcmChans[i];
        if (ch->gateTime > 0)
        {
            ch->gateTime--;
            if (ch->gateTime == 0)
                ch->status |= M4A_CHN_STOP;
        }
        pcm_channel_tick(ch, drv->master_volume);
    }

    /* 2. Zero the mix buffer.  SoundMainRAM accumulates per channel
     * into this scratch — not the ring directly, since reverb runs
     * before clamp. */
    memset(drv->pcmMixL, 0, sizeof(drv->pcmMixL));
    memset(drv->pcmMixR, 0, sizeof(drv->pcmMixR));

    /* 3. Mix every active channel. */
    for (int i = 0; i < M4A_MAX_PCM_CHANNELS; i++)
        render_channel(&drv->pcmChans[i], drv->pcmMixL, drv->pcmMixR);

    /* 4. SoundMainRAM_Reverb in-place on the int16 mix. */
    sound_main_ram_reverb(drv);

    /* 5. Clamp int16 → int8 and write into M4APcmRing at write_cursor.
     *
     * Routing layer separation per plan §6b: M4APcmRing.ring_a /
     * ring_b are the FIFO_A / FIFO_B mono byte streams.  The chip
     * applies SOUNDCNT_H DMA routing bits on render — driver does NOT
     * read them.  Real m4a hardcodes a fixed mix-to-FIFO mapping that
     * matches Pokemon Emerald's REG_SOUNDCNT_H setup at boot
     * (m4a.c:352–354):
     *     SOUND_A_RIGHT_OUTPUT | SOUND_B_LEFT_OUTPUT
     * → DMA_A is routed to right output, DMA_B to left.
     *
     * So m4a writes the right mix into the FIFO_A buffer
     * (gPcmDmaBuffer[0..1583]) and the left mix into the FIFO_B
     * buffer (gPcmDmaBuffer[1584..3167]).  We mirror that convention
     * here: ring_a = right mix, ring_b = left mix.  Single source of
     * truth for routing is now hw_pcm_render's reading of
     * dma_*_enable_* — driver never sees those bits. */
    uint64_t base = drv->pcm.write_cursor % M4A_PCM_DMA_BUF_SIZE;
    for (int i = 0; i < M4A_PCM_SAMPLES_PER_VBLANK; i++)
    {
        /* Per-channel contribution is already (sample × envVol) >> 8 →
         * int8-range; the int16 mix buffer is just saturation headroom
         * for stacking up to M4A_MAX_PCM_CHANNELS voices.  Clamp, don't
         * re-shift. */
        int32_t l = drv->pcmMixL[i];
        int32_t rr = drv->pcmMixR[i];
        if (l > 127)
            l = 127;
        else if (l < -128)
            l = -128;
        if (rr > 127)
            rr = 127;
        else if (rr < -128)
            rr = -128;

        size_t idx = (size_t)((base + (uint64_t)i) % M4A_PCM_DMA_BUF_SIZE);
        drv->pcm.ring_a[idx] = (int8_t)rr; /* FIFO_A: right mix */
        drv->pcm.ring_b[idx] = (int8_t)l;  /* FIFO_B: left  mix */
    }
    drv->pcm.write_cursor += M4A_PCM_SAMPLES_PER_VBLANK;
    drv->pcm.pcm_rate_hz = M4A_PCM_RATE_HZ;

    /* Publish gate: stamp this vblank's ring writes with the firing
     * sample_offset so the chip's hw_pcm only treats this block as
     * available from `event_vblank_offset` onwards within the
     * current render span.  Without this, the chip read clock would
     * see all of this advance call's ring writes from sample_offset
     * 0 — leaking post-vblank PCM into pre-vblank time. */
    m4a_internal_emit_event(drv, M4A_REG_PCM_PUBLISH, 0u);
}
