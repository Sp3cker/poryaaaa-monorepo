#include "hw_audio/hw_audio.h"
#include "test_assert.h"

#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

enum
{
    CONTRACT_HOST_RATE = 65536,
    CONTRACT_CYCLES_PER_HOST_FRAME = PORYAAAA_GBA_CLOCK_HZ / CONTRACT_HOST_RATE,
    CONTRACT_RENDER_FRAMES = 2048,
};

static void set_event_orders(M4ARegWrite* events, size_t count)
{
    uint64_t previous_cycle = 0;
    uint32_t order = 0;
    for (size_t index = 0; index < count; ++index)
    {
        if (index != 0 && events[index].cycle == previous_cycle)
            ++order;
        else
            order = 0;
        events[index].order = order;
        previous_cycle = events[index].cycle;
    }
}

static void render_interval(HwAudio* audio,
                            M4ARegWrite* events,
                            size_t count,
                            uint64_t begin_cycle,
                            uint64_t end_cycle,
                            int frames,
                            float* left,
                            float* right)
{
    set_event_orders(events, count);
    M4ARegWriteBatch batch = {
        .events = events,
        .count = count,
        .begin_cycle = begin_cycle,
        .end_cycle = end_cycle,
    };
    hw_audio_render_events(audio, &batch, left, right, frames);
}

static float peak_abs(const float* samples, int frames)
{
    float peak = 0.0f;
    for (int index = 0; index < frames; ++index)
    {
        float value = fabsf(samples[index]);
        if (value > peak)
            peak = value;
    }
    return peak;
}

static float peak_abs_range(const float* samples, int begin, int end)
{
    float peak = 0.0f;
    for (int index = begin; index < end; ++index)
    {
        float value = fabsf(samples[index]);
        if (value > peak)
            peak = value;
    }
    return peak;
}

static float largest_difference(const float* left, const float* right, int frames)
{
    float difference = 0.0f;
    for (int index = 0; index < frames; ++index)
    {
        float value = fabsf(left[index] - right[index]);
        if (value > difference)
            difference = value;
    }
    return difference;
}

static bool is_silent(const float* left, const float* right, int frames)
{
    return peak_abs(left, frames) < 1e-5f && peak_abs(right, frames) < 1e-5f;
}

static size_t append_square1_setup(M4ARegWrite* events, uint64_t cycle, uint8_t envelope, uint8_t duty)
{
    size_t count = 0;
    events[count++] = (M4ARegWrite){cycle, M4A_REG_NR52, 0x80, 0};
    events[count++] = (M4ARegWrite){cycle, M4A_REG_NR50, 0x77, 0};
    events[count++] = (M4ARegWrite){cycle, M4A_REG_NR51, 0x11, 0};
    events[count++] = (M4ARegWrite){cycle, M4A_REG_SOUNDCNT_H, 0x02, 0};
    events[count++] = (M4ARegWrite){cycle, M4A_REG_NR11, duty, 0};
    events[count++] = (M4ARegWrite){cycle, M4A_REG_NR12, envelope, 0};
    events[count++] = (M4ARegWrite){cycle, M4A_REG_NR13, 0xF8, 0};
    events[count++] = (M4ARegWrite){cycle, M4A_REG_NR14, 0x87, 0};
    return count;
}

static size_t append_square2_setup(M4ARegWrite* events, uint64_t cycle, uint8_t envelope, uint8_t duty)
{
    size_t count = 0;
    events[count++] = (M4ARegWrite){cycle, M4A_REG_NR52, 0x80, 0};
    events[count++] = (M4ARegWrite){cycle, M4A_REG_NR50, 0x77, 0};
    events[count++] = (M4ARegWrite){cycle, M4A_REG_NR51, 0x22, 0};
    events[count++] = (M4ARegWrite){cycle, M4A_REG_SOUNDCNT_H, 0x02, 0};
    events[count++] = (M4ARegWrite){cycle, M4A_REG_NR21, duty, 0};
    events[count++] = (M4ARegWrite){cycle, M4A_REG_NR22, envelope, 0};
    events[count++] = (M4ARegWrite){cycle, M4A_REG_NR23, 0xF8, 0};
    events[count++] = (M4ARegWrite){cycle, M4A_REG_NR24, 0x87, 0};
    return count;
}

static size_t append_wave_setup(M4ARegWrite* events, uint64_t cycle, uint32_t wave_word, uint8_t wave_volume)
{
    size_t count = 0;
    events[count++] = (M4ARegWrite){cycle, M4A_REG_NR52, 0x80, 0};
    events[count++] = (M4ARegWrite){cycle, M4A_REG_NR50, 0x77, 0};
    events[count++] = (M4ARegWrite){cycle, M4A_REG_NR51, 0x44, 0};
    events[count++] = (M4ARegWrite){cycle, M4A_REG_SOUNDCNT_H, 0x02, 0};
    events[count++] = (M4ARegWrite){cycle, M4A_REG_NR30, 0x40, 0};
    events[count++] = (M4ARegWrite){cycle, M4A_REG_WAVE_RAM_WORD_0, wave_word, 0};
    events[count++] = (M4ARegWrite){cycle, M4A_REG_NR30, 0x80, 0};
    events[count++] = (M4ARegWrite){cycle, M4A_REG_NR32, wave_volume, 0};
    events[count++] = (M4ARegWrite){cycle, M4A_REG_NR33, 0xF8, 0};
    events[count++] = (M4ARegWrite){cycle, M4A_REG_NR34, 0x87, 0};
    return count;
}

static size_t append_noise_setup(M4ARegWrite* events, uint64_t cycle)
{
    size_t count = 0;
    events[count++] = (M4ARegWrite){cycle, M4A_REG_NR52, 0x80, 0};
    events[count++] = (M4ARegWrite){cycle, M4A_REG_NR50, 0x77, 0};
    events[count++] = (M4ARegWrite){cycle, M4A_REG_NR51, 0x88, 0};
    events[count++] = (M4ARegWrite){cycle, M4A_REG_SOUNDCNT_H, 0x02, 0};
    events[count++] = (M4ARegWrite){cycle, M4A_REG_NR42, 0xF8, 0};
    events[count++] = (M4ARegWrite){cycle, M4A_REG_NR43, 0x00, 0};
    events[count++] = (M4ARegWrite){cycle, M4A_REG_NR44, 0x80, 0};
    return count;
}

/* Production rendering observes the event order, so a DMA word must be visible
 * to its same-cycle TIMER only when the write is ordered before that TIMER. */
