#include "m4a_internal.h"
#include "m4a_pcm_internal.h"

#include <limits.h>
#include <stddef.h>
#include <string.h>

/*
 * F_decode_compressed indexes the first packed byte without masking it.  The
 * remaining bytes are the pinned IWRAM image following delta_lookup_table.
 */
static const uint8_t kBdpcmLookup[256] = {
    0x00, 0x01, 0x04, 0x09, 0x10, 0x19, 0x24, 0x31, 0xc0, 0xcf, 0xdc, 0xe7, 0xf0, 0xf7, 0xfc, 0xff, 0x00, 0x79, 0x00,
    0x03, 0x2c, 0x90, 0x4f, 0x52, 0x28, 0x90, 0x4f, 0x42, 0x00, 0x50, 0x99, 0xe8, 0x4c, 0x90, 0x4f, 0xe2, 0x00, 0x50,
    0x89, 0xe8, 0x30, 0xc0, 0x4f, 0xe2, 0x20, 0x00, 0x00, 0x4a, 0x00, 0xe0, 0x83, 0xe0, 0x40, 0xe0, 0x8e, 0xe2, 0x3f,
    0xe0, 0xce, 0xe3, 0x23, 0x93, 0xa0, 0xe1, 0x09, 0x83, 0x4e, 0xe0, 0x3c, 0x10, 0x1f, 0xe5, 0x08, 0x10, 0x81, 0xe0,
    0x0d, 0x00, 0x51, 0xe1, 0xfb, 0x00, 0x00, 0x2a, 0x00, 0x10, 0x83, 0xe0, 0x00, 0x00, 0x52, 0xe0, 0x03, 0x00, 0x2d,
    0xe9, 0x08, 0xd0, 0x4d, 0xe0, 0x06, 0x00, 0x00, 0xca, 0x02, 0x10, 0x83, 0xe0, 0x3f, 0x10, 0x81, 0xe2, 0x3f, 0x10,
    0xc1, 0xe3, 0x01, 0x10, 0x4e, 0xe0, 0x01, 0x80, 0x48, 0xe0, 0x08, 0x00, 0x8d, 0xe0, 0x00, 0x02, 0x00, 0xeb, 0x3f,
    0x30, 0x03, 0xe2, 0x0d, 0x00, 0xa0, 0xe1, 0x08, 0x20, 0x9a, 0xe5, 0x00, 0x10, 0xc2, 0x05, 0x24, 0x20, 0x92, 0xe5,
    0x10, 0x20, 0x82, 0xe2, 0x21, 0x10, 0xa0, 0xe3, 0x91, 0x29, 0x29, 0xe0, 0xc1, 0xff, 0xff, 0xeb, 0x40, 0x80, 0x58,
    0xe2, 0xfc, 0xff, 0xff, 0xca, 0x4b, 0x00, 0x00, 0xea, 0x3f, 0xe0, 0x83, 0xe2, 0x3f, 0xe0, 0xce, 0xe3, 0x00, 0x90,
    0x43, 0xe0, 0x01, 0x90, 0x49, 0xe2, 0x29, 0x93, 0xa0, 0xe1, 0x09, 0x83, 0x4e, 0xe0, 0xc4, 0xe0, 0x1f, 0xe5, 0x08,
    0xe0, 0x8e, 0xe0, 0x0d, 0x00, 0x5e, 0xe1, 0xd9, 0x00, 0x00, 0x2a, 0x00, 0xe0, 0x43, 0xe0, 0x00, 0x00, 0x52, 0xe0,
    0x01, 0x40, 0x2d, 0xe9, 0x0d, 0x00, 0xa0, 0xe1, 0x08, 0xd0, 0x4d, 0xe0, 0x04, 0x00, 0x00, 0xca, 0x02, 0x10, 0x43,
    0xe0, 0x09, 0x13, 0x41, 0xe0, 0x01, 0x80, 0x48, 0xe0,
};

