#include "m4a_internal.h"
#include "m4a_pcm_internal.h"

#include <stddef.h>
#include <string.h>

#define SAPPY_TYPE_COMPRESSED 0x20u
#define SAPPY_TYPE_SPECIAL (VOICE_TYPE_REV | SAPPY_TYPE_COMPRESSED)

/* gDeltaEncodingTable at the pinned pokeemerald revision. */
static const int8_t kSappyDelta[16] = {
    0,
    1,
    4,
    9,
    16,
    25,
    36,
    49,
    -64,
    -49,
    -36,
    -25,
    -16,
    -9,
    -4,
    -1,
};

/* Reproduce ARM arithmetic shifts without relying on host signed-shift rules. */
static int32_t sappy_arshift(int64_t value, unsigned shift)
{
    if (value >= 0)
        return (int32_t)(value >> shift);
    return (int32_t)-(((-value) + ((INT64_C(1) << shift) - 1)) >> shift);
}

/* Convert a wrapped DMA byte to a signed host value. */
static int8_t sappy_signed_byte(uint8_t value)
{
    return value < 0x80u ? (int8_t)value : (int8_t)((int16_t)value - 0x100);
}

/* Decode the vanilla 33-byte DPAD block into the shared 64-byte cache. */
static void sappy_decode_block(M4ADriver* drv, M4ADriverPcmChan* channel, int32_t block)
{
    M4APcmSappyChannelState* state = &channel->sappy;
    M4APcmSappyGlobalState* global = &drv->pcmMixerState.sappy;
    const uint8_t* encoded = (const uint8_t*)channel->wav->data + (uint32_t)block * M4A_SAPPY_COMPRESSED_BLOCK_BYTES;
    uint8_t sample = encoded[0];
    global->decoding_buffer[0] = sappy_signed_byte(sample);
    sample = (uint8_t)(sample + kSappyDelta[encoded[1] & 0x0Fu]);
    global->decoding_buffer[1] = sappy_signed_byte(sample);
    uint32_t output = 2;
    for (uint32_t byte = 2; byte < M4A_SAPPY_COMPRESSED_BLOCK_BYTES; byte++)
    {
        const uint8_t packed = encoded[byte];
        sample = (uint8_t)(sample + kSappyDelta[packed >> 4]);
        global->decoding_buffer[output++] = sappy_signed_byte(sample);
        sample = (uint8_t)(sample + kSappyDelta[packed & 0x0Fu]);
        global->decoding_buffer[output++] = sappy_signed_byte(sample);
    }
    state->decoded_block = block;
}

/* Read one logical source sample through the source-owned cache when needed. */
static int32_t sappy_source_sample(M4ADriver* drv, M4ADriverPcmChan* channel, int32_t position)
{
    if (!channel->wav || !channel->wav->data)
        return 0;
    M4APcmSappyChannelState* state = &channel->sappy;
    if (!state->compressed)
    {
        if (position == -1)
            return sappy_signed_byte((uint8_t)(channel->wav->size >> 24));
        if (position < 0 || (uint32_t)position > channel->wav->size)
            return 0;
        return channel->wav->data[position];
    }
    if (position < 0 || (uint32_t)position >= channel->wav->size)
        return 0;
    const int32_t block = position / (int32_t)M4A_SAPPY_COMPRESSED_BLOCK_SAMPLES;
    if (state->decoded_block != block)
        sappy_decode_block(drv, channel, block);
    return drv->pcmMixerState.sappy.decoding_buffer[(uint32_t)position & (M4A_SAPPY_COMPRESSED_BLOCK_SAMPLES - 1u)];
}

/* Add one voice contribution exactly as the packed ARM byte-lane loop does. */
static void sappy_mix_sample(int32_t sample, uint8_t right_gain, uint8_t left_gain, int8_t* right, int8_t* left)
{
    const int32_t right_contribution = sappy_arshift((int64_t)sample * right_gain, 8);
    const int32_t left_contribution = sappy_arshift((int64_t)sample * left_gain, 8);
    *right = sappy_signed_byte((uint8_t)((uint8_t)*right + (uint8_t)right_contribution));
    *left = sappy_signed_byte((uint8_t)((uint8_t)*left + (uint8_t)left_contribution));
}

