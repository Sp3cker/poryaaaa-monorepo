#ifndef M4A_PCM_IPATIX_COST_H
#define M4A_PCM_IPATIX_COST_H

#include "m4a_pcm_cost.h"

/* Account for the channel-state decisions made before iPatix mixing. */
static int ipatix_envelope_cycles(const M4ADriverPcmChan* ch)
{
    const uint8_t status = ch->status;
    unsigned envelope = ch->envelopeVolume;
    int cycles;
    if (status & M4A_CHN_START)
    {
        if ((status & M4A_CHN_STOP) || !ch->wav || !ch->wav->data)
            return M4A_HQMIX_START_STOP + M4A_HQMIX_STOP;
        const WaveData* wav = ch->wav;
        if (!wav->size)
            cycles = ch->ipatix.synth_type == 3 ? M4A_HQMIX_INIT_SYNTH_TRI
                                                : M4A_HQMIX_INIT_SYNTH + M4A_HQMIX_INIT_CHECK;
        else if (wav->type & 1u)
            cycles = ((ch->type & VOICE_TYPE_REV) ? M4A_HQMIX_INIT_CMP_REV : M4A_HQMIX_INIT_CMP_FWD) +
                     M4A_HQMIX_INIT_CHECK;
        else
            cycles = ((ch->type & VOICE_TYPE_REV) ? M4A_HQMIX_INIT_REV : M4A_HQMIX_INIT_FWD) +
                     M4A_HQMIX_INIT_CHECK;
        cycles += M4A_HQMIX_INIT_FLAGS + ((wav->status & 0xC000u) ? M4A_HQMIX_INIT_FLAGS_LOOP : 0);
        envelope = 0;
        goto attack;
    }
    cycles = M4A_HQMIX_ENV_HEAD;
    if (status & M4A_CHN_IEC)
        return cycles + (ch->pseudoEchoLength <= 1 ? M4A_HQMIX_ENV_IEC_END + M4A_HQMIX_STOP : M4A_HQMIX_ENV_IEC);
    cycles += M4A_HQMIX_ENV_NOT_IEC;
    if (status & M4A_CHN_STOP)
    {
        envelope = (envelope * ch->release) >> 8;
        if (!envelope)
            cycles += M4A_HQMIX_ENV_RELEASE_ZERO;
        else if (envelope > ch->pseudoEchoVolume)
            return cycles + M4A_HQMIX_ENV_RELEASE;
        else
            cycles += M4A_HQMIX_ENV_RELEASE_LOW;
        goto released;
    }
    cycles += M4A_HQMIX_ENV_NOT_STOP;
    if ((status & M4A_CHN_ENV_MASK) == M4A_CHN_ENV_DECAY)
    {
        envelope = (envelope * ch->decay) >> 8;
        if (envelope > ch->sustain)
            return cycles + M4A_HQMIX_ENV_DECAY;
        if (ch->sustain)
            return cycles + M4A_HQMIX_ENV_DECAY_SUSTAIN;
        cycles += M4A_HQMIX_ENV_DECAY_ZERO;
        goto released;
    }
    cycles += M4A_HQMIX_ENV_NOT_DECAY;
    if ((status & M4A_CHN_ENV_MASK) != M4A_CHN_ENV_ATTACK)
        return cycles + M4A_HQMIX_ENV_SUSTAIN;
    cycles += M4A_HQMIX_ENV_TO_ATTACK;
attack:
    return cycles + (envelope + ch->attack >= 0xFFu ? M4A_HQMIX_ENV_ATTACK_PEAK : M4A_HQMIX_ENV_ATTACK);
released:
    return cycles + (ch->pseudoEchoVolume ? M4A_HQMIX_ENV_ECHO_START : M4A_HQMIX_ENV_ECHO_NONE + M4A_HQMIX_STOP);
}

/* F_clear_mem's aligned stores and final word loop. */
static int64_t ipatix_clear_cycles(int32_t bytes)
{
    int64_t cycles = M4A_HQMIX_CLEAR + M4A_HQMIX_CLEAR_STORE * ((bytes >> 4) & 3);
    for (;;)
    {
        bytes -= 0x40;
        if (bytes < 0)
            break;
        cycles += 1 + 3 + 4 * M4A_HQMIX_CLEAR_STORE;
    }
    cycles += 2;
    bytes &= 0xC;
    if (!bytes)
        return cycles + M4A_HQMIX_CLEAR_RET;
    return cycles + 2 + (bytes / 4) * M4A_HQMIX_CLEAR_WORD + M4A_HQMIX_LOOP_LAST + 10;
}