/* Update the private pulse descriptor phase from the Camelot header. */
static void ipatix_update_pulse_duty(M4ADriverPcmChan* ch)
{
    M4APcmIpatixChannelState* state = &ch->ipatix;
    const uint8_t* config = (const uint8_t*)ch->wav->data;
    uint32_t lfo = (uint32_t)state->count + ((uint32_t)config[3] << 24);
    state->count = (int32_t)lfo;
    uint32_t folded = lfo + ((uint32_t)config[5] << 24);
    if ((int32_t)folded < 0)
        folded = ~folded;
    state->synth_pulse_duty = ((uint32_t)config[2] << 24) + (folded >> 8) * config[4];
}

/* Decode one 33-byte Pokémon BDPCM block into its 64 usable samples. */
static void
ipatix_decode_bdpcm_block(M4APcmIpatixChannelState* state, const int8_t* data, int32_t block, uint32_t wave_size)
{
    memset(state->decoded_samples, 0, sizeof(state->decoded_samples));
    if (!data || block < 0)
    {
        state->decoded_block = block;
        state->decoded_block_valid = true;
        return;
    }

    const uint64_t first_sample = (uint64_t)(uint32_t)block * M4A_IPATIX_BDPCM_BLOCK_SAMPLES;
    if (first_sample >= wave_size)
    {
        state->decoded_block = block;
        state->decoded_block_valid = true;
        return;
    }

    const uint32_t sample_count = (wave_size - (uint32_t)first_sample) < M4A_IPATIX_BDPCM_BLOCK_SAMPLES
                                      ? wave_size - (uint32_t)first_sample
                                      : M4A_IPATIX_BDPCM_BLOCK_SAMPLES;
    const uint32_t packed_samples = sample_count - 1u;
    const uint32_t encoded_bytes = 1u + (packed_samples + 1u) / 2u;
    const uint8_t* encoded = (const uint8_t*)data + (uint32_t)block * M4A_IPATIX_BDPCM_BLOCK_BYTES;
    int32_t sample = (int8_t)encoded[0];
    state->decoded_samples[0] = (int8_t)sample;
    uint32_t output = 1;
    if (output < sample_count)
    {
        sample += kBdpcmLookup[encoded[1]];
        state->decoded_samples[output++] = (int8_t)sample;
    }
    for (uint32_t byte = 2; byte < encoded_bytes && output < sample_count; byte++)
    {
        const uint8_t packed = encoded[byte];
        sample += kBdpcmLookup[packed >> 4];
        state->decoded_samples[output++] = (int8_t)sample;
        if (output < sample_count)
        {
            sample += kBdpcmLookup[packed & 0x0Fu];
            state->decoded_samples[output++] = (int8_t)sample;
        }
    }
    state->decoded_block = block;
    state->decoded_block_valid = true;
}

/* Read one logical sample, decoding the containing BDPCM block on demand. */
static int32_t ipatix_source_sample(const M4ADriverPcmChan* ch, int32_t position)
{
    const M4APcmIpatixChannelState* state = &ch->ipatix;
    if (!ch->wav || !ch->wav->data || position < 0 || (uint32_t)position >= ch->wav->size)
        return 0;
    if (state->compressed)
    {
        M4APcmIpatixChannelState* mutable_state = (M4APcmIpatixChannelState*)state;
        const int32_t block = position / (int32_t)M4A_IPATIX_BDPCM_BLOCK_SAMPLES;
        const uint32_t offset = (uint32_t)position & (M4A_IPATIX_BDPCM_BLOCK_SAMPLES - 1u);
        if (!mutable_state->decoded_block_valid || mutable_state->decoded_block != block)
            ipatix_decode_bdpcm_block(mutable_state, ch->wav->data, block, ch->wav->size);
        return mutable_state->decoded_samples[offset];
    }
    return ch->wav->data[position];
}
/* Prepare the source cursor used by one iPatix block. */
typedef struct
{
    M4ADriverPcmChan* channel;
    M4APcmIpatixChannelState* state;
    int32_t position;
    int32_t count;
    int32_t count_base;
    int32_t loop_start;
    int32_t loop_length;
    int32_t step;
    bool looped;
    bool reverse;
    bool pad_end;
    bool ended_with_padding;
    const int8_t* padding_source;
    uint32_t padding_copy_count;
    uint32_t padding_total_count;
    bool padding_override;
    bool terminal_padding;
} IpatixSource;