/* Seed final DMA bytes with vanilla zeroing or four-tap DMA-domain reverb. */
static void sappy_seed_output(M4ADriver* drv, const M4APcmBlockGeometry* geometry)
{
    M4APcmSappyGlobalState* global = &drv->pcmMixerState.sappy;
    if (drv->reverb_amount == 0)
    {
        memset(global->output_left, 0, geometry->frame_size);
        memset(global->output_right, 0, geometry->frame_size);
        return;
    }
    for (uint32_t i = 0; i < geometry->frame_size; i++)
    {
        const uint32_t current = (geometry->ring_base + i) % geometry->dma_buf_size;
        const uint32_t previous = (current + geometry->frame_size) % geometry->dma_buf_size;
        const int32_t sum = (int32_t)drv->pcm.ring_a[current] + drv->pcm.ring_b[current] + drv->pcm.ring_a[previous] +
                            drv->pcm.ring_b[previous];
        int32_t wet = sappy_arshift((int64_t)sum * drv->reverb_amount, 9);
        if (((uint32_t)wet & 0x80u) != 0)
            wet++;
        const int8_t seed = sappy_signed_byte((uint8_t)wet);
        global->output_right[i] = seed;
        global->output_left[i] = seed;
    }
}

/* Initialize native source state on the first SoundMain tick, then run ADSR. */
static void sappy_channel_tick(M4ADriverPcmChan* channel, uint8_t master_volume)
{
    if (!(channel->status & M4A_CHN_ON))
        return;
    M4APcmSappyChannelState* state = &channel->sappy;
    uint8_t envelope = channel->envelopeVolume;
    if (channel->status & M4A_CHN_START)
    {
        if ((channel->status & M4A_CHN_STOP) || !channel->wav || !channel->wav->data)
        {
            channel->status = 0;
            return;
        }
        const uint32_t offset = state->start_offset < channel->wav->size ? state->start_offset : channel->wav->size;
        channel->status = M4A_CHN_ENV_ATTACK;
        state->source_position = (int32_t)offset;
        state->count = (int32_t)(channel->wav->size - offset);
        state->fractional_position = 0;
        state->decoded_block = -1;
        state->special_initialized = (channel->type & SAPPY_TYPE_SPECIAL) != 0;
        state->compressed = state->special_initialized && channel->wav->type != 0;
        if ((channel->type & VOICE_TYPE_REV) != 0)
            state->source_position = (int32_t)(channel->wav->size - offset);
        if ((channel->wav->status & 0xC000u) != 0 && channel->wav->loopStart < channel->wav->size)
            channel->status |= M4A_CHN_LOOP;
        envelope = 0;
    }
    if (channel->status & M4A_CHN_IEC)
    {
        const uint8_t original_length = channel->pseudoEchoLength;
        channel->pseudoEchoLength--;
        if (original_length <= 1)
        {
            channel->status = 0;
            return;
        }
    }
    else if (channel->status & M4A_CHN_STOP)
    {
        envelope = (uint8_t)(((uint32_t)envelope * channel->release) >> 8);
        if (envelope <= channel->pseudoEchoVolume)
        {
            if (channel->pseudoEchoVolume == 0)
            {
                channel->status = 0;
                return;
            }
            envelope = channel->pseudoEchoVolume;
            channel->status |= M4A_CHN_IEC;
        }
    }
    else
    {
        const uint8_t phase = channel->status & M4A_CHN_ENV_MASK;
        if (phase == M4A_CHN_ENV_DECAY)
        {
            envelope = (uint8_t)(((uint32_t)envelope * channel->decay) >> 8);
            if (envelope <= channel->sustain)
            {
                envelope = channel->sustain;
                if (envelope == 0)
                {
                    if (channel->pseudoEchoVolume == 0)
                    {
                        channel->status = 0;
                        return;
                    }
                    envelope = channel->pseudoEchoVolume;
                    channel->status |= M4A_CHN_IEC;
                }
                else
                    channel->status--;
            }
        }
        else if (phase == M4A_CHN_ENV_ATTACK)
        {
            const uint32_t sum = (uint32_t)envelope + channel->attack;
            if (sum >= 0xFFu)
            {
                envelope = 0xFF;
                channel->status--;
            }
            else
                envelope = (uint8_t)sum;
        }
    }
    channel->envelopeVolume = envelope;
    const uint32_t master_gain = ((uint32_t)(master_volume + 1u) * envelope) >> 4;
    channel->envelopeVolumeRight = (uint8_t)(((uint32_t)channel->rightVolume * master_gain) >> 8);
    channel->envelopeVolumeLeft = (uint8_t)(((uint32_t)channel->leftVolume * master_gain) >> 8);
}