static void test_same_cycle_fifo_write_precedes_timer(void)
{
    printf("Testing production chip contracts: same-cycle FIFO write ordering...\n");
    float write_first_left[CONTRACT_RENDER_FRAMES] = {0};
    float write_first_right[CONTRACT_RENDER_FRAMES] = {0};
    float timer_first_left[CONTRACT_RENDER_FRAMES] = {0};
    float timer_first_right[CONTRACT_RENDER_FRAMES] = {0};
    const uint64_t end_cycle = CONTRACT_RENDER_FRAMES * CONTRACT_CYCLES_PER_HOST_FRAME;

    M4ARegWrite write_first[] = {
        {0, M4A_REG_NR52, 0x80, 0},
        {0, M4A_REG_SOUNDCNT_H, 0x0304, 0},
        {0, M4A_REG_FIFO_A, 0x0000007F, 0},
        {0, M4A_REG_TIMER_0, 0, 0},
    };
    M4ARegWrite timer_first[] = {
        {0, M4A_REG_NR52, 0x80, 0},
        {0, M4A_REG_SOUNDCNT_H, 0x0304, 0},
        {0, M4A_REG_TIMER_0, 0, 0},
        {0, M4A_REG_FIFO_A, 0x0000007F, 0},
    };

    HwAudio* write_before_timer = hw_audio_create((float)CONTRACT_HOST_RATE);
    HwAudio* timer_before_write = hw_audio_create((float)CONTRACT_HOST_RATE);
    ASSERT(write_before_timer != NULL && timer_before_write != NULL, "HwAudio allocations succeed");
    if (write_before_timer && timer_before_write)
    {
        render_interval(write_before_timer,
                        write_first,
                        sizeof(write_first) / sizeof(write_first[0]),
                        0,
                        end_cycle,
                        CONTRACT_RENDER_FRAMES,
                        write_first_left,
                        write_first_right);
        render_interval(timer_before_write,
                        timer_first,
                        sizeof(timer_first) / sizeof(timer_first[0]),
                        0,
                        end_cycle,
                        CONTRACT_RENDER_FRAMES,
                        timer_first_left,
                        timer_first_right);
        ASSERT(peak_abs(write_first_left, CONTRACT_RENDER_FRAMES) > 0.001f,
               "same-cycle FIFO write is audible when it precedes TIMER_0");
        ASSERT(is_silent(timer_first_left, timer_first_right, CONTRACT_RENDER_FRAMES),
               "TIMER_0 before its same-cycle FIFO write preserves empty-FIFO silence");
    }
    hw_audio_destroy(write_before_timer);
    hw_audio_destroy(timer_before_write);
}

/* A non-DAC-cycle register write affects elapsed oscillator phase. The
 * selected period exposes that phase displacement at a later DAC sample. */
static void test_mid_sample_phase_tracks_cycle_delta(void)
{
    printf("Testing production chip contracts: exact mid-sample square phase...\n");
    float early_left[CONTRACT_RENDER_FRAMES] = {0};
    float early_right[CONTRACT_RENDER_FRAMES] = {0};
    float late_left[CONTRACT_RENDER_FRAMES] = {0};
    float late_right[CONTRACT_RENDER_FRAMES] = {0};
    const uint64_t end_cycle = CONTRACT_RENDER_FRAMES * CONTRACT_CYCLES_PER_HOST_FRAME;

    M4ARegWrite early[10];
    M4ARegWrite late[10];
    size_t early_count = append_square1_setup(early, 0, 0xF8, 0x80);
    size_t late_count = append_square1_setup(late, 0, 0xF8, 0x80);
    early[early_count++] = (M4ARegWrite){128, M4A_REG_NR13, 0xFB, 0};
    late[late_count++] = (M4ARegWrite){256, M4A_REG_NR13, 0xFB, 0};

    HwAudio* early_write = hw_audio_create((float)CONTRACT_HOST_RATE);
    HwAudio* late_write = hw_audio_create((float)CONTRACT_HOST_RATE);
    ASSERT(early_write != NULL && late_write != NULL, "HwAudio allocations succeed");
    if (early_write && late_write)
    {
        render_interval(early_write, early, early_count, 0, end_cycle, CONTRACT_RENDER_FRAMES, early_left, early_right);
        render_interval(late_write, late, late_count, 0, end_cycle, CONTRACT_RENDER_FRAMES, late_left, late_right);
        ASSERT(largest_difference(early_left, late_left, CONTRACT_RENDER_FRAMES) > 1e-4f,
               "a 128-cycle frequency-write displacement changes rendered square phase");
    }
    hw_audio_destroy(early_write);
    hw_audio_destroy(late_write);
}

/* NR52 enable does not rebase the frame sequencer: a late enable sees the same
 * absolute frame cadence as one rendered in a single uninterrupted interval. */
static void test_nr52_frame_cadence_remains_absolute(void)
{
    printf("Testing production chip contracts: absolute NR52 frame cadence...\n");
    enum
    {
        FRAMES = 1200,
    };
    float early_left[FRAMES] = {0};
    float early_right[FRAMES] = {0};
    float delayed_left[FRAMES] = {0};
    float delayed_right[FRAMES] = {0};
    const uint64_t end_cycle = (uint64_t)FRAMES * CONTRACT_CYCLES_PER_HOST_FRAME;

    M4ARegWrite early[10];
    M4ARegWrite delayed[10];
    size_t early_count = append_square2_setup(early, 0, 0x29, 0x80);
    size_t delayed_count = append_square2_setup(delayed, 6483, 0x29, 0x80);

    HwAudio* early_enable = hw_audio_create((float)CONTRACT_HOST_RATE);
    HwAudio* delayed_enable = hw_audio_create((float)CONTRACT_HOST_RATE);
    ASSERT(early_enable != NULL && delayed_enable != NULL, "HwAudio allocations succeed");
    if (early_enable && delayed_enable)
    {
        render_interval(early_enable, early, early_count, 0, end_cycle, FRAMES, early_left, early_right);
        render_interval(delayed_enable, delayed, delayed_count, 0, end_cycle, FRAMES, delayed_left, delayed_right);
        ASSERT(peak_abs(delayed_left, FRAMES) > 0.001f,
               "late NR52 enable remains audible through the production renderer");
        ASSERT(largest_difference(early_left, delayed_left, FRAMES) > 1e-4f,
               "late NR52 enable retains its absolute frame-sequencer phase");
    }
    hw_audio_destroy(early_enable);
    hw_audio_destroy(delayed_enable);
}

/* Square output is a held DAC latch: duty writes wait for an oscillator edge;
 * NRx4 triggers and the envelope frame tick refresh the observable latch. */