/* Construct a forward or reverse source view from private channel state. */
static IpatixSource ipatix_source_from_channel(M4ADriverPcmChan* channel)
{
    IpatixSource source;
    source.channel = channel;
    source.state = &channel->ipatix;
    source.reverse = (channel->type & VOICE_TYPE_REV) != 0;
    source.position = source.state->source_position;
    if (source.state->compressed)
    {
        /*
         * Compressed reverse playback keeps the source cursor one past the
         * current logical sample, as C_channel_init_comp_reverse does.
         */
        if (source.reverse && source.position > 0)
            source.position--;
    }
    else if (channel->wav && channel->wav->data && source.state->current_pointer)
    {
        const ptrdiff_t pointer_offset = source.state->current_pointer - channel->wav->data;
        if (pointer_offset >= 0 && (uint64_t)pointer_offset <= channel->wav->size)
        {
            if (source.reverse)
                source.position = pointer_offset > 0 ? (int32_t)pointer_offset - 1 : 0;
            else
                source.position = (int32_t)pointer_offset;
        }
    }
    source.count = source.state->count;
    source.count_base = 0;
    source.loop_start = source.state->loop_start_position;
    source.loop_length = source.state->loop_length;
    source.step = source.reverse ? -1 : 1;
    source.looped = source.state->is_loop && source.loop_length > 0;
    source.pad_end = source.reverse;
    source.ended_with_padding = false;
    source.padding_source = NULL;
    source.padding_copy_count = 0;
    source.padding_total_count = 0;
    source.padding_override = false;
    source.terminal_padding = false;
    if (source.reverse && source.looped)
    {
        source.count_base = source.loop_start;
        source.count -= source.loop_start;
    }
    return source;
}

/* Read one playback-order sample, including reverse-loader zero padding. */
static int32_t ipatix_source_sample_at(const IpatixSource* source, int32_t position)
{
    if (!source->padding_override)
        return ipatix_source_sample(source->channel, position);
    if (position < 0 || (uint32_t)position >= source->padding_total_count ||
        (uint32_t)position >= source->padding_copy_count)
    {
        return 0;
    }
    return source->padding_source[source->padding_copy_count - 1u - (uint32_t)position];
}

static int32_t ipatix_source_current_sample(const IpatixSource* source)
{
    if (source->ended_with_padding)
        return 0;
    return ipatix_source_sample_at(source, source->position);
}

/* Advance a source cursor and reproduce iPatix endpoint/loop interpolation state. */
static bool
ipatix_source_advance(IpatixSource* source, uint32_t advance, bool fixed, int32_t* sample_stored, bool* stopped)
{
    if (advance == 0)
        return true;

    const int32_t old_position = source->position;
    source->count -= (int32_t)advance;
    if (source->count <= 0)
    {
        if (source->looped)
        {
            while (source->count <= 0)
                source->count += source->loop_length;
            if (source->step > 0)
                source->position = source->loop_start + source->loop_length - source->count;
            else
                source->position = source->loop_start + source->loop_length - 1 - (source->loop_length - source->count);
            if (!fixed)
            {
                if (source->count == source->loop_length)
                {
                    if (source->step > 0)
                    {
                        const int32_t predecessor =
                            advance > 1 && source->loop_start > 0 ? source->loop_start - 1 : old_position;
                        *sample_stored = ipatix_source_sample_at(source, predecessor);
                    }
                    else
                    {
                        *sample_stored = ipatix_source_sample_at(source, source->loop_start);
                    }
                }
                else
                {
                    *sample_stored = ipatix_source_sample_at(source, source->position - source->step);
                }
            }
        }
        else
        {
            *stopped = true;
            if (source->pad_end && !fixed)
            {
                *sample_stored = ipatix_source_sample_at(source, old_position);
                source->position = 0;
                source->count = 0;
                source->ended_with_padding = true;
                return true;
            }
            source->count = 0;
            return false;
        }
    }
    else
    {
        source->position += (int32_t)advance * source->step;
        if (!fixed)
            *sample_stored = ipatix_source_sample_at(source, source->position - source->step);
    }
    return true;
}