/* Render the pinned 1:1 ordinary forward path. */
static void sappy_render_fixed(M4ADriver* drv, M4ADriverPcmChan* channel, uint32_t frame_size)
{
    M4APcmSappyChannelState* state = &channel->sappy;
    int32_t position = state->source_position;
    int32_t count = state->count;
    const int32_t loop_start = channel->wav ? (int32_t)channel->wav->loopStart : 0;
    const int32_t loop_length = (channel->status & M4A_CHN_LOOP) && channel->wav->loopStart < channel->wav->size
                                    ? (int32_t)(channel->wav->size - channel->wav->loopStart)
                                    : 0;
    for (uint32_t i = 0; i < frame_size; i++)
    {
        if (count <= 0)
        {
            channel->status = 0;
            return;
        }
        const int32_t sample = sappy_source_sample(drv, channel, position);
        sappy_mix_sample(sample,
                         channel->envelopeVolumeRight,
                         channel->envelopeVolumeLeft,
                         &drv->pcmMixerState.sappy.output_right[i],
                         &drv->pcmMixerState.sappy.output_left[i]);
        position++;
        count--;
        if (count == 0)
        {
            if (loop_length == 0)
            {
                channel->status = 0;
                return;
            }
            position = loop_start;
            count = loop_length;
        }
    }
    state->source_position = position;
    state->count = count;
}

/* Render vanilla forward interpolation and its loop-entry label. */
static void sappy_render_forward(M4ADriver* drv, M4ADriverPcmChan* channel, uint32_t frame_size)
{
    M4APcmSappyChannelState* state = &channel->sappy;
    int32_t cursor = state->source_position;
    int32_t count = state->count;
    uint32_t phase = state->fractional_position;
    int32_t sample0 = sappy_source_sample(drv, channel, cursor);
    cursor++;
    int32_t sample1 = sappy_source_sample(drv, channel, cursor);
    const uint32_t step = (channel->type & VOICE_TYPE_FIX) != 0 ? 0x800000u : channel->frequency;
    const int32_t loop_start = (int32_t)channel->wav->loopStart;
    const int32_t loop_length = (channel->status & M4A_CHN_LOOP) && channel->wav->loopStart < channel->wav->size
                                    ? (int32_t)(channel->wav->size - channel->wav->loopStart)
                                    : 0;
    for (uint32_t i = 0; i < frame_size; i++)
    {
        const int32_t difference = sample1 - sample0;
        const int32_t interpolated = sample0 + sappy_arshift((int64_t)phase * difference, 23);
        sappy_mix_sample(interpolated,
                         channel->envelopeVolumeRight,
                         channel->envelopeVolumeLeft,
                         &drv->pcmMixerState.sappy.output_right[i],
                         &drv->pcmMixerState.sappy.output_left[i]);
        phase += step;
        const uint32_t advance = phase >> 23;
        phase &= 0x7FFFFFu;
        if (advance == 0)
            continue;
        int64_t remaining = (int64_t)count - advance;
        if (remaining <= 0)
        {
            if (loop_length == 0)
            {
                channel->status = 0;
                return;
            }
            int64_t overshoot = -remaining;
            do
            {
                remaining += loop_length;
                if (remaining > 0)
                    break;
                overshoot -= loop_length;
            } while (true);
            cursor = loop_start;
            const int32_t adjustment = (int32_t)overshoot;
            cursor += adjustment;
            sample0 = sappy_source_sample(drv, channel, cursor);
            cursor++;
            sample1 = sappy_source_sample(drv, channel, cursor);
            count = (int32_t)remaining;
            continue;
        }
        count = (int32_t)remaining;
        const int32_t adjustment = (int32_t)advance - 1;
        if (adjustment == 0)
            sample0 = sample1;
        else
        {
            cursor += adjustment;
            sample0 = sappy_source_sample(drv, channel, cursor);
        }
        cursor++;
        sample1 = sappy_source_sample(drv, channel, cursor);
    }
    state->source_position = cursor - 1;
    state->count = count;
    state->fractional_position = phase;
}

/* Render the source-defined reverse path and its uncompressed header padding. */
static void sappy_render_reverse(M4ADriver* drv, M4ADriverPcmChan* channel, uint32_t frame_size)
{
    M4APcmSappyChannelState* state = &channel->sappy;
    int32_t base = state->source_position - 1;
    int32_t count = state->count;
    uint32_t phase = state->fractional_position;
    int32_t sample0 = sappy_source_sample(drv, channel, base);
    int32_t sample1 = sappy_source_sample(drv, channel, base - 1);
    const uint32_t step = (channel->type & VOICE_TYPE_FIX) != 0 ? 0x800000u : channel->frequency;
    for (uint32_t i = 0; i < frame_size; i++)
    {
        const int32_t interpolated = sample0 + sappy_arshift((int64_t)phase * (sample1 - sample0), 23);
        sappy_mix_sample(interpolated,
                         channel->envelopeVolumeRight,
                         channel->envelopeVolumeLeft,
                         &drv->pcmMixerState.sappy.output_right[i],
                         &drv->pcmMixerState.sappy.output_left[i]);
        phase += step;
        const uint32_t advance = phase >> 23;
        phase &= 0x7FFFFFu;
        if (advance == 0)
            continue;
        const int64_t remaining = (int64_t)count - advance;
        if (remaining <= 0)
        {
            channel->status = 0;
            return;
        }
        count = (int32_t)remaining;
        base -= (int32_t)advance;
        sample0 = sappy_source_sample(drv, channel, base);
        sample1 = sappy_source_sample(drv, channel, base - 1);
    }
    state->source_position = base + 1;
    state->count = count;
    state->fractional_position = phase;
}