static void test_square_latched_samples_refresh_at_hardware_edges(void)
{
    printf("Testing production chip contracts: square sample latch refreshes...\n");
    enum
    {
        FRAMES = 2048,
    };
    float baseline_left[FRAMES] = {0};
    float baseline_right[FRAMES] = {0};
    float at_dac_left[FRAMES] = {0};
    float at_dac_right[FRAMES] = {0};
    float after_dac_left[FRAMES] = {0};
    float after_dac_right[FRAMES] = {0};
    const uint64_t end_cycle = (uint64_t)FRAMES * CONTRACT_CYCLES_PER_HOST_FRAME;

    M4ARegWrite baseline_events[10];
    M4ARegWrite at_dac_events[10];
    M4ARegWrite after_dac_events[10];
    size_t baseline_count = append_square1_setup(baseline_events, 0, 0xF8, 0x80);
    size_t at_dac_count = append_square1_setup(at_dac_events, 0, 0xF8, 0x80);
    size_t after_dac_count = append_square1_setup(after_dac_events, 0, 0xF8, 0x80);
    at_dac_events[at_dac_count++] = (M4ARegWrite){256, M4A_REG_NR11, 0x00, 0};
    after_dac_events[after_dac_count++] = (M4ARegWrite){257, M4A_REG_NR11, 0x00, 0};

    HwAudio* baseline = hw_audio_create((float)CONTRACT_HOST_RATE);
    HwAudio* at_dac_boundary = hw_audio_create((float)CONTRACT_HOST_RATE);
    HwAudio* after_dac_boundary = hw_audio_create((float)CONTRACT_HOST_RATE);
    ASSERT(baseline != NULL && at_dac_boundary != NULL && after_dac_boundary != NULL, "HwAudio allocations succeed");
    if (baseline && at_dac_boundary && after_dac_boundary)
    {
        render_interval(baseline, baseline_events, baseline_count, 0, end_cycle, FRAMES, baseline_left, baseline_right);
        render_interval(at_dac_boundary, at_dac_events, at_dac_count, 0, end_cycle, FRAMES, at_dac_left, at_dac_right);
        render_interval(after_dac_boundary,
                        after_dac_events,
                        after_dac_count,
                        0,
                        end_cycle,
                        FRAMES,
                        after_dac_left,
                        after_dac_right);
        ASSERT(largest_difference(at_dac_left, after_dac_left, FRAMES) < 1e-4f,
               "duty write at a DAC boundary holds the prior square sample until its oscillator edge");
        ASSERT(largest_difference(baseline_left, at_dac_left, FRAMES) > 1e-4f,
               "duty write becomes audible after a later oscillator refresh");
    }
    hw_audio_destroy(baseline);
    hw_audio_destroy(at_dac_boundary);
    hw_audio_destroy(after_dac_boundary);

    M4ARegWrite sq1_trigger[] = {
        {0, M4A_REG_NR52, 0x80, 0},
        {0, M4A_REG_NR50, 0x77, 0},
        {0, M4A_REG_NR51, 0x11, 0},
        {0, M4A_REG_SOUNDCNT_H, 0x02, 0},
        {0, M4A_REG_NR11, 0x80, 0},
        {0, M4A_REG_NR12, 0xF8, 0},
        {0, M4A_REG_NR13, 0xF8, 0},
        {512, M4A_REG_NR14, 0x87, 0},
    };
    M4ARegWrite sq2_trigger[] = {
        {0, M4A_REG_NR52, 0x80, 0},
        {0, M4A_REG_NR50, 0x77, 0},
        {0, M4A_REG_NR51, 0x22, 0},
        {0, M4A_REG_SOUNDCNT_H, 0x02, 0},
        {0, M4A_REG_NR21, 0x80, 0},
        {0, M4A_REG_NR22, 0xF8, 0},
        {0, M4A_REG_NR23, 0xF8, 0},
        {512, M4A_REG_NR24, 0x87, 0},
    };
    float trigger_left[FRAMES] = {0};
    float trigger_right[FRAMES] = {0};
    HwAudio* sq1 = hw_audio_create((float)CONTRACT_HOST_RATE);
    HwAudio* sq2 = hw_audio_create((float)CONTRACT_HOST_RATE);
    ASSERT(sq1 != NULL && sq2 != NULL, "HwAudio allocations succeed");
    if (sq1 && sq2)
    {
        render_interval(sq1,
                        sq1_trigger,
                        sizeof(sq1_trigger) / sizeof(sq1_trigger[0]),
                        0,
                        end_cycle,
                        FRAMES,
                        trigger_left,
                        trigger_right);
        ASSERT(peak_abs(trigger_left, FRAMES) > 0.001f, "NR14 trigger refreshes an audible square latch");
        memset(trigger_left, 0, sizeof(trigger_left));
        memset(trigger_right, 0, sizeof(trigger_right));
        render_interval(sq2,
                        sq2_trigger,
                        sizeof(sq2_trigger) / sizeof(sq2_trigger[0]),
                        0,
                        end_cycle,
                        FRAMES,
                        trigger_left,
                        trigger_right);
        ASSERT(peak_abs(trigger_left, FRAMES) > 0.001f, "NR24 trigger refreshes an audible square latch");
    }
    hw_audio_destroy(sq1);
    hw_audio_destroy(sq2);

    M4ARegWrite envelope[10];
    float envelope_left[FRAMES] = {0};
    float envelope_right[FRAMES] = {0};
    size_t envelope_count = append_square1_setup(envelope, 0, 0x19, 0x80);
    HwAudio* envelope_audio = hw_audio_create((float)CONTRACT_HOST_RATE);
    ASSERT(envelope_audio != NULL, "HwAudio allocation succeeds");
    if (envelope_audio)
    {
        render_interval(envelope_audio, envelope, envelope_count, 0, end_cycle, FRAMES, envelope_left, envelope_right);
        ASSERT(peak_abs_range(envelope_left, 1024, FRAMES) > peak_abs_range(envelope_left, 32, 512),
               "envelope frame tick refreshes the held square sample at its new volume");
    }
    hw_audio_destroy(envelope_audio);
}

static void test_wave_ram_bank_and_trigger_delay(void)
{
    printf("Testing production chip contracts: Wave RAM bank and trigger delay...\n");
    float populated_left[CONTRACT_RENDER_FRAMES] = {0};
    float populated_right[CONTRACT_RENDER_FRAMES] = {0};
    float empty_left[CONTRACT_RENDER_FRAMES] = {0};
    float empty_right[CONTRACT_RENDER_FRAMES] = {0};
    const uint64_t end_cycle = CONTRACT_RENDER_FRAMES * CONTRACT_CYCLES_PER_HOST_FRAME;
    M4ARegWrite populated[12];
    M4ARegWrite empty[12];
    size_t populated_count = append_wave_setup(populated, 0, 0x67452312u, 0x20);
    size_t empty_count = append_wave_setup(empty, 0, 0, 0x20);

    HwAudio* banked = hw_audio_create((float)CONTRACT_HOST_RATE);
    HwAudio* zeroed = hw_audio_create((float)CONTRACT_HOST_RATE);
    ASSERT(banked != NULL && zeroed != NULL, "HwAudio allocations succeed");
    if (banked && zeroed)
    {
        render_interval(
            banked, populated, populated_count, 0, end_cycle, CONTRACT_RENDER_FRAMES, populated_left, populated_right);
        render_interval(zeroed, empty, empty_count, 0, end_cycle, CONTRACT_RENDER_FRAMES, empty_left, empty_right);
        ASSERT(largest_difference(populated_left, empty_left, CONTRACT_RENDER_FRAMES) > 1e-4f,
               "Wave RAM bank data becomes audible only after the triggered delayed Wave clock");
    }
    hw_audio_destroy(banked);
    hw_audio_destroy(zeroed);
}