typedef struct
{
    int64_t cycles;
    uint32_t fixed_block_left;
    uint32_t fixed_tail_left;
    uint32_t fixed_tail_size;
    uint8_t code;
    int load;
    bool fast;
    bool skip;
    bool fixed;
    bool fixed_tail_pending;
    bool ended;
} IpatixCost;

/* Charge FIX word/tail work using the sample already read by the mixer. */
static void ipatix_cost_fixed_sample(IpatixCost* cost, const IpatixSource* source, uint32_t left, int32_t sample)
{
    if (!cost->fixed_block_left && !cost->fixed_tail_left)
    {
        if (!cost->fixed_tail_pending)
        {
            const uint32_t available = (uint32_t)source->count < left ? (uint32_t)source->count : left;
            const uint32_t blocks = (available - 1u) / 4u;
            if (blocks)
            {
                cost->cycles += M4A_HQMIX_FIX_CHECK + (int64_t)blocks * M4A_HQMIX_FIX_BLOCK +
                                M4A_HQMIX_LOOP_LAST + M4A_HQMIX_FIX_REWIND;
                cost->fixed_block_left = blocks * 4u;
                cost->fixed_tail_pending = true;
            }
            else
                cost->cycles += M4A_HQMIX_FIX_NOBLOCK;
        }
        if (!cost->fixed_block_left)
        {
            cost->cycles += M4A_HQMIX_FIX_REST;
            cost->fixed_tail_size = left < 4u ? left : 4u;
            cost->fixed_tail_left = cost->fixed_tail_size;
            cost->fixed_tail_pending = false;
        }
    }
    if (cost->fixed_block_left)
    {
        if (sample != 0)
            cost->cycles += M4A_HQMIX_FIX_NONZERO;
        --cost->fixed_block_left;
        return;
    }
    cost->cycles += M4A_HQMIX_FIX_SAMPLE;
    if (source->count == 1)
    {
        if (!source->looped)
        {
            cost->cycles += M4A_HQMIX_FIX_END + M4A_HQMIX_END_STOP;
            cost->ended = true;
            return;
        }
        cost->cycles += M4A_HQMIX_FIX_WRAP;
    }
    if (--cost->fixed_tail_left == 0)
    {
        if (cost->fixed_tail_size == 4u)
            cost->cycles += M4A_HQMIX_LOOP_LAST;
        cost->cycles += left > 1u ? M4A_HQMIX_FIX_NEXT : M4A_HQMIX_FIX_DONE + M4A_HQMIX_END_STORE;
    }
}

