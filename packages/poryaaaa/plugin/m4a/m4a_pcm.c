#include "m4a_internal.h"

#include <string.h>

/* SoundMainRAM (vanilla Sappy m4a) — pokeemerald m4a_1.s.
 *
 * Per VBlank: walk every active PCM channel, fetch + interpolate samples
 * from the voice's WaveData using the channel's 23-bit fractional
 * accumulator, multiply by envelopeVolumeLeft/Right, and sum into an int16
 * stereo mix buffer.  A bounded fractional accumulator selects each block's
 * floor/ceil frame count, so the configured PCM rate is exact over a long
 * run.  After mix:
 *   1) SoundMainRAM_Reverb runs in-place on the int16 buffer.
 *   2) The buffer is clamped to int8 and written into the public
 *      M4APcmRing at write_cursor.
 * Routing of L/R to the two ring sides (FIFO_A / FIFO_B) follows the
 * dma_a_enable_left/right + dma_b_enable_left/right bits in the
 * register file. */

static void pcm_synth_pulse_update(M4ADriverPcmChan* ch)
{
    const uint8_t* config = (const uint8_t*)ch->wav->data;
    uint32_t lfo = (uint32_t)ch->count + ((uint32_t)config[3] << 24);
    ch->count = (int32_t)lfo;
    uint32_t folded = lfo + ((uint32_t)config[5] << 24);
    if ((int32_t)folded < 0)
        folded = ~folded;
    ch->synthPulseDuty = ((uint32_t)config[2] << 24) + (folded >> 8) * config[4];
}

/* PCM channel start — pokeemerald m4a.c equivalent. */
void m4a_drv_pcm_start(M4ADriverPcmChan* ch, WaveData* wav, uint8_t type, uint32_t startOffset)
{
    ch->wav = wav;
    ch->type = type;
    ch->currentPointer = NULL;
    ch->sampleStored = 0;
    ch->count = 0;
    ch->fw = 0;
    ch->envelopeVolume = 0;
    ch->envelopeVolumeRight = 0;
    ch->envelopeVolumeLeft = 0;
    ch->isLoop = false;
    ch->loopStart = NULL;
    ch->loopLen = 0;
    ch->synthType = 0;
    ch->synthPulseDuty = 0;

    if (!wav)
    {
        ch->status = 0;
        return;
    }

    if (wav->size == 0 && wav->data)
    {
        const uint8_t waveType = (uint8_t)wav->data[1];
        ch->currentPointer = wav->data;
        ch->synthType = waveType == 0 ? 1 : waveType == 1 ? 2 : 3;
        if (waveType == 2)
            ch->fw = 0x40000000u;
        if (ch->synthType == 1)
            pcm_synth_pulse_update(ch);
    }
    else
    {
        if (startOffset > wav->size)
            startOffset = wav->size;
        if (wav->data)
        {
            ch->currentPointer =
                (type & VOICE_TYPE_REV) ? wav->data + wav->size - startOffset : wav->data + startOffset;
            if ((wav->status & 0xC000) && wav->loopStart < wav->size)
            {
                ch->isLoop = true;
                ch->loopStart = wav->data + wav->loopStart;
                ch->loopLen = (int32_t)wav->size - (int32_t)wav->loopStart;
            }
        }
        ch->count = (int32_t)(wav->size - startOffset);
    }

    ch->status = M4A_CHN_ON | M4A_CHN_ENV_ATTACK;
    if (ch->isLoop)
        ch->status |= M4A_CHN_LOOP;
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

    const uint32_t masterProduct = (uint32_t)(masterVolume + 1) * envVol;
    const uint32_t rightVolume = ((uint32_t)ch->rightVolume * masterProduct) >> 13;
    const uint32_t leftVolume = ((uint32_t)ch->leftVolume * masterProduct) >> 13;
    ch->envelopeVolumeRight = (uint8_t)rightVolume;
    ch->envelopeVolumeLeft = (uint8_t)leftVolume;
    if (ch->synthType == 1 && ch->wav && ch->wav->data)
        pcm_synth_pulse_update(ch);
}

static int pcm_next_frame_size(M4ADriver* drv)
{
    const uint64_t numerator = (uint64_t)drv->pcm_rate_hz * M4A_PCM_VBLANK_RATE_DENOMINATOR;
    const uint64_t total = (uint64_t)drv->pcm_vblank_remainder + numerator;
    uint32_t samples = (uint32_t)(total / M4A_PCM_VBLANK_RATE_NUMERATOR);
    drv->pcm_vblank_remainder = (uint32_t)(total % M4A_PCM_VBLANK_RATE_NUMERATOR);
    if (samples > drv->pcm_max_samples_per_vblank)
        samples = drv->pcm_max_samples_per_vblank;
    return (int)samples;
}