static void test_delayed_frame_callback_clocks_wave_once(void)
{
    printf("Testing production chip contracts: delayed frame boundary clocks Wave once...\n");
    enum
    {
        FIRST_FRAMES = 128,
        SECOND_FRAMES = 128,
    };
    const uint64_t frame_boundary = (uint64_t)FIRST_FRAMES * CONTRACT_CYCLES_PER_HOST_FRAME;
    const uint64_t end_cycle = frame_boundary + (uint64_t)SECOND_FRAMES * CONTRACT_CYCLES_PER_HOST_FRAME;
    float one_left[FIRST_FRAMES + SECOND_FRAMES] = {0};
    float one_right[FIRST_FRAMES + SECOND_FRAMES] = {0};
    float split_left[FIRST_FRAMES + SECOND_FRAMES] = {0};
    float split_right[FIRST_FRAMES + SECOND_FRAMES] = {0};
    M4ARegWrite events[12];
    size_t count = append_wave_setup(events, frame_boundary - 37, 0x67452312u, 0x20);

    HwAudio* one_call = hw_audio_create((float)CONTRACT_HOST_RATE);
    HwAudio* split_calls = hw_audio_create((float)CONTRACT_HOST_RATE);
    ASSERT(one_call != NULL && split_calls != NULL, "HwAudio allocations succeed");
    if (one_call && split_calls)
    {
        render_interval(one_call, events, count, 0, end_cycle, FIRST_FRAMES + SECOND_FRAMES, one_left, one_right);
        render_interval(split_calls, events, count, 0, frame_boundary, FIRST_FRAMES, split_left, split_right);
        render_interval(split_calls,
                        NULL,
                        0,
                        frame_boundary,
                        end_cycle,
                        SECOND_FRAMES,
                        split_left + FIRST_FRAMES,
                        split_right + FIRST_FRAMES);
        ASSERT(largest_difference(one_left, split_left, FIRST_FRAMES + SECOND_FRAMES) < 1e-4f,
               "frame-boundary partition does not double-clock Wave state");
    }
    hw_audio_destroy(one_call);
    hw_audio_destroy(split_calls);
}

static void test_wave_nr30_selects_bank_before_clock(void)
{
    printf("Testing production chip contracts: NR30 bank selection precedes Wave clock...\n");
    float selected_left[CONTRACT_RENDER_FRAMES] = {0};
    float selected_right[CONTRACT_RENDER_FRAMES] = {0};
    float original_left[CONTRACT_RENDER_FRAMES] = {0};
    float original_right[CONTRACT_RENDER_FRAMES] = {0};
    const uint64_t end_cycle = CONTRACT_RENDER_FRAMES * CONTRACT_CYCLES_PER_HOST_FRAME;
    M4ARegWrite selected[16];
    M4ARegWrite original[16];
    size_t selected_count = append_wave_setup(selected, 0, 0x11111111u, 0x20);
    size_t original_count = append_wave_setup(original, 0, 0x11111111u, 0x20);
    selected[selected_count++] = (M4ARegWrite){32, M4A_REG_NR30, 0x00, 0};
    selected[selected_count++] = (M4ARegWrite){32, M4A_REG_WAVE_RAM_WORD_0, 0x77777777u, 0};
    selected[selected_count++] = (M4ARegWrite){88, M4A_REG_NR30, 0xC0, 0};

    HwAudio* selected_bank = hw_audio_create((float)CONTRACT_HOST_RATE);
    HwAudio* original_bank = hw_audio_create((float)CONTRACT_HOST_RATE);
    ASSERT(selected_bank != NULL && original_bank != NULL, "HwAudio allocations succeed");
    if (selected_bank && original_bank)
    {
        render_interval(selected_bank,
                        selected,
                        selected_count,
                        0,
                        end_cycle,
                        CONTRACT_RENDER_FRAMES,
                        selected_left,
                        selected_right);
        render_interval(original_bank,
                        original,
                        original_count,
                        0,
                        end_cycle,
                        CONTRACT_RENDER_FRAMES,
                        original_left,
                        original_right);
        ASSERT(largest_difference(selected_left, original_left, CONTRACT_RENDER_FRAMES) > 1e-4f,
               "NR30 selection at the pending Wave clock changes the observed bank");
    }
    hw_audio_destroy(selected_bank);
    hw_audio_destroy(original_bank);
}

/* In 64-sample mode, a Wave RAM write immediately before the pending clock
 * is transformed by it; the same-clock write follows it. */
static void test_wave_ram_write_forces_pending_clock(void)
{
    printf("Testing production chip contracts: Wave RAM write catches up pending clock...\n");
    float caught_up_left[CONTRACT_RENDER_FRAMES] = {0};
    float caught_up_right[CONTRACT_RENDER_FRAMES] = {0};
    float later_left[CONTRACT_RENDER_FRAMES] = {0};
    float later_right[CONTRACT_RENDER_FRAMES] = {0};
    const uint64_t end_cycle = CONTRACT_RENDER_FRAMES * CONTRACT_CYCLES_PER_HOST_FRAME;
    M4ARegWrite caught_up[16];
    M4ARegWrite later[16];
    size_t caught_up_count = append_wave_setup(caught_up, 0, 0x12345678u, 0x20);
    size_t later_count = append_wave_setup(later, 0, 0x12345678u, 0x20);
    caught_up[caught_up_count++] = (M4ARegWrite){0, M4A_REG_NR30, 0xA0, 0};
    later[later_count++] = (M4ARegWrite){0, M4A_REG_NR30, 0xA0, 0};
    caught_up[caught_up_count++] = (M4ARegWrite){87, M4A_REG_WAVE_RAM_WORD_0, 0x00000056u, 0};
    later[later_count++] = (M4ARegWrite){88, M4A_REG_WAVE_RAM_WORD_0, 0x00000056u, 0};

    HwAudio* write_at_clock = hw_audio_create((float)CONTRACT_HOST_RATE);
    HwAudio* write_after_clock = hw_audio_create((float)CONTRACT_HOST_RATE);
    ASSERT(write_at_clock != NULL && write_after_clock != NULL, "HwAudio allocations succeed");
    if (write_at_clock && write_after_clock)
    {
        render_interval(write_at_clock,
                        caught_up,
                        caught_up_count,
                        0,
                        end_cycle,
                        CONTRACT_RENDER_FRAMES,
                        caught_up_left,
                        caught_up_right);
        render_interval(
            write_after_clock, later, later_count, 0, end_cycle, CONTRACT_RENDER_FRAMES, later_left, later_right);
        ASSERT(largest_difference(caught_up_left, later_left, CONTRACT_RENDER_FRAMES) > 1e-4f,
               "Wave RAM write around a pending clock preserves its event order");
    }
    hw_audio_destroy(write_at_clock);
    hw_audio_destroy(write_after_clock);
}

