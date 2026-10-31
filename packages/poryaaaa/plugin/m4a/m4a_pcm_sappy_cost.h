#ifndef M4A_PCM_SAPPY_COST_H
#define M4A_PCM_SAPPY_COST_H

#include "m4a_pcm_cost.h"

/* Account for the envelope decisions made by sappy_channel_tick. */
static int sappy_envelope_cycles(const M4ADriverPcmChan* ch)
{
    const uint8_t status = ch->status;
    unsigned envelope = ch->envelopeVolume;
    int cycles;
    if (status & M4A_CHN_START)
    {
        if ((status & M4A_CHN_STOP) || !ch->wav || !ch->wav->data)
            return M4A_MIX_ENV_START_STOP + M4A_MIX_ENV_END;
        cycles = M4A_MIX_ENV_START +
                 (((ch->wav->status & 0xC000u) && ch->wav->loopStart < ch->wav->size) ? M4A_MIX_ENV_START_LOOP : 0);
        envelope = 0;
        goto attack;
    }
    cycles = M4A_MIX_ENV_HEAD;
    if (status & M4A_CHN_IEC)
        return cycles + (ch->pseudoEchoLength <= 1 ? M4A_MIX_ENV_IEC_END + M4A_MIX_ENV_END : M4A_MIX_ENV_IEC);
    cycles += M4A_MIX_ENV_NOT_IEC;
    if (status & M4A_CHN_STOP)
    {
        envelope = (envelope * ch->release) >> 8;
        if (envelope > ch->pseudoEchoVolume)
            return cycles + M4A_MIX_ENV_RELEASE;
        cycles += M4A_MIX_ENV_RELEASE_LOW;
        goto echo;
    }
    cycles += M4A_MIX_ENV_NOT_STOP;
    if ((status & M4A_CHN_ENV_MASK) == M4A_CHN_ENV_DECAY)
    {
        envelope = (envelope * ch->decay) >> 8;
        if (envelope > ch->sustain)
            return cycles + M4A_MIX_ENV_DECAY;
        if (ch->sustain)
            return cycles + M4A_MIX_ENV_DECAY_SUSTAIN;
        cycles += M4A_MIX_ENV_DECAY_ZERO;
        goto echo;
    }
    if ((status & M4A_CHN_ENV_MASK) != M4A_CHN_ENV_ATTACK)
        return cycles + M4A_MIX_ENV_SUSTAIN;
    cycles += M4A_MIX_ENV_TO_ATTACK;
attack:
    return cycles + (envelope + ch->attack >= 0xFFu ? M4A_MIX_ENV_ATTACK_PEAK : M4A_MIX_ENV_ATTACK);
echo:
    return cycles + (ch->pseudoEchoVolume ? M4A_MIX_ENV_ECHO_START : M4A_MIX_ENV_ECHO_NONE + M4A_MIX_ENV_END);
}

/* Count the vanilla decoder's block lookup, including out-of-range accesses. */
static int sappy_decoder_cycles(int32_t position, uint32_t* cached_block)
{
    const uint32_t block = (uint32_t)position >> 6;
    if (*cached_block == block)
        return M4A_MIX_UNK2_CALL + M4A_MIX_UNK2_HIT;
    *cached_block = block;
    return M4A_MIX_UNK2_CALL + M4A_MIX_UNK2_DECODE + m4a_pcm_cost_mul(block) +
           (position < 0 ? M4A_MIX_UNK2_UNMAPPED : 0);
}

typedef struct
{
    int64_t cycles;
    uint32_t cached_block;
    uint32_t fixed_bulk_left;
    uint32_t fixed_tail_left;
    uint32_t fixed_tail_size;
    bool fixed_tail_pending;
    bool special;
    bool compressed;
    bool reverse;
} SappyCost;