/* Add one signed sample to the packed 00LL00RR accumulator with ARM wrap. */
static void ipatix_mix_sample(int32_t sample, uint32_t packed_volume, uint32_t* mix)
{
    *mix += packed_volume * (uint32_t)sample;
}
/* Return the ARM low-word interpolation product and preserve its bit pattern. */
static uint32_t ipatix_interpolation_product(int32_t difference, uint32_t fine_position)
{
    return (uint32_t)difference * fine_position;
}

/* Reproduce ARM's arithmetic right shift without relying on signed-shift rules. */
static int32_t ipatix_arshift32(uint32_t value, unsigned shift)
{
    if (shift == 0)
        return value <= INT32_MAX ? (int32_t)value : (int32_t)((int64_t)value - 0x100000000LL);
    const int64_t signed_value = (value & 0x80000000u) != 0 ? (int64_t)value - 0x100000000LL : (int64_t)value;
    if (signed_value >= 0)
        return (int32_t)(value >> shift);
    const int64_t magnitude = -signed_value;
    return (int32_t)-((magnitude + ((INT64_C(1) << shift) - 1)) >> shift);
}

/* Convert one low byte of an ARM word to a signed FIFO sample. */
static int8_t ipatix_signed_byte(uint8_t value)
{
    return value <= INT8_MAX ? (int8_t)value : (int8_t)((int)value - 0x100);
}

/* Convert one wrapped ARM word to a signed C value without implementation-defined casts. */
static int32_t ipatix_signed_word(uint32_t value);