static void test_wave_nr52_disable_preserves_residual_ram_phase(void)
{
    printf("Testing production chip contracts: NR52 retains residual Wave phase...\n");
    float retained_left[CONTRACT_RENDER_FRAMES] = {0};
    float retained_right[CONTRACT_RENDER_FRAMES] = {0};
    float restarted_left[CONTRACT_RENDER_FRAMES] = {0};
    float restarted_right[CONTRACT_RENDER_FRAMES] = {0};
    const uint64_t end_cycle = CONTRACT_RENDER_FRAMES * CONTRACT_CYCLES_PER_HOST_FRAME;
    M4ARegWrite retained[20];
    M4ARegWrite restarted[20];
    size_t retained_count = append_wave_setup(retained, 0, 0x67452312u, 0x20);
    size_t restarted_count = append_wave_setup(restarted, 160, 0x67452312u, 0x20);
    retained[retained_count++] = (M4ARegWrite){160, M4A_REG_NR52, 0x00, 0};
    retained[retained_count++] = (M4ARegWrite){160, M4A_REG_NR52, 0x80, 0};
    retained[retained_count++] = (M4ARegWrite){160, M4A_REG_NR30, 0x80, 0};
    retained[retained_count++] = (M4ARegWrite){160, M4A_REG_NR32, 0x20, 0};
    retained[retained_count++] = (M4ARegWrite){160, M4A_REG_NR34, 0x87, 0};

    HwAudio* disabled = hw_audio_create((float)CONTRACT_HOST_RATE);
    HwAudio* fresh = hw_audio_create((float)CONTRACT_HOST_RATE);
    ASSERT(disabled != NULL && fresh != NULL, "HwAudio allocations succeed");
    if (disabled && fresh)
    {
        render_interval(
            disabled, retained, retained_count, 0, end_cycle, CONTRACT_RENDER_FRAMES, retained_left, retained_right);
        render_interval(
            fresh, restarted, restarted_count, 0, end_cycle, CONTRACT_RENDER_FRAMES, restarted_left, restarted_right);
        ASSERT(peak_abs(retained_left, CONTRACT_RENDER_FRAMES) > 0.001f,
               "Wave resumes audibly after an NR52 disable-enable interval");
        ASSERT(largest_difference(retained_left, restarted_left, CONTRACT_RENDER_FRAMES) > 1e-4f,
               "NR52 disable preserves residual Wave phase instead of resetting it");
    }
    hw_audio_destroy(disabled);
    hw_audio_destroy(fresh);
}

static void test_wave_length_expiry_gates_sparse_delta(void)
{
    printf("Testing production chip contracts: Wave length expiry across sparse delta...\n");
    enum
    {
        FRAMES = 1024,
        TAIL_BEGIN = 896,
    };
    float gated_left[FRAMES] = {0};
    float gated_right[FRAMES] = {0};
    float sustained_left[FRAMES] = {0};
    float sustained_right[FRAMES] = {0};
    const uint64_t end_cycle = (uint64_t)FRAMES * CONTRACT_CYCLES_PER_HOST_FRAME;
    M4ARegWrite gated[12];
    M4ARegWrite sustained[12];
    size_t gated_count = append_wave_setup(gated, 0, 0x67452312u, 0x20);
    size_t sustained_count = append_wave_setup(sustained, 0, 0x67452312u, 0x20);
    gated[gated_count - 1] = (M4ARegWrite){0, M4A_REG_NR34, 0xC7, 0};
    gated[gated_count++] = (M4ARegWrite){0, M4A_REG_NR31, 255, 0};

    HwAudio* length_gated = hw_audio_create((float)CONTRACT_HOST_RATE);
    HwAudio* no_length_gate = hw_audio_create((float)CONTRACT_HOST_RATE);
    ASSERT(length_gated != NULL && no_length_gate != NULL, "HwAudio allocations succeed");
    if (length_gated && no_length_gate)
    {
        render_interval(length_gated, gated, gated_count, 0, end_cycle, FRAMES, gated_left, gated_right);
        render_interval(
            no_length_gate, sustained, sustained_count, 0, end_cycle, FRAMES, sustained_left, sustained_right);
        ASSERT(peak_abs_range(gated_left, TAIL_BEGIN, FRAMES) < 1e-4f,
               "Wave length expiry silences the tail of one sparse render interval");
        ASSERT(peak_abs_range(sustained_left, TAIL_BEGIN, FRAMES) > 0.001f,
               "without NR34 length enable the same sparse render remains audible");
    }
    hw_audio_destroy(length_gated);
    hw_audio_destroy(no_length_gate);
}

static void test_wave_repeated_nr52_off_clears_bank(void)
{
    printf("Testing production chip contracts: repeated NR52-off clears Wave bank state...\n");
    float repeated_left[CONTRACT_RENDER_FRAMES] = {0};
    float repeated_right[CONTRACT_RENDER_FRAMES] = {0};
    float clean_left[CONTRACT_RENDER_FRAMES] = {0};
    float clean_right[CONTRACT_RENDER_FRAMES] = {0};
    const uint64_t end_cycle = CONTRACT_RENDER_FRAMES * CONTRACT_CYCLES_PER_HOST_FRAME;
    M4ARegWrite repeated[] = {
        {0, M4A_REG_NR30, 0x60, 0},
        {0, M4A_REG_NR52, 0x00, 0},
        {0, M4A_REG_NR52, 0x80, 0},
        {0, M4A_REG_NR50, 0x77, 0},
        {0, M4A_REG_NR51, 0x44, 0},
        {0, M4A_REG_SOUNDCNT_H, 0x02, 0},
        {0, M4A_REG_WAVE_RAM_WORD_0, 0x12121212u, 0},
        {0, M4A_REG_NR30, 0xC0, 0},
        {0, M4A_REG_NR32, 0x20, 0},
        {0, M4A_REG_NR33, 0xF8, 0},
        {0, M4A_REG_NR34, 0x87, 0},
    };
    M4ARegWrite clean[] = {
        {0, M4A_REG_NR52, 0x80, 0},
        {0, M4A_REG_NR50, 0x77, 0},
        {0, M4A_REG_NR51, 0x44, 0},
        {0, M4A_REG_SOUNDCNT_H, 0x02, 0},
        {0, M4A_REG_WAVE_RAM_WORD_0, 0x12121212u, 0},
        {0, M4A_REG_NR30, 0xC0, 0},
        {0, M4A_REG_NR32, 0x20, 0},
        {0, M4A_REG_NR33, 0xF8, 0},
        {0, M4A_REG_NR34, 0x87, 0},
    };

    HwAudio* repeated_off = hw_audio_create((float)CONTRACT_HOST_RATE);
    HwAudio* clean_start = hw_audio_create((float)CONTRACT_HOST_RATE);
    ASSERT(repeated_off != NULL && clean_start != NULL, "HwAudio allocations succeed");
    if (repeated_off && clean_start)
    {
        render_interval(repeated_off,
                        repeated,
                        sizeof(repeated) / sizeof(repeated[0]),
                        0,
                        end_cycle,
                        CONTRACT_RENDER_FRAMES,
                        repeated_left,
                        repeated_right);
        render_interval(clean_start,
                        clean,
                        sizeof(clean) / sizeof(clean[0]),
                        0,
                        end_cycle,
                        CONTRACT_RENDER_FRAMES,
                        clean_left,
                        clean_right);
        ASSERT(largest_difference(repeated_left, clean_left, CONTRACT_RENDER_FRAMES) < 1e-4f,
               "repeated NR52-off leaves the same cleared Wave bank state as a clean start");
    }
    hw_audio_destroy(repeated_off);
    hw_audio_destroy(clean_start);
}