/* Select only vanilla source paths within one voice render. */
static void sappy_render_channel(M4ADriver* drv, M4ADriverPcmChan* channel, uint32_t frame_size)
{
    if (!(channel->status & M4A_CHN_ON) || (channel->status & M4A_CHN_START) || !channel->wav || !channel->wav->data)
    {
        return;
    }
    M4APcmSappyChannelState* state = &channel->sappy;
    if (state->count <= 0 || channel->wav->size == 0)
    {
        channel->status = 0;
        return;
    }
    if (state->special_initialized)
        channel->status |= M4A_CHN_SPECIAL;
    if (state->special_initialized && !state->compressed && (channel->type & VOICE_TYPE_REV) == 0)
    {
        return;
    }
    if (state->special_initialized && state->compressed)
        state->decoded_block = -1;
    if (!state->special_initialized && (channel->type & VOICE_TYPE_FIX) != 0)
        sappy_render_fixed(drv, channel, frame_size);
    else if ((channel->type & VOICE_TYPE_REV) != 0)
        sappy_render_reverse(drv, channel, frame_size);
    else
        sappy_render_forward(drv, channel, frame_size);
}

/* Reset the adapter's DMA output, decoder cache, and private voice state. */
void m4a_pcm_sappy_reset(M4ADriver* drv)
{
    if (!drv)
        return;
    memset(&drv->pcmMixerState.sappy, 0, sizeof(drv->pcmMixerState.sappy));
    for (int i = 0; i < M4A_MAX_PCM_CHANNELS; i++)
    {
        memset(&drv->pcmChans[i].sappy, 0, sizeof(drv->pcmChans[i].sappy));
        drv->pcmChans[i].sappy.decoded_block = -1;
    }
    drv->pcmMixerState.sappy.initialized = true;
}

/* Initialize common controls and private vanilla source state for a new note. */
void m4a_pcm_sappy_start(M4ADriver* drv, M4ADriverPcmChan* channel, WaveData* wav, uint8_t type, uint32_t start_offset)
{
    if (!drv || !channel)
        return;
    M4APcmSappyChannelState* state = &channel->sappy;
    memset(state, 0, sizeof(*state));
    state->decoded_block = -1;
    state->start_offset = start_offset;
    channel->wav = wav;
    channel->type = type;
    channel->envelopeVolume = 0;
    channel->envelopeVolumeRight = 0;
    channel->envelopeVolumeLeft = 0;
    if (!wav || !wav->data)
    {
        channel->status = 0;
        return;
    }
    channel->status = M4A_CHN_START;
}

/* Store an effective pitch update through the vanilla lifecycle seam. */
void m4a_pcm_sappy_update_pitch(M4ADriver* drv, M4ADriverPcmChan* channel, uint32_t frequency)
{
    (void)drv;
    if (channel)
        channel->frequency = frequency;
}

/* Inherit only vanilla playback position for the same logical wave. */
bool m4a_pcm_sappy_inherit(M4ADriver* drv, M4ADriverPcmChan* destination, const M4ADriverPcmChan* source)
{
    (void)drv;
    if (!destination || !source || destination->wav != source->wav)
        return false;
    destination->sappy = source->sappy;
    return true;
}

/* Seed, tick, and directly mix one complete vanilla SoundMain block. */
M4APcmBlockOutput m4a_pcm_sappy_render(M4ADriver* drv, const M4APcmBlockGeometry* geometry)
{
    M4APcmBlockOutput output = {0};
    if (!drv || !geometry)
        return output;

    output.geometry = *geometry;
    output.left = drv->pcmMixerState.sappy.output_left;
    output.right = drv->pcmMixerState.sappy.output_right;
    output.kind = M4A_PCM_BLOCK_RENDERED;

    sappy_seed_output(drv, geometry);
    const uint8_t max_channels =
        drv->max_pcm_channels > M4A_MAX_PCM_CHANNELS ? M4A_MAX_PCM_CHANNELS : drv->max_pcm_channels;
    for (uint8_t i = 0; i < max_channels; i++)
    {
        M4ADriverPcmChan* channel = &drv->pcmChans[i];
        sappy_channel_tick(channel, drv->master_volume);
        if (channel->gateTime > 0)
        {
            channel->gateTime--;
            if (channel->gateTime == 0)
                channel->status |= M4A_CHN_STOP;
        }
        sappy_render_channel(drv, channel, geometry->frame_size);
    }
    return output;
}