/* Reproduce the ROM's copy/decode/fast-path selection before sample mixing. */
static IpatixCost ipatix_cost_begin(M4ADriver* drv, const M4ADriverPcmChan* ch, const IpatixSource* source,
                                    uint32_t frame_size, uint32_t frequency, uint32_t fine_position)
{
    IpatixCost cost = {0};
    const bool compressed = source->state->compressed;
    const bool reverse = source->reverse;
    const bool fixed = (ch->type & VOICE_TYPE_FIX) != 0;
    if (fixed && !compressed && !reverse)
    {
        cost.cycles = M4A_HQMIX_TO_FIX + M4A_HQMIX_FIX_SETUP;
        if (!frame_size)
            cost.cycles += M4A_HQMIX_FIX_DONE + M4A_HQMIX_END_STORE;
        cost.fixed = true;
        return cost;
    }
    const uint32_t div = (M4A_GBA_CYCLES_PER_SECOND / (drv->pcm_rate_hz ? drv->pcm_rate_hz : M4A_PCM_RATE_HZ) + 1u) >> 1;
    cost.cycles = M4A_HQMIX_TO_COMP_REV + M4A_HQMIX_SETUP + m4a_pcm_cost_mul(div) + m4a_pcm_cost_umul(frame_size);
    const uint32_t step = fixed ? 0x800000u : frequency;
    const uint32_t advance = (uint32_t)(((uint64_t)fine_position + (uint64_t)step * frame_size) >> 23);
    if (compressed || reverse)
    {
        const int64_t idx = compressed ? source->state->source_position
                            : source->state->current_pointer
                                ? source->state->current_pointer - ch->wav->data
                                : (int64_t)source->position + 1;
        const int64_t align = compressed ? 64 : 4;
        const int64_t high = reverse ? (idx + align - 1) & -align : (idx + advance + align) & -align;
        const int64_t low = reverse ? (idx - advance - 1) & -align : idx & -align;
        int64_t bytes = high - low;
        cost.cycles += compressed ? M4A_HQMIX_SETUP_CMP : M4A_HQMIX_SETUP_UREV;
        if (compressed)
            cost.cycles += reverse ? M4A_HQMIX_COMP_REV : M4A_HQMIX_COMP;
        /* The upstream probe's default IWRAM stack permits 1220 bytes. */
        if (bytes >= 1220 || bytes < 0)
        {
            cost.cycles += (reverse ? M4A_HQMIX_STACK_SKIP_REV : M4A_HQMIX_STACK_SKIP) + M4A_HQMIX_END;
            cost.skip = true;
            return cost;
        }
        const bool end = (int64_t)source->state->count - advance <= 0;
        if (compressed)
        {
            uint32_t block = reverse ? (uint32_t)(idx - advance - 1) >> 6 : (uint32_t)idx >> 6;
            cost.cycles += reverse ? M4A_HQMIX_COMP_REV_SETUP : M4A_HQMIX_COMP_FOR;
            if (end)
            {
                const int64_t clear = reverse ? (idx - source->state->count) - low
                                              : high - ((idx + source->state->count + 63) & ~INT64_C(63));
                bytes -= clear;
                cost.cycles += (reverse ? M4A_HQMIX_COMP_REV_END - M4A_HQMIX_COMP_REV_SETUP
                                        : M4A_HQMIX_COMP_FOR_END - M4A_HQMIX_COMP_FOR) +
                               ipatix_clear_cycles((int32_t)clear);
            }
            const int64_t blocks = bytes > 0 ? (bytes + 63) / 64 : 1;
            cost.cycles += M4A_HQMIX_DECODE_SETUP + m4a_pcm_cost_mul(block) + (end ? 1 : 0) +
                           blocks * (M4A_HQMIX_BLOCK + ((reverse && low < 0) ? M4A_HQMIX_BLOCK_OPENBUS : 0)) +
                           M4A_HQMIX_LOOP_LAST + M4A_HQMIX_DECODE_DONE;
        }
        else
        {
            cost.cycles += M4A_HQMIX_UREV;
            if (end)
            {
                const int64_t clear = (idx - source->state->count) - low;
                bytes -= clear;
                cost.cycles += M4A_HQMIX_UREV_END - M4A_HQMIX_UREV + ipatix_clear_cycles((int32_t)clear);
            }
            cost.cycles += (bytes > 0 ? (bytes + 3) / 4 : 1) * M4A_HQMIX_UREV_WORD + M4A_HQMIX_LOOP_LAST +
                           M4A_HQMIX_UREV_DONE;
        }
        cost.fast = true;
        cost.load = M4A_HQMIX_LOAD_STACK;
    }
    else if ((uint64_t)source->count > advance)
    {
        cost.fast = true;
        if (advance < 0x400u && advance < 1220u)
        {
            const int32_t length = M4A_HQMIX_DMA3 + M4A_HQMIX_DMA3_WORD * (int32_t)((advance + 7u) >> 2);
            if (drv->cpu_hq_dma_count < 16)
            {
                const int index = drv->cpu_hq_dma_count++;
                drv->cpu_hq_dma_start[index] = (int32_t)(drv->cpu_mix_cycles + cost.cycles + M4A_HQMIX_COPY);
                drv->cpu_hq_dma_len[index] = length;
            }
            cost.cycles += M4A_HQMIX_COPY + length + M4A_HQMIX_VLA;
            cost.load = M4A_HQMIX_LOAD_STACK;
        }
        else
        {
            cost.cycles += M4A_HQMIX_DIRECT;
            cost.load = M4A_HQMIX_LOAD_ROM;
        }
    }
    else
        cost.cycles += M4A_HQMIX_UNBUF;
    if (cost.fast)
    {
        cost.code = step < 0x800000u ? 1u : (step < 0x1000000u ? 2u : 3u);
        cost.cycles += M4A_HQMIX_SELECT + (drv->cpu_hq_fast_code == cost.code ? 0 : M4A_HQMIX_PASTE) +
                       M4A_HQMIX_FAST_ENTRY + cost.load;
        drv->cpu_hq_fast_code = cost.code;
    }
    return cost;
}

#endif