static void test_fifo_stereo_little_endian(void)
{
    printf("Testing production chip contracts: FIFO stereo and little-endian order...\n");
    float stereo_left[CONTRACT_RENDER_FRAMES] = {0};
    float stereo_right[CONTRACT_RENDER_FRAMES] = {0};
    float lsb_left[CONTRACT_RENDER_FRAMES] = {0};
    float lsb_right[CONTRACT_RENDER_FRAMES] = {0};
    float msb_left[CONTRACT_RENDER_FRAMES] = {0};
    float msb_right[CONTRACT_RENDER_FRAMES] = {0};
    const uint64_t end_cycle = CONTRACT_RENDER_FRAMES * CONTRACT_CYCLES_PER_HOST_FRAME;
    M4ARegWrite stereo[] = {
        {0, M4A_REG_NR52, 0x80, 0},
        {0, M4A_REG_SOUNDCNT_H, 0x230C, 0},
        {0, M4A_REG_FIFO_A, 0x0000007F, 0},
        {0, M4A_REG_FIFO_B, 0x00000080, 0},
        {0, M4A_REG_TIMER_0, 0, 0},
    };
    M4ARegWrite lsb[] = {
        {0, M4A_REG_NR52, 0x80, 0},
        {0, M4A_REG_SOUNDCNT_H, 0x0304, 0},
        {0, M4A_REG_FIFO_A, 0x0000007F, 0},
        {0, M4A_REG_TIMER_0, 0, 0},
    };
    M4ARegWrite msb[] = {
        {0, M4A_REG_NR52, 0x80, 0},
        {0, M4A_REG_SOUNDCNT_H, 0x0304, 0},
        {0, M4A_REG_FIFO_A, 0x7F000000, 0},
        {0, M4A_REG_TIMER_0, 0, 0},
    };

    HwAudio* stereo_audio = hw_audio_create((float)CONTRACT_HOST_RATE);
    HwAudio* lsb_audio = hw_audio_create((float)CONTRACT_HOST_RATE);
    HwAudio* msb_audio = hw_audio_create((float)CONTRACT_HOST_RATE);
    ASSERT(stereo_audio != NULL && lsb_audio != NULL && msb_audio != NULL, "HwAudio allocations succeed");
    if (stereo_audio && lsb_audio && msb_audio)
    {
        render_interval(stereo_audio,
                        stereo,
                        sizeof(stereo) / sizeof(stereo[0]),
                        0,
                        end_cycle,
                        CONTRACT_RENDER_FRAMES,
                        stereo_left,
                        stereo_right);
        render_interval(
            lsb_audio, lsb, sizeof(lsb) / sizeof(lsb[0]), 0, end_cycle, CONTRACT_RENDER_FRAMES, lsb_left, lsb_right);
        render_interval(
            msb_audio, msb, sizeof(msb) / sizeof(msb[0]), 0, end_cycle, CONTRACT_RENDER_FRAMES, msb_left, msb_right);
        ASSERT(peak_abs(stereo_left, CONTRACT_RENDER_FRAMES) > 0.001f &&
                   peak_abs(stereo_right, CONTRACT_RENDER_FRAMES) > 0.001f,
               "FIFO A and B route independently to their selected stereo sides");
        ASSERT(largest_difference(stereo_left, stereo_right, CONTRACT_RENDER_FRAMES) > 1e-4f,
               "opposite signed FIFO channels remain stereo-distinct");
        ASSERT(peak_abs(lsb_left, CONTRACT_RENDER_FRAMES) > 0.001f,
               "first FIFO clock exposes the little-endian low byte");
        ASSERT(is_silent(msb_left, msb_right, CONTRACT_RENDER_FRAMES),
               "most-significant FIFO byte is not consumed before the low byte");
    }
    hw_audio_destroy(stereo_audio);
    hw_audio_destroy(lsb_audio);
    hw_audio_destroy(msb_audio);
}

static void test_fifo_reset_preserves_current_word(void)
{
    printf("Testing production chip contracts: FIFO reset preserves in-flight word...\n");
    float reset_left[CONTRACT_RENDER_FRAMES] = {0};
    float reset_right[CONTRACT_RENDER_FRAMES] = {0};
    float empty_left[CONTRACT_RENDER_FRAMES] = {0};
    float empty_right[CONTRACT_RENDER_FRAMES] = {0};
    const uint64_t end_cycle = CONTRACT_RENDER_FRAMES * CONTRACT_CYCLES_PER_HOST_FRAME;
    M4ARegWrite reset[] = {
        {0, M4A_REG_NR52, 0x80, 0},
        {0, M4A_REG_SOUNDCNT_H, 0x0304, 0},
        {0, M4A_REG_FIFO_A, 0x00000B07, 0},
        {0, M4A_REG_TIMER_0, 0, 0},
        {256, M4A_REG_SOUNDCNT_H, 0x0B04, 0},
        {256, M4A_REG_TIMER_0, 0, 0},
    };
    M4ARegWrite empty[] = {
        {0, M4A_REG_NR52, 0x80, 0},
        {0, M4A_REG_SOUNDCNT_H, 0x0304, 0},
        {0, M4A_REG_TIMER_0, 0, 0},
    };

    HwAudio* reset_audio = hw_audio_create((float)CONTRACT_HOST_RATE);
    HwAudio* empty_audio = hw_audio_create((float)CONTRACT_HOST_RATE);
    ASSERT(reset_audio != NULL && empty_audio != NULL, "HwAudio allocations succeed");
    if (reset_audio && empty_audio)
    {
        render_interval(reset_audio,
                        reset,
                        sizeof(reset) / sizeof(reset[0]),
                        0,
                        end_cycle,
                        CONTRACT_RENDER_FRAMES,
                        reset_left,
                        reset_right);
        render_interval(empty_audio,
                        empty,
                        sizeof(empty) / sizeof(empty[0]),
                        0,
                        end_cycle,
                        CONTRACT_RENDER_FRAMES,
                        empty_left,
                        empty_right);
        ASSERT(peak_abs(reset_left, CONTRACT_RENDER_FRAMES) > 0.001f,
               "FIFO reset retains the in-flight word's next little-endian byte");
        ASSERT(is_silent(empty_left, empty_right, CONTRACT_RENDER_FRAMES), "empty FIFO baseline remains silent");
    }
    hw_audio_destroy(reset_audio);
    hw_audio_destroy(empty_audio);
}