/* Render one zero-length Camelot pulse, pseudo-saw, or triangle descriptor. */
static void ipatix_render_synth(M4ADriverPcmChan* channel, uint32_t* mix, uint32_t frame_size)
{
    M4APcmIpatixChannelState* state = &channel->ipatix;
    uint32_t phase = state->fine_position;
    const uint32_t step = channel->frequency << 3;
    const uint32_t base_packed_volume = ((uint32_t)channel->envelopeVolumeLeft << 16) | channel->envelopeVolumeRight;
    /*
     * The normal source path halves 8-bit gains before mixing.  Synth setup
     * skips that path; only pseudo-saw applies its own unrounded LSR/BIC.
     */
    const uint32_t packed_volume = state->synth_type == 2 ? (base_packed_volume >> 1) & ~0xFF00u : base_packed_volume;

    for (uint32_t i = 0; i < frame_size; i++)
    {
        int32_t value;
        if (state->synth_type == 1)
        {
            value = phase < state->synth_pulse_duty ? 64 : -64;
            phase += step;
        }
        else if (state->synth_type == 2)
        {
            phase += step;
            const uint32_t folded_phase = (phase << 1) >> 27;
            const int32_t raw = (int32_t)(phase >> 24) - 0x70 - (int32_t)folded_phase;
            const int32_t filtered = ipatix_arshift32((uint32_t)state->count, 1);
            state->count = ipatix_signed_word((uint32_t)raw + (uint32_t)filtered);
            value = state->count;
        }
        else
        {
            phase += step;
            if ((phase & 0x80000000u) != 0)
                value = (int32_t)(0x180u - (phase >> 23));
            else
                value = ipatix_arshift32(phase, 23) - 0x80;
        }
        ipatix_mix_sample(value, packed_volume, &mix[i]);
    }
    state->fine_position = phase;
}
/* Render one ordinary or BDPCM channel into the persistent HQ work buffer. */
static void ipatix_render_channel(M4ADriverPcmChan* channel, uint32_t* mix, uint32_t frame_size)
{
    if (!(channel->status & M4A_CHN_ON) || (channel->status & M4A_CHN_START))
        return;
    M4APcmIpatixChannelState* state = &channel->ipatix;
    if (channel->envelopeVolumeRight == 0 && channel->envelopeVolumeLeft == 0)
        return;
    if (state->synth_type != 0)
    {
        if (channel->wav && channel->wav->data)
            ipatix_render_synth(channel, mix, frame_size);
        return;
    }
    if (!channel->wav || !channel->wav->data)
        return;
    if (state->count <= 0)
    {
        channel->status = 0;
        return;
    }

    IpatixSource source = ipatix_source_from_channel(channel);
    int32_t sample_stored = state->sample_stored;
    uint32_t fine_position = state->fine_position;
    const bool fixed = (channel->type & VOICE_TYPE_FIX) != 0;
    const uint32_t frequency = channel->frequency;
    uint32_t packed_volume = ((uint32_t)channel->envelopeVolumeLeft << 16) | channel->envelopeVolumeRight;
    const uint64_t required_advance = ((uint64_t)fine_position + (uint64_t)frequency * frame_size) >> 23;
    if (!fixed && source.reverse && !source.looped && required_advance >= (uint32_t)source.count)
    {
        /*
         * C_data_load_uncomp_rev aligns and reverses whole words before
         * clearing the unavailable tail.  Its final short block can therefore
         * expose the aligned bytes immediately preceding the wave.
         */
        const uintptr_t current = (uintptr_t)state->current_pointer;
        const uintptr_t source_start = current - (uint32_t)source.count;
        const uintptr_t aligned_end = (current + 3u) & ~(uintptr_t)3u;
        const uintptr_t block_start = (current - (uintptr_t)required_advance - 1u) & ~(uintptr_t)3u;
        source.padding_source = (const int8_t*)block_start;
        source.padding_copy_count = (uint32_t)(aligned_end - source_start);
        source.padding_total_count = (uint32_t)(aligned_end - block_start);
        source.padding_override = true;
        source.terminal_padding = true;
        source.position = (int32_t)((0u - current) & 3u);
        source.count = (int32_t)source.padding_total_count - source.position;
        source.step = 1;
        source.pad_end = false;
    }
    if (!fixed)
    {
        const uint32_t carry = packed_volume & 1u;
        packed_volume = ((packed_volume >> 1) + 0x8000u + carry) & ~0x8000u;
    }
    const bool fast_mixing = !fixed && (!source.reverse || source.padding_override) && !source.state->compressed &&
                             source.count > 0 && (uint64_t)source.count > required_advance;
    const bool high_speed_mixing = fast_mixing && frequency >= 0x800000u;
    const bool very_high_speed_mixing = high_speed_mixing && frequency >= 0x1000000u;
    if (high_speed_mixing)
        packed_volume <<= 1;

    uint32_t interpolation_register = 0;
    bool force_fast_multiply = false;
    bool stopped = false;
    for (uint32_t i = 0; i < frame_size; i++)
    {
        const bool tail_sample = source.ended_with_padding;
        const int32_t source_sample = ipatix_source_current_sample(&source);
        const int32_t difference = source_sample - sample_stored;
        int32_t sample;
        if (fixed)
            sample = source_sample;
        else
        {
            if (!fast_mixing || (i & 7u) == 0 || force_fast_multiply || difference != 0)
                interpolation_register = ipatix_interpolation_product(difference, fine_position);
            const int32_t fraction = ipatix_arshift32(interpolation_register, high_speed_mixing ? 23 : 22);
            const uint32_t base = high_speed_mixing ? (uint32_t)sample_stored : (uint32_t)sample_stored << 1;
            sample = ipatix_signed_word(base + (uint32_t)fraction);
            interpolation_register = (uint32_t)sample;
        }

        ipatix_mix_sample(sample, packed_volume, &mix[i]);
        if (tail_sample)
        {
            /*
             * Reverse resampling has one source-defined zero look-ahead
             * sample.  Do not turn it into an artificial infinite source.
             */
            sample_stored = 0;
            stopped = true;
            break;
        }

        uint32_t advance;
        if (fixed)
            advance = 1;
        else
        {
            fine_position += frequency;
            advance = fine_position >> 23;
            fine_position &= 0x7FFFFFu;
        }
        force_fast_multiply = fast_mixing && !high_speed_mixing && advance == 0;
        if (advance != 0)
        {
            if (!ipatix_source_advance(&source, advance, fixed, &sample_stored, &stopped))
                break;
            if (very_high_speed_mixing)
                sample_stored = ipatix_source_current_sample(&source);
        }
    }

    if (source.terminal_padding)
    {
        source.ended_with_padding = true;
        stopped = true;
    }
    if (source.ended_with_padding)
    {
        state->source_position = 0;
        state->current_pointer = channel->wav ? channel->wav->data : NULL;
        state->count = 0;
    }
    else
    {
        state->source_position = source.position + (source.reverse && state->compressed ? 1 : 0);
        if (channel->wav && channel->wav->data)
        {
            if (source.reverse && !state->compressed)
                state->current_pointer = channel->wav->data + source.position + 1;
            else if (!state->compressed)
                state->current_pointer = channel->wav->data + source.position;
            else
                /* Compressed position is logical; no encoded-byte pointer exists. */
                state->current_pointer = channel->wav->data;
        }
        else
            state->current_pointer = NULL;
        state->count = source.count_base + source.count;
    }
    state->sample_stored = (int8_t)sample_stored;
    state->fine_position = fine_position;
    if (stopped)
        channel->status = 0;
}