static int pcm_active_dma_buf_size(const M4ADriver* drv)
{
    uint32_t size = drv->pcm_dma_buf_size;
    if (size == 0 || size > M4A_PCM_MAX_DMA_BUF_SIZE)
        size = M4A_PCM_DMA_BUF_SIZE;
    return (int)size;
}

/* SoundMainRAM_Reverb (vanilla Sappy m4a) — separate post-pass on the
 * int16 mix buffer.  4-tap algorithm:
 *   sum = L[pos] + R[pos] + L[pos+frameSize] + R[pos+frameSize]
 *   wet = (sum * amount) >> 9
 *   L[pos] += wet;  R[pos] += wet;
 *   write L[pos], R[pos] back into the delay line.
 * frameSize is the active one-vblank PCM block; the circular buffer
 * retains the canonical DMA buffer duration at that PCM rate. */
static void sound_main_ram_reverb(M4ADriver* drv, int frameSize)
{
    if (drv->reverb_amount == 0)
        return;

    const int bufSize = pcm_active_dma_buf_size(drv);
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

/* Accumulate with the ARM mixer's packed 00LL00RR MLA semantics.  Unsigned
 * arithmetic makes the required 32-bit wraparound explicit. */
static void mix_pcm_sample(int32_t sample, uint32_t packedVolume, uint32_t* mix, int index)
{
    mix[index] += packedVolume * (uint32_t)sample;
}

/* Convert one wrapped ARM word to its two's-complement signed value. */
static int32_t pcm_signed_word(uint32_t value)
{
    if (value <= INT32_MAX)
        return (int32_t)value;
    return (int32_t)((int64_t)value - 0x100000000LL);
}

/* Quantize the packed mixer exactly as SoundMainRAM: clamp each lane's
 * numerator to 30 signed bits, emit its high byte, and carry seven discarded
 * bits into the next sample. */
static void downsample_pcm_block(M4ADriver* drv, int frameSize)
{
    uint8_t remainderL = drv->pcm_noise_shape_left;
    uint8_t remainderR = drv->pcm_noise_shape_right;
    for (int i = 0; i < frameSize; i++)
    {
        const uint32_t packed = drv->pcmMixPacked[i];
        int32_t leftNumerator = pcm_signed_word(packed + ((uint32_t)remainderL << 16));
        int32_t rightNumerator = pcm_signed_word((packed << 16) + ((uint32_t)remainderR << 16));
        if (leftNumerator > 0x3FFFFFFF)
            leftNumerator = 0x3FFFFFFF;
        else if (leftNumerator < -0x40000000)
            leftNumerator = -0x40000000;
        if (rightNumerator > 0x3FFFFFFF)
            rightNumerator = 0x3FFFFFFF;
        else if (rightNumerator < -0x40000000)
            rightNumerator = -0x40000000;

        drv->pcmMixL[i] = (int8_t)((uint32_t)leftNumerator >> 23);
        drv->pcmMixR[i] = (int8_t)((uint32_t)rightNumerator >> 23);
        remainderL = (uint8_t)(((uint32_t)leftNumerator >> 16) & 0x7Fu);
        remainderR = (uint8_t)(((uint32_t)rightNumerator >> 16) & 0x7Fu);
    }
    drv->pcm_noise_shape_left = remainderL;
    drv->pcm_noise_shape_right = remainderR;
}

typedef struct
{
    int8_t* ptr;       /* current source byte in playback order */
    int8_t* loopFirst; /* first source byte after loop wrap */
    int32_t count;     /* playback-order samples remaining before stop/wrap */
    int32_t countBase; /* converts playback-order count back to ch->count */
    int32_t loopLen;
    int32_t step;
    int32_t pointerBias;      /* converts current source byte back to ch->currentPointer */
    int8_t loopPrevious;      /* final byte before an exact one-step wrap */
    int8_t loopEntryPrevious; /* byte before loopFirst for a multi-step wrap */
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
    source.loopPrevious = ch->wav && ch->wav->data && ch->wav->size ? ch->wav->data[ch->wav->size - 1] : 0;
    source.loopEntryPrevious = ch->wav ? (int8_t)(ch->wav->size >> 24) : 0;
    source.endPaddingSample = 0;
    source.looped = ch->isLoop && ch->loopLen > 0 && ch->wav;
    source.padEnd = false;
    if (source.looped && ch->loopStart > ch->wav->data)
        source.loopEntryPrevious = ch->loopStart[-1];
    source.endedWithPadding = false;

    if (ch->type & VOICE_TYPE_REV)
    {
        const int32_t loopStartOffset = source.looped ? (int32_t)(ch->loopStart - ch->wav->data) : 0;
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
            /* The ARM loop relocates r3 before its multi-byte reload.  An
             * exact wrap therefore uses loopStart[-1] unless the advance was
             * one byte, whose ADDEQ path retains the sample's final byte. */
            if (!fixed)
            {
                if (source->count == source->loopLen)
                    *sampleStored = source->step > 0 && advance > 1 ? source->loopEntryPrevious : source->loopPrevious;
                else
                    *sampleStored = source->ptr[-source->step];
            }
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

static void render_synth_channel(M4ADriverPcmChan* ch, uint32_t* mix, int frameSize)
{
    uint32_t phase = ch->fw;
    const uint32_t step = ch->frequency << 3;
    uint32_t packedVolume = ((uint32_t)ch->envelopeVolumeLeft << 16) | (uint32_t)ch->envelopeVolumeRight;
    if (ch->synthType == 2)
    {
        packedVolume = ((uint32_t)(ch->envelopeVolumeLeft >> 1) << 16) | (uint32_t)(ch->envelopeVolumeRight >> 1);
    }

    for (int i = 0; i < frameSize; i++)
    {
        int32_t value;
        if (ch->synthType == 1)
        {
            value = phase < ch->synthPulseDuty ? 64 : -64;
            phase += step;
        }
        else if (ch->synthType == 2)
        {
            phase += step;
            const int32_t raw = (int32_t)(phase >> 24) - 0x70 - (int32_t)((phase << 1) >> 27);
            ch->count = raw + (ch->count >> 1);
            value = ch->count >> 1;
        }
        else
        {
            phase += step;
            value = (int32_t)phase < 0 ? 0x180 - (int32_t)(phase >> 23) : (int32_t)(phase >> 23) - 0x80;
        }
        mix_pcm_sample(value, packedVolume, mix, i);
    }
    ch->fw = phase;
}

/* Render one PCM channel into the mix buffer.  Reverse voices are mapped
 * into a playback-order cursor before the common mixer sees them, keeping
 * interpolation, loop/end handling, and sample-store updates in one path. */
static void render_channel(M4ADriverPcmChan* ch, uint32_t* mix, int frameSize)
{
    if (!(ch->status & M4A_CHN_ON) || (ch->status & M4A_CHN_START))
        return;
    if (ch->synthType != 0)
    {
        if (ch->wav && ch->wav->data)
            render_synth_channel(ch, mix, frameSize);
        return;
    }
    if (!ch->wav || !ch->currentPointer || ch->count <= 0)
        return;
    /* Preserve the compatibility engine's packed-volume gate: an inaudible
     * attack does not consume source data before its envelope opens. */
    if (ch->envelopeVolumeRight == 0 && ch->envelopeVolumeLeft == 0)
        return;

    PcmSource source = pcm_source_from_channel(ch);
    int32_t sampleStored = ch->sampleStored;
    uint32_t fw = ch->fw;
    const bool fixed = (ch->type & VOICE_TYPE_FIX) != 0;
    const uint32_t freq = ch->frequency;
    const uint64_t requiredAdvance = ((uint64_t)fw + (uint64_t)freq * (uint32_t)frameSize) >> 23;
    const bool fastMixing = !fixed && !(ch->type & VOICE_TYPE_REV) && !(ch->wav->type & 0x80u) && source.count > 0 &&
                            (uint64_t)source.count > requiredAdvance;
    uint32_t packedVolume = ((uint32_t)ch->envelopeVolumeLeft << 16) | (uint32_t)ch->envelopeVolumeRight;
    if (!fixed)
    {
        const uint32_t carry = packedVolume & 1u;
        packedVolume = (packedVolume >> 1) + 0x8000u + carry;
        packedVolume &= ~0x8000u;
    }
    if (fastMixing && freq >= 0x800000u)
        packedVolume <<= 1;
    bool stopped = false;
    int32_t fastInterpolationScratch = 0;
    bool skipFastMultiply = false;

    if (source.count <= 0)
        return;

    for (int i = 0; i < frameSize; i++)
    {
        const int32_t sourceSample = pcm_source_current_sample(&source);
        int32_t sample;
        if (fixed)
        {
            sample = sourceSample;
        }
        else if (fastMixing)
        {
            const int32_t diff = sourceSample - sampleStored;
            if ((i & 7) == 0 || !skipFastMultiply)
                fastInterpolationScratch = pcm_signed_word(fw * (uint32_t)diff);
            if (freq >= 0x800000u)
                sample = sampleStored + (fastInterpolationScratch >> 23);
            else
                sample = sampleStored * 2 + (fastInterpolationScratch >> 22);

            /* r9 retains the prior interpolated sample only when the source
             * advanced to an equal byte.  A phase-only tick leaves NE set and
             * must multiply again, even when the current difference is zero. */
            fastInterpolationScratch = sample;
        }
        else
        {
            const int32_t diff = sourceSample - sampleStored;
            const int64_t interpolated = (int64_t)diff * (int32_t)fw;
            sample = sampleStored * 2 + (int32_t)(interpolated >> 22);
        }

        mix_pcm_sample(sample, packedVolume, mix, i);
        fw += freq;
        const uint32_t advance = fw >> 23;
        if (advance)
        {
            fw &= 0x7FFFFF;
            if (!pcm_source_advance(&source, advance, fixed, &sampleStored, &stopped))
                break;
        }
        skipFastMultiply = fastMixing && advance != 0 && pcm_source_current_sample(&source) == sampleStored;
    }

    ch->currentPointer = source.endedWithPadding ? ch->wav->data : source.ptr + source.pointerBias;
    ch->sampleStored = (int8_t)sampleStored;
    ch->fw = fw;
    ch->count = source.endedWithPadding ? 0 : source.countBase + source.count;
    if (stopped)
        ch->status = 0;
}

/* Render and publish one scheduled SoundMain PCM block. */
static void render_pcm_block(M4ADriver* drv)
{
    const int frameSize = pcm_next_frame_size(drv);
    const int bufSize = pcm_active_dma_buf_size(drv);

    /* 1. Tick the envelope before expiring a gate for the next tick. */
    for (int i = 0; i < M4A_MAX_PCM_CHANNELS; i++)
    {
        M4ADriverPcmChan* ch = &drv->pcmChans[i];
        pcm_channel_tick(ch, drv->master_volume);
        if (ch->gateTime > 0)
        {
            ch->gateTime--;
            if (ch->gateTime == 0)
                ch->status |= M4A_CHN_STOP;
        }
    }

    /* 2. Zero SoundMainRAM's packed 00LL00RR scratch words. */
    memset(drv->pcmMixPacked, 0, (size_t)frameSize * sizeof(*drv->pcmMixPacked));

    /* 3. Mix every active channel. */
    for (int i = 0; i < M4A_MAX_PCM_CHANNELS; i++)
        render_channel(&drv->pcmChans[i], drv->pcmMixPacked, frameSize);

    /* 4. Split and quantize the packed words into the two FIFO lanes. */
    downsample_pcm_block(drv, frameSize);

    /* 5. SoundMainRAM_Reverb in-place on the quantized mix. */
    sound_main_ram_reverb(drv, frameSize);

    /* 6. Clamp to int8 and write into M4APcmRing at write_cursor.
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
     * So m4a writes the right mix into the FIFO_A side and the left mix
     * into the FIFO_B side.  We mirror that convention here: ring_a =
     * right mix, ring_b = left mix.  Single source of truth for routing is
     * now hw_pcm_render's reading of dma_*_enable_* — driver never sees
     * those bits. */
    uint32_t segment = 0u;
    if (drv->pcm_dma_counter > 1u && drv->pcm_dma_counter <= drv->pcm_dma_period)
        segment = (uint32_t)drv->pcm_dma_period - ((uint32_t)drv->pcm_dma_counter - 1u);
    const uint64_t base = (uint64_t)segment * drv->pcm_max_samples_per_vblank % (uint32_t)bufSize;
    for (int i = 0; i < frameSize; i++)
    {
        /* The error-carrying downsampler has already reduced each packed
         * mixer lane to the FIFO byte scale. */
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

        const size_t idx = (size_t)((base + (uint64_t)i) % (uint64_t)bufSize);
        drv->pcm.ring_a[idx] = (int8_t)rr; /* FIFO_A: right mix */
        drv->pcm.ring_b[idx] = (int8_t)l;  /* FIFO_B: left  mix */
    }
    if (frameSize > 0)
    {
        drv->pcm.write_cursor += (uint32_t)frameSize;
        drv->pcm.pcm_samples_per_vblank = (uint32_t)frameSize;
        drv->pcm.pcm_dma_buf_size = (uint32_t)bufSize;

        /* The ring remains the driver's circular software-mix/DMA source.
         * m4a_advance emits canonical FIFO words only when its timer-driven
         * DMA model refills hardware; publication is not a hardware event. */
    }
}

/* SoundMainRAM — vanilla Sappy m4a per-vblank PCM mixer. */
void m4a_sound_main_ram(M4ADriver* drv)
{
    if (!drv)
        return;
    render_pcm_block(drv);
}