static void test_timer_selection_and_empty_fifo_silence(void)
{
    printf("Testing production chip contracts: timer selection and empty FIFO silence...\n");
    float timer0_left[CONTRACT_RENDER_FRAMES] = {0};
    float timer0_right[CONTRACT_RENDER_FRAMES] = {0};
    float timer1_left[CONTRACT_RENDER_FRAMES] = {0};
    float timer1_right[CONTRACT_RENDER_FRAMES] = {0};
    float empty_left[CONTRACT_RENDER_FRAMES] = {0};
    float empty_right[CONTRACT_RENDER_FRAMES] = {0};
    const uint64_t end_cycle = CONTRACT_RENDER_FRAMES * CONTRACT_CYCLES_PER_HOST_FRAME;
    M4ARegWrite timer0[] = {
        {0, M4A_REG_NR52, 0x80, 0},
        {0, M4A_REG_SOUNDCNT_H, 0x0704, 0},
        {0, M4A_REG_FIFO_A, 0x0000007F, 0},
        {0, M4A_REG_TIMER_0, 0, 0},
    };
    M4ARegWrite timer1[] = {
        {0, M4A_REG_NR52, 0x80, 0},
        {0, M4A_REG_SOUNDCNT_H, 0x0704, 0},
        {0, M4A_REG_FIFO_A, 0x0000007F, 0},
        {0, M4A_REG_TIMER_1, 0, 0},
    };
    M4ARegWrite empty[] = {
        {0, M4A_REG_NR52, 0x80, 0},
        {0, M4A_REG_SOUNDCNT_H, 0x0704, 0},
        {0, M4A_REG_TIMER_1, 0, 0},
        {256, M4A_REG_TIMER_1, 0, 0},
        {512, M4A_REG_TIMER_1, 0, 0},
        {768, M4A_REG_TIMER_1, 0, 0},
    };

    HwAudio* unselected = hw_audio_create((float)CONTRACT_HOST_RATE);
    HwAudio* selected = hw_audio_create((float)CONTRACT_HOST_RATE);
    HwAudio* empty_audio = hw_audio_create((float)CONTRACT_HOST_RATE);
    ASSERT(unselected != NULL && selected != NULL && empty_audio != NULL, "HwAudio allocations succeed");
    if (unselected && selected && empty_audio)
    {
        render_interval(unselected,
                        timer0,
                        sizeof(timer0) / sizeof(timer0[0]),
                        0,
                        end_cycle,
                        CONTRACT_RENDER_FRAMES,
                        timer0_left,
                        timer0_right);
        render_interval(selected,
                        timer1,
                        sizeof(timer1) / sizeof(timer1[0]),
                        0,
                        end_cycle,
                        CONTRACT_RENDER_FRAMES,
                        timer1_left,
                        timer1_right);
        render_interval(empty_audio,
                        empty,
                        sizeof(empty) / sizeof(empty[0]),
                        0,
                        end_cycle,
                        CONTRACT_RENDER_FRAMES,
                        empty_left,
                        empty_right);
        ASSERT(is_silent(timer0_left, timer0_right, CONTRACT_RENDER_FRAMES),
               "FIFO A selected for TIMER_1 ignores TIMER_0");
        ASSERT(peak_abs(timer1_left, CONTRACT_RENDER_FRAMES) > 0.001f,
               "FIFO A selected for TIMER_1 becomes audible at TIMER_1");
        ASSERT(is_silent(empty_left, empty_right, CONTRACT_RENDER_FRAMES),
               "repeated selected timer clocks leave an empty FIFO silent");
    }
    hw_audio_destroy(unselected);
    hw_audio_destroy(selected);
    hw_audio_destroy(empty_audio);
}

/* SOUNDBIAS cadence is tested by the rendered response at every DAC interval;
 * this deliberately does not inspect hw_audio_internal_rate or another debug accessor. */
static void test_soundbias_all_dac_intervals_are_render_observable(void)
{
    printf("Testing production chip contracts: all SOUNDBIAS DAC intervals render...\n");
    static const struct
    {
        uint32_t bias;
        uint64_t timer_cycle;
    } fixtures[] = {
        {0x0200u, 5123u},
        {0x4200u, 2561u},
        {0x8200u, 1280u},
        {0xC200u, 640u},
    };

    for (size_t index = 0; index < sizeof(fixtures) / sizeof(fixtures[0]); ++index)
    {
        float left[CONTRACT_RENDER_FRAMES] = {0};
        float right[CONTRACT_RENDER_FRAMES] = {0};
        M4ARegWrite events[] = {
            {0, M4A_REG_NR52, 0x80, 0},
            {0, M4A_REG_SOUNDCNT_H, 0x0304, 0},
            {0, M4A_REG_SOUNDBIAS, fixtures[index].bias, 0},
            {0, M4A_REG_FIFO_A, 0x0000007F, 0},
            {fixtures[index].timer_cycle, M4A_REG_TIMER_0, 0, 0},
        };
        HwAudio* audio = hw_audio_create((float)CONTRACT_HOST_RATE);
        ASSERT(audio != NULL, "HwAudio allocation succeeds");
        if (audio)
        {
            const uint64_t end_cycle = CONTRACT_RENDER_FRAMES * CONTRACT_CYCLES_PER_HOST_FRAME;
            render_interval(
                audio, events, sizeof(events) / sizeof(events[0]), 0, end_cycle, CONTRACT_RENDER_FRAMES, left, right);
            ASSERT(peak_abs(left, CONTRACT_RENDER_FRAMES) > 0.001f,
                   "selected SOUNDBIAS DAC interval produces an observed FIFO response");
        }
        hw_audio_destroy(audio);
    }
}