/* Convert one wrapped ARM word to a signed C value without implementation-defined casts. */
static int32_t ipatix_signed_word(uint32_t value)
{
    if (value <= INT32_MAX)
        return (int32_t)value;
    return (int32_t)((int64_t)value - 0x100000000LL);
}

/* Apply the pinned ARM ADDS/EORVS overflow test while retaining the undoubled sum. */
static int32_t ipatix_clamp_lane(uint32_t sum)
{
    const uint32_t doubled = sum + sum;
    const bool overflow = ((sum ^ doubled) & 0x80000000u) != 0;
    if (overflow)
        return (doubled & 0x80000000u) != 0 ? INT32_MAX / 2 : INT32_MIN / 2;
    return ipatix_signed_word(sum);
}
/* Recreate both source halfword tap sums before multiplying/storing seeds. */
static void ipatix_reverb_seeds(const M4ADriver* drv,
                                uint32_t position,
                                uint32_t frame_size,
                                uint32_t buf_size,
                                uint32_t* seed_low,
                                uint32_t* seed_high)
{
    const uint32_t future = (position + frame_size) % buf_size;
    const uint32_t next_position = (position + 1u) % buf_size;
    const uint32_t next_future = (future + 1u) % buf_size;
    const int32_t low_sum = (int32_t)drv->pcm.ring_a[future] + (int32_t)drv->pcm.ring_b[future] +
                            (int32_t)drv->pcm.ring_b[position] + (int32_t)drv->pcm.ring_a[position];
    const int32_t high_sum = (int32_t)drv->pcm.ring_a[next_future] + (int32_t)drv->pcm.ring_b[next_future] +
                             (int32_t)drv->pcm.ring_b[next_position] + (int32_t)drv->pcm.ring_a[next_position];
    const uint32_t packed_reverb = (uint32_t)(drv->reverb_amount >> 2);
    const uint32_t packed_gain = packed_reverb | (packed_reverb << 16);
    /* The source computes R1 (high) then R0 (low), and stores R0,R1. */
    const uint32_t wet_high = packed_gain * (uint32_t)high_sum;
    const uint32_t wet_low = packed_gain * (uint32_t)low_sum;
    *seed_low = wet_low;
    *seed_high = wet_high;
}

/* Downconvert packed HQ words once and retain independent seven-bit feedback. */
static void ipatix_downsample(M4ADriver* drv, const M4APcmBlockGeometry* geometry)
{
    M4APcmIpatixGlobalState* global = &drv->pcmMixerState.ipatix;
    uint8_t remainder_left = global->discarded_left;
    uint8_t remainder_right = global->discarded_right;
    for (uint32_t i = 0; i < geometry->frame_size; i += 2)
    {
        const uint32_t packed = global->packed_mix[i];
        const uint32_t left_sum = packed + ((uint32_t)remainder_left << 16);
        const uint32_t right_sum = (packed << 16) + ((uint32_t)remainder_right << 16);
        const int32_t left_lane = ipatix_clamp_lane(left_sum);
        const int32_t right_lane = ipatix_clamp_lane(right_sum);
        global->output_left[i] = ipatix_signed_byte((uint8_t)((uint32_t)left_lane >> 23));
        global->output_right[i] = ipatix_signed_byte((uint8_t)((uint32_t)right_lane >> 23));
        remainder_left = (uint8_t)(((uint32_t)left_lane >> 16) & 0x7Fu);
        remainder_right = (uint8_t)(((uint32_t)right_lane >> 16) & 0x7Fu);

        const uint32_t position = (geometry->ring_base + i) % geometry->dma_buf_size;
        uint32_t seed_low = 0;
        uint32_t seed_high = 0;
        if (drv->reverb_amount != 0)
            ipatix_reverb_seeds(drv, position, geometry->frame_size, geometry->dma_buf_size, &seed_low, &seed_high);
        global->packed_mix[i] = drv->reverb_amount != 0 ? seed_low : 0;

        if (i + 1u < geometry->frame_size)
        {
            const uint32_t packed_next = global->packed_mix[i + 1u];
            const uint32_t left_sum_next = packed_next + ((uint32_t)remainder_left << 16);
            const uint32_t right_sum_next = (packed_next << 16) + ((uint32_t)remainder_right << 16);
            const int32_t left_lane_next = ipatix_clamp_lane(left_sum_next);
            const int32_t right_lane_next = ipatix_clamp_lane(right_sum_next);
            global->output_left[i + 1u] = ipatix_signed_byte((uint8_t)((uint32_t)left_lane_next >> 23));
            global->output_right[i + 1u] = ipatix_signed_byte((uint8_t)((uint32_t)right_lane_next >> 23));
            remainder_left = (uint8_t)(((uint32_t)left_lane_next >> 16) & 0x7Fu);
            remainder_right = (uint8_t)(((uint32_t)right_lane_next >> 16) & 0x7Fu);
            global->packed_mix[i + 1u] = drv->reverb_amount != 0 ? seed_high : 0;
        }
    }
    global->discarded_left = remainder_left;
    global->discarded_right = remainder_right;
}