/* Start the cost clock from the channel state before its real mix. */
static SappyCost sappy_cost_begin(const M4ADriverPcmChan* ch, uint32_t frame_size)
{
    SappyCost cost = {0};
    const M4APcmSappyChannelState* state = &ch->sappy;
    if (!frame_size || !(ch->status & M4A_CHN_ON) || (ch->status & M4A_CHN_START) || !ch->wav || !ch->wav->data)
        return cost;
    cost.cycles = M4A_MIX_VOL + ((ch->status & M4A_CHN_LOOP) ? M4A_MIX_VOL_LOOP : 0);
    if (state->count <= 0 || ch->wav->size == 0)
        return cost;
    cost.special = state->special_initialized;
    cost.compressed = cost.special && state->compressed;
    cost.reverse = (ch->type & VOICE_TYPE_REV) != 0;
    const bool fixed = (ch->type & VOICE_TYPE_FIX) != 0;
    if (!cost.special && fixed)
    {
        cost.cycles += M4A_MIX_TO_FIX;
        return cost;
    }
    if (!cost.special)
    {
        cost.cycles += M4A_MIX_TO_INTERP + M4A_MIX_INTERP_SETUP + m4a_pcm_cost_mul(ch->cpu_raw_frequency);
        return cost;
    }
    cost.cycles += M4A_MIX_TO_UNK1 + M4A_MIX_RETURN_UNK1;
    cost.cycles += (ch->status & M4A_CHN_SPECIAL)
                       ? M4A_MIX_UNK1_HEAD
                       : (cost.reverse ? M4A_MIX_UNK1_FIRST_REV : M4A_MIX_UNK1_FIRST) +
                             (cost.compressed ? M4A_MIX_UNK1_FIRST_CMP : M4A_MIX_UNK1_FIRST_RAW);
    cost.cycles += M4A_MIX_UNK1_SETUP + (fixed ? 0 : m4a_pcm_cost_mul(ch->cpu_raw_frequency));
    if (cost.compressed)
    {
        cost.cached_block = 0xFF000000u;
        cost.cycles += M4A_MIX_UNK1_CMP + (cost.reverse ? M4A_MIX_UNK1_CMP_REV : M4A_MIX_UNK1_CMP_FWD);
    }
    else
        cost.cycles += M4A_MIX_UNK1_RAW + (cost.reverse ? M4A_MIX_UNK1_RAW_REV : M4A_MIX_UNK1_RAW_FWD);
    return cost;
}

/* Count one decoder call without reading or changing the shared cache. */
static void sappy_cost_decode(SappyCost* cost, int32_t position)
{
    if (cost && cost->compressed)
        cost->cycles += sappy_decoder_cycles(position, &cost->cached_block);
}

/* Charge the beginning and completed portion of an interpolated sample. */
static void sappy_cost_sample_begin(SappyCost* cost, uint32_t index)
{
    if (cost)
        cost->cycles += ((index & 3u) == 0 ? M4A_MIX_WORD_LOAD : 0) + M4A_MIX_INTERP_SAMPLE;
}

static void sappy_cost_sample_done(SappyCost* cost, uint32_t index, uint32_t frame_size)
{
    if (!cost)
        return;
    if ((index & 3u) == 3u)
        cost->cycles += (cost->special ? M4A_MIX_WORD_STORE_SP : M4A_MIX_WORD_STORE) + M4A_MIX_NEXT_WORD;
    else if (index + 1u < frame_size)
        cost->cycles += M4A_MIX_NEXT_SAMPLE;
}