static void test_soundbias_clipping(void)
{
    printf("Testing production chip contracts: SOUNDBIAS clipping...\n");
    float default_left[CONTRACT_RENDER_FRAMES] = {0};
    float default_right[CONTRACT_RENDER_FRAMES] = {0};
    float zero_left[CONTRACT_RENDER_FRAMES] = {0};
    float zero_right[CONTRACT_RENDER_FRAMES] = {0};
    const uint64_t end_cycle = CONTRACT_RENDER_FRAMES * CONTRACT_CYCLES_PER_HOST_FRAME;
    M4ARegWrite default_bias[] = {
        {0, M4A_REG_NR52, 0x80, 0},
        {0, M4A_REG_SOUNDCNT_H, 0x330C, 0},
        {0, M4A_REG_FIFO_A, 0x0000007F, 0},
        {0, M4A_REG_FIFO_B, 0x0000007F, 0},
        {0, M4A_REG_TIMER_0, 0, 0},
    };
    M4ARegWrite zero_bias[] = {
        {0, M4A_REG_NR52, 0x80, 0},
        {0, M4A_REG_SOUNDCNT_H, 0x330C, 0},
        {0, M4A_REG_SOUNDBIAS, 0x0000, 0},
        {0, M4A_REG_FIFO_A, 0x0000007F, 0},
        {0, M4A_REG_FIFO_B, 0x0000007F, 0},
        {0, M4A_REG_TIMER_0, 0, 0},
    };

    HwAudio* default_audio = hw_audio_create((float)CONTRACT_HOST_RATE);
    HwAudio* zero_audio = hw_audio_create((float)CONTRACT_HOST_RATE);
    ASSERT(default_audio != NULL && zero_audio != NULL, "HwAudio allocations succeed");
    if (default_audio && zero_audio)
    {
        render_interval(default_audio,
                        default_bias,
                        sizeof(default_bias) / sizeof(default_bias[0]),
                        0,
                        end_cycle,
                        CONTRACT_RENDER_FRAMES,
                        default_left,
                        default_right);
        render_interval(zero_audio,
                        zero_bias,
                        sizeof(zero_bias) / sizeof(zero_bias[0]),
                        0,
                        end_cycle,
                        CONTRACT_RENDER_FRAMES,
                        zero_left,
                        zero_right);
        ASSERT(peak_abs(default_left, CONTRACT_RENDER_FRAMES) > 0.001f,
               "summed DirectSound reaches the default SOUNDBIAS clipping path");
        ASSERT(largest_difference(default_left, zero_left, CONTRACT_RENDER_FRAMES) > 1e-4f,
               "SOUNDBIAS changes the observable DAC clip window");
    }
    hw_audio_destroy(default_audio);
    hw_audio_destroy(zero_audio);
}
static void test_noise_trigger_resets_rendered_clock(void)
{
    printf("Testing production chip contracts: NR44 trigger resets rendered noise clock...\\n");
    float retriggered_left[CONTRACT_RENDER_FRAMES] = {0};
    float retriggered_right[CONTRACT_RENDER_FRAMES] = {0};
    float continuous_left[CONTRACT_RENDER_FRAMES] = {0};
    float continuous_right[CONTRACT_RENDER_FRAMES] = {0};
    const uint64_t end_cycle = CONTRACT_RENDER_FRAMES * CONTRACT_CYCLES_PER_HOST_FRAME;
    M4ARegWrite retriggered[10];
    M4ARegWrite continuous[10];
    size_t retriggered_count = append_noise_setup(retriggered, 0);
    size_t continuous_count = append_noise_setup(continuous, 0);
    retriggered[retriggered_count++] = (M4ARegWrite){512, M4A_REG_NR44, 0x80, 0};

    HwAudio* reset_clock = hw_audio_create((float)CONTRACT_HOST_RATE);
    HwAudio* free_running = hw_audio_create((float)CONTRACT_HOST_RATE);
    ASSERT(reset_clock != NULL && free_running != NULL, "HwAudio allocations succeed");
    if (reset_clock && free_running)
    {
        render_interval(reset_clock,
                        retriggered,
                        retriggered_count,
                        0,
                        end_cycle,
                        CONTRACT_RENDER_FRAMES,
                        retriggered_left,
                        retriggered_right);
        render_interval(free_running,
                        continuous,
                        continuous_count,
                        0,
                        end_cycle,
                        CONTRACT_RENDER_FRAMES,
                        continuous_left,
                        continuous_right);
        ASSERT(peak_abs(retriggered_left, CONTRACT_RENDER_FRAMES) > 0.001f,
               "NR44 trigger produces audible rendered noise");
        ASSERT(largest_difference(retriggered_left, continuous_left, CONTRACT_RENDER_FRAMES) > 1e-4f,
               "NR44 trigger resets the rendered noise feedback clock");
    }
    hw_audio_destroy(reset_clock);
    hw_audio_destroy(free_running);
}

static void test_timer_does_not_partition_noise_clock(void)
{
    printf("Testing production chip contracts: timer does not partition noise clock...\n");
    float with_timer_left[CONTRACT_RENDER_FRAMES] = {0};
    float with_timer_right[CONTRACT_RENDER_FRAMES] = {0};
    float without_timer_left[CONTRACT_RENDER_FRAMES] = {0};
    float without_timer_right[CONTRACT_RENDER_FRAMES] = {0};
    const uint64_t end_cycle = CONTRACT_RENDER_FRAMES * CONTRACT_CYCLES_PER_HOST_FRAME;
    M4ARegWrite with_timer[10];
    M4ARegWrite without_timer[10];
    size_t with_timer_count = append_noise_setup(with_timer, 0);
    size_t without_timer_count = append_noise_setup(without_timer, 0);
    with_timer[with_timer_count++] = (M4ARegWrite){320, M4A_REG_TIMER_0, 0, 0};

    HwAudio* timer_audio = hw_audio_create((float)CONTRACT_HOST_RATE);
    HwAudio* plain_audio = hw_audio_create((float)CONTRACT_HOST_RATE);
    ASSERT(timer_audio != NULL && plain_audio != NULL, "HwAudio allocations succeed");
    if (timer_audio && plain_audio)
    {
        render_interval(timer_audio,
                        with_timer,
                        with_timer_count,
                        0,
                        end_cycle,
                        CONTRACT_RENDER_FRAMES,
                        with_timer_left,
                        with_timer_right);
        render_interval(plain_audio,
                        without_timer,
                        without_timer_count,
                        0,
                        end_cycle,
                        CONTRACT_RENDER_FRAMES,
                        without_timer_left,
                        without_timer_right);
        ASSERT(peak_abs(with_timer_left, CONTRACT_RENDER_FRAMES) > 0.001f,
               "noise trigger produces audible production output");
        ASSERT(largest_difference(with_timer_left, without_timer_left, CONTRACT_RENDER_FRAMES) < 1e-4f,
               "DirectSound TIMER observation does not partition the noise clock");
    }
    hw_audio_destroy(timer_audio);
    hw_audio_destroy(plain_audio);
}

void test_hw_audio_contracts_run_all(void)
{
    test_same_cycle_fifo_write_precedes_timer();
    test_mid_sample_phase_tracks_cycle_delta();
    test_nr52_frame_cadence_remains_absolute();
    test_square_latched_samples_refresh_at_hardware_edges();
    test_wave_ram_bank_and_trigger_delay();
    test_delayed_frame_callback_clocks_wave_once();
    test_wave_nr30_selects_bank_before_clock();
    test_wave_ram_write_forces_pending_clock();
    test_wave_nr52_disable_preserves_residual_ram_phase();
    test_wave_length_expiry_gates_sparse_delta();
    test_wave_repeated_nr52_off_clears_bank();
    test_fifo_stereo_little_endian();
    test_fifo_reset_preserves_current_word();
    test_timer_selection_and_empty_fifo_silence();
    test_soundbias_all_dac_intervals_are_render_observable();
    test_soundbias_clipping();
    test_noise_trigger_resets_rendered_clock();
    test_timer_does_not_partition_noise_clock();
}