/* Tick iPatix ADSR/pseudo-echo state and derive master-scaled channel gains. */
static void ipatix_channel_tick(M4ADriverPcmChan* channel, uint8_t master_volume)
{
    if (!(channel->status & M4A_CHN_ON))
        return;

    uint8_t envelope = channel->envelopeVolume;
    M4APcmIpatixChannelState* state = &channel->ipatix;
    if (channel->status & M4A_CHN_START)
    {
        /*
         * C_channel_state_loop tests INIT|RELEASE before initialization.
         * START|STOP therefore turns the slot off immediately; STOP is only
         * a release request after a started voice has entered normal state.
         */
        if (channel->status & M4A_CHN_STOP)
        {
            channel->status = 0;
            return;
        }
        channel->status = M4A_CHN_ENV_ATTACK | (state->is_loop ? M4A_CHN_LOOP : 0);
        envelope = 0;
        state->sample_stored = 0;
        state->fine_position = state->synth_type == 3 ? 0x40000000u : 0;
    }

    if (channel->status & M4A_CHN_IEC)
    {
        const uint8_t original_length = channel->pseudoEchoLength;
        channel->pseudoEchoLength--;
        /*
         * ARM SUBS/BHI continues only for an unsigned result that is both
         * nonzero and without borrow.  Preserve the wrapped zero case while
         * stopping for original lengths zero and one.
         */
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
                    channel->status = (channel->status & ~M4A_CHN_ENV_MASK) | M4A_CHN_IEC;
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
    const uint32_t master_product = (uint32_t)(master_volume + 1u) * envelope;
    channel->envelopeVolumeRight = (uint8_t)(((uint32_t)channel->rightVolume * master_product) >> 13);
    channel->envelopeVolumeLeft = (uint8_t)(((uint32_t)channel->leftVolume * master_product) >> 13);
    if (state->synth_type == 1 && channel->wav && channel->wav->data)
        ipatix_update_pulse_duty(channel);
}

/* Reset the adapter's persistent HQ, output, feedback, and reverb storage. */
void m4a_pcm_ipatix_reset(M4ADriver* drv)
{
    if (!drv)
        return;
    memset(&drv->pcmMixerState.ipatix, 0, sizeof(drv->pcmMixerState.ipatix));
    for (int i = 0; i < M4A_MAX_PCM_CHANNELS; i++)
        drv->pcmChans[i].ipatix.decoded_block = -1;
    drv->pcmMixerState.ipatix.initialized = true;
}

/* Initialize common controls and private iPatix source state for a new note. */
void m4a_pcm_ipatix_start(M4ADriver* drv, M4ADriverPcmChan* ch, WaveData* wav, uint8_t type, uint32_t start_offset)
{
    if (!drv || !ch)
        return;
    M4APcmIpatixChannelState* state = &ch->ipatix;
    memset(state, 0, sizeof(*state));
    state->decoded_block = -1;
    ch->wav = wav;
    ch->type = type;
    ch->envelopeVolume = 0;
    ch->envelopeVolumeRight = 0;
    ch->envelopeVolumeLeft = 0;
    if (!wav || !wav->data)
    {
        ch->status = 0;
        return;
    }

    const bool looped = (wav->status & 0xC000u) != 0 && wav->loopStart < wav->size;
    state->is_loop = looped;
    state->compressed = (wav->type & 1u) != 0;
    state->loop_length = looped ? (int32_t)(wav->size - wav->loopStart) : 0;
    state->loop_start_position = looped ? (int32_t)wav->loopStart : 0;
    if (start_offset > wav->size)
        start_offset = wav->size;

    if (wav->size == 0)
    {
        const uint8_t wave_type = (uint8_t)wav->data[1];
        state->current_pointer = wav->data;
        state->synth_type = wave_type == 0 ? 1 : (wave_type == 1 ? 2 : 3);
        state->fine_position = state->synth_type == 3 ? 0x40000000u : 0;
        state->source_position = 0;
        state->count = 0;
    }
    else
    {
        const bool reverse = (type & VOICE_TYPE_REV) != 0;
        const uint32_t remaining = wav->size - start_offset;
        state->count = (int32_t)remaining;
        if (reverse)
        {
            if (state->compressed)
            {
                /*
                 * Compressed reverse uses the logical one-past cursor from
                 * C_channel_init_comp_reverse.  It is not the uncompressed
                 * byte-pointer convention.
                 */
                state->source_position = (int32_t)remaining;
                state->current_pointer = wav->data;
            }
            else
            {
                state->source_position = remaining > 0 ? (int32_t)remaining - 1 : 0;
                state->current_pointer = wav->data + remaining;
            }
        }
        else
        {
            state->source_position = (int32_t)start_offset;
            state->current_pointer = state->compressed ? wav->data : wav->data + start_offset;
        }
    }
    ch->status = M4A_CHN_START | (looped ? M4A_CHN_LOOP : 0);
}

/* Store a common pitch update through the iPatix lifecycle seam. */
void m4a_pcm_ipatix_update_pitch(M4ADriver* drv, M4ADriverPcmChan* ch, uint32_t frequency)
{
    (void)drv;
    if (ch)
        ch->frequency = frequency;
}

/* Inherit private playback state while retaining destination logical controls. */
bool m4a_pcm_ipatix_inherit(M4ADriver* drv, M4ADriverPcmChan* destination, const M4ADriverPcmChan* source)
{
    (void)drv;
    if (!destination || !source || destination->wav != source->wav)
        return false;
    destination->ipatix = source->ipatix;
    return true;
}

/* Mix, downconvert, and retain iPatix feedback for one SoundMain block. */
M4APcmBlockOutput m4a_pcm_ipatix_render(M4ADriver* drv, const M4APcmBlockGeometry* geometry)
{
    M4APcmBlockOutput output = {0};
    if (!drv || !geometry)
        return output;

    output.geometry = *geometry;
    output.left = drv->pcmMixerState.ipatix.output_left;
    output.right = drv->pcmMixerState.ipatix.output_right;
    output.kind = M4A_PCM_BLOCK_RENDERED;

    M4APcmIpatixGlobalState* global = &drv->pcmMixerState.ipatix;
    for (int i = 0; i < M4A_MAX_PCM_CHANNELS; i++)
    {
        M4ADriverPcmChan* channel = &drv->pcmChans[i];
        ipatix_channel_tick(channel, drv->master_volume);
        if (channel->gateTime > 0)
        {
            channel->gateTime--;
            if (channel->gateTime == 0)
                channel->status |= M4A_CHN_STOP;
        }
    }
    const uint8_t max_channels =
        drv->max_pcm_channels > M4A_MAX_PCM_CHANNELS ? M4A_MAX_PCM_CHANNELS : drv->max_pcm_channels;
    for (uint8_t i = 0; i < max_channels; i++)
        ipatix_render_channel(&drv->pcmChans[i], global->packed_mix, geometry->frame_size);

    if (geometry->frame_size != 0)
        ipatix_downsample(drv, geometry);
    return output;
}