/* Count FIX's word groups from current count; tails use the real loop below. */
static bool sappy_cost_fixed_begin(SappyCost* cost, int32_t count, uint32_t left)
{
    if (!cost)
        return false;
    if (!cost->fixed_bulk_left && !cost->fixed_tail_left)
    {
        if (!cost->fixed_tail_pending && count > 4)
        {
            const bool inside = (uint32_t)count > left;
            const uint32_t samples = inside ? left : ((uint32_t)count - 1u) & ~3u;
            const uint32_t words = samples / 4u;
            const uint32_t remainder = samples & 3u;
            cost->cycles += inside ? M4A_MIX_FIX_BLOCK : M4A_MIX_FIX_BLOCK_END;
            cost->cycles += (int64_t)words * (M4A_MIX_WORD_LOAD + 4 * M4A_MIX_FIX_SAMPLE +
                                              3 * M4A_MIX_NEXT_SAMPLE + M4A_MIX_WORD_STORE + M4A_MIX_NEXT_WORD);
            if (words)
                cost->cycles += M4A_MIX_LOOP_LAST;
            if (remainder)
                cost->cycles += M4A_MIX_WORD_LOAD + (int64_t)remainder * M4A_MIX_FIX_SAMPLE +
                                (int64_t)(remainder - 1u) * M4A_MIX_NEXT_SAMPLE;
            cost->fixed_bulk_left = samples;
            if (samples == left)
                cost->cycles += M4A_MIX_FIX_BLOCK_DONE + M4A_MIX_STORE;
            else
            {
                cost->cycles += M4A_MIX_FIX_BLOCK_TAIL;
                cost->fixed_tail_pending = true;
            }
        }
        else
        {
            if (!cost->fixed_tail_pending)
                cost->cycles += M4A_MIX_FIX_SHORT;
            cost->cycles += M4A_MIX_WORD_LOAD;
            cost->fixed_tail_size = left < 4u ? left : 4u;
            cost->fixed_tail_left = cost->fixed_tail_size;
            cost->fixed_tail_pending = false;
        }
    }
    if (cost->fixed_bulk_left)
    {
        --cost->fixed_bulk_left;
        return false;
    }
    cost->cycles += M4A_MIX_FIX_SAMPLE;
    return true;
}

/* Follow the actual FIX end, wrap, and four-sample tail branches. */
static void sappy_cost_fixed_done(SappyCost* cost, bool tail, int32_t count, int32_t loop_length, uint32_t left)
{
    if (!cost || !tail)
        return;
    if (count == 0)
    {
        if (!loop_length)
        {
            cost->cycles += M4A_MIX_FIX_END;
            return;
        }
        cost->cycles += M4A_MIX_FIX_WRAP;
    }
    else
        cost->cycles += M4A_MIX_FIX_TAIL;
    const uint32_t index = cost->fixed_tail_size - cost->fixed_tail_left;
    if (index != 3u && left > 1u)
        cost->cycles += M4A_MIX_NEXT_SAMPLE;
    else if (index == 3u)
        cost->cycles += M4A_MIX_WORD_STORE;
    if (--cost->fixed_tail_left == 0)
        cost->cycles += left == 1u ? M4A_MIX_FIX_TAIL_DONE + M4A_MIX_STORE : M4A_MIX_NEXT_WORD;
}

/* Account for the frame clear or reverb, plus the fixed walk setup and exit. */
static int64_t sappy_frame_cycles(const M4ADriver* drv, uint32_t frame_size)
{
    int64_t cycles;
    if (drv->reverb_amount)
        cycles = M4A_MIX_REVERB + (int64_t)M4A_MIX_REVERB_SAMPLE * frame_size;
    else
    {
        const uint32_t passes = (frame_size >> 4) ? frame_size >> 4 : 1u;
        cycles = M4A_MIX_ENTRY + M4A_MIX_NOREVERB + ((frame_size & 4u) ? M4A_MIX_NOREVERB_4 : 0) +
                 ((frame_size & 8u) ? M4A_MIX_NOREVERB_8 : 0) +
                 (int64_t)passes * M4A_MIX_NOREVERB_LOOP + M4A_MIX_LOOP_LAST;
    }
    return cycles + M4A_MIX_CHAN_SETUP + M4A_MIX_SLOT_LAST + M4A_MIX_EXIT + M4A_MIX_EPILOGUE;
}

#endif
