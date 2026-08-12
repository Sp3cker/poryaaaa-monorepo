#include "hw_audio/hw_audio_trace.h"
#include "hw_audio/hw_audio_trace_text.h"
#include "test_assert.h"

#include <limits.h>
#include <stdio.h>

static HwAudioTraceStatus apply_event(HwAudio* audio,
                                      uint64_t cycle,
                                      uint32_t order,
                                      HwAudioTraceEventKind kind,
                                      uint8_t width,
                                      uint32_t address,
                                      uint32_t value,
                                      HwAudioNativeFrame* frame)
{
    HwAudioTraceEvent event = {
        .cycle = cycle,
        .order = order,
        .kind = kind,
        .width = width,
        .address = address,
        .value = value,
    };
    return hw_audio_trace_apply(audio, &event, frame);
}

static void configure_directsound(HwAudio* audio, uint16_t soundcnt_h)
{
    HwAudioNativeFrame ignored;
    ASSERT_EQ(apply_event(audio, 0, 0, HW_AUDIO_TRACE_WRITE, 2, HW_AUDIO_GBA_IO_BASE + 0x84, 0x0080, &ignored),
              HW_AUDIO_TRACE_OK,
              "DirectSound master-enable write is accepted");
    ASSERT_EQ(apply_event(audio, 0, 1, HW_AUDIO_TRACE_WRITE, 2, HW_AUDIO_GBA_IO_BASE + 0x82, soundcnt_h, &ignored),
              HW_AUDIO_TRACE_OK,
              "DirectSound routing write is accepted");
}

/* SAMPLE is an ordered observation: a same-cycle DMA write affects only later samples. */
static void test_trace_sample_and_write_same_cycle_order(void)
{
    printf("Testing hw_audio trace contracts: SAMPLE/write order...\n");
    HwAudio* audio = hw_audio_create(48000.0f);
    HwAudioNativeFrame frame;
    hw_audio_trace_reset(audio);
    configure_directsound(audio, 0x0304);

    ASSERT_EQ(apply_event(audio, 10, 0, HW_AUDIO_TRACE_SAMPLE, 0, 0, 0, &frame),
              HW_AUDIO_TRACE_OK,
              "SAMPLE before its same-cycle DMA write is accepted");
    ASSERT_EQ(frame.left, 0, "SAMPLE before DMA write observes the old FIFO hold on left");
    ASSERT_EQ(frame.right, 0, "SAMPLE before DMA write observes the old FIFO hold on right");

    ASSERT_EQ(apply_event(audio, 10, 1, HW_AUDIO_TRACE_WRITE, 4, HW_AUDIO_GBA_IO_BASE + 0xA0, 0x0000007F, &frame),
              HW_AUDIO_TRACE_OK,
              "same-cycle DMA write is accepted after SAMPLE");
    ASSERT_EQ(apply_event(audio, 10, 2, HW_AUDIO_TRACE_TIMER, 0, 0, 0, &frame),
              HW_AUDIO_TRACE_OK,
              "selected timer drains the later FIFO write");
    ASSERT_EQ(apply_event(audio, 10, 3, HW_AUDIO_TRACE_SAMPLE, 0, 0, 0, &frame),
              HW_AUDIO_TRACE_OK,
              "SAMPLE after same-cycle write and timer is accepted");
    ASSERT_EQ(frame.left, 24384, "write before SAMPLE is audible on routed left output");
    ASSERT_EQ(frame.right, 24384, "write before SAMPLE is audible on routed right output");
    hw_audio_destroy(audio);
}

/* A frequency write between native SAMPLE cycles must see only the elapsed
 * hardware cycles, not a full DAC period applied at the preceding SAMPLE. */
static void test_trace_mid_sample_write_uses_exact_cycle_phase(void)
{
    printf("Testing hw_audio trace contracts: exact mid-sample write phase...\n");
    HwAudio* audio = hw_audio_create(48000.0f);
    HwAudioNativeFrame frame;
    hw_audio_trace_reset(audio);

    ASSERT_EQ(apply_event(audio, 0, 0, HW_AUDIO_TRACE_WRITE, 2, HW_AUDIO_GBA_IO_BASE + 0x80, 0x1177, &frame),
              HW_AUDIO_TRACE_OK,
              "SOUNDCNT_L routes square 1 to both sides");
    ASSERT_EQ(apply_event(audio, 0, 1, HW_AUDIO_TRACE_WRITE, 2, HW_AUDIO_GBA_IO_BASE + 0x82, 0x0002, &frame),
              HW_AUDIO_TRACE_OK,
              "SOUNDCNT_H sets full PSG volume");
    ASSERT_EQ(apply_event(audio, 0, 2, HW_AUDIO_TRACE_WRITE, 2, HW_AUDIO_GBA_IO_BASE + 0x84, 0x0080, &frame),
              HW_AUDIO_TRACE_OK,
              "SOUNDCNT_X enables PSG");
    ASSERT_EQ(apply_event(audio, 0, 3, HW_AUDIO_TRACE_WRITE, 1, HW_AUDIO_GBA_IO_BASE + 0x62, 0x80, &frame),
              HW_AUDIO_TRACE_OK,
              "SOUND1CNT_H low byte sets the 50 percent duty");
    ASSERT_EQ(apply_event(audio, 0, 4, HW_AUDIO_TRACE_WRITE, 1, HW_AUDIO_GBA_IO_BASE + 0x63, 0xF0, &frame),
              HW_AUDIO_TRACE_OK,
              "SOUND1CNT_H high byte sets full envelope volume");
    ASSERT_EQ(apply_event(audio, 0, 5, HW_AUDIO_TRACE_WRITE, 1, HW_AUDIO_GBA_IO_BASE + 0x64, 0xF8, &frame),
              HW_AUDIO_TRACE_OK,
              "SOUND1CNT_X low byte sets the initial 128-cycle period");
    ASSERT_EQ(apply_event(audio, 0, 6, HW_AUDIO_TRACE_WRITE, 1, HW_AUDIO_GBA_IO_BASE + 0x65, 0x87, &frame),
              HW_AUDIO_TRACE_OK,
              "SOUND1CNT_X high-byte trigger starts square 1");
    ASSERT_EQ(apply_event(audio, 0, 7, HW_AUDIO_TRACE_SAMPLE, 0, 0, 0, &frame),
              HW_AUDIO_TRACE_OK,
              "initial native sample is accepted");
    ASSERT_EQ(frame.left, 11520, "square starts on the duty pattern's high phase");

    ASSERT_EQ(apply_event(audio, 256, 0, HW_AUDIO_TRACE_WRITE, 1, HW_AUDIO_GBA_IO_BASE + 0x64, 0xFC, &frame),
              HW_AUDIO_TRACE_OK,
              "mid-period frequency write is accepted");
    ASSERT_EQ(apply_event(audio, 512, 0, HW_AUDIO_TRACE_SAMPLE, 0, 0, 0, &frame),
              HW_AUDIO_TRACE_OK,
              "next native sample is accepted");
    ASSERT_EQ(frame.left, 11520, "the 256 old-rate and 256 new-rate cycles reach duty phase six");
    ASSERT_EQ(frame.right, 11520, "the exact-cycle phase remains centered");
    hw_audio_destroy(audio);
}

/* NR52 power-off preserves mGBA's absolute square-clock phase for the next channel write. */
static void test_trace_square_phase_spans_nr52_power_off(void)
{
    printf("Testing hw_audio trace contracts: NR52 square phase...\n");
    HwAudio* audio = hw_audio_create(48000.0f);
    HwAudioNativeFrame frame;
    hw_audio_trace_reset(audio);
    ASSERT_EQ(apply_event(audio, 0, 0, HW_AUDIO_TRACE_WRITE, 2, HW_AUDIO_GBA_IO_BASE + 0x84, 0x0080, &frame),
              HW_AUDIO_TRACE_OK,
              "SOUNDCNT_X initially enables PSG");
    ASSERT_EQ(apply_event(audio, 8220, 0, HW_AUDIO_TRACE_WRITE, 2, HW_AUDIO_GBA_IO_BASE + 0x84, 0x0000, &frame),
              HW_AUDIO_TRACE_OK,
              "SOUNDCNT_X powers PSG off after its square clock has advanced");

    ASSERT_EQ(apply_event(audio, 8220, 1, HW_AUDIO_TRACE_WRITE, 2, HW_AUDIO_GBA_IO_BASE + 0x80, 0x1177, &frame),
              HW_AUDIO_TRACE_OK,
              "SOUNDCNT_L routing is accepted while PSG is powered off");
    ASSERT_EQ(apply_event(audio, 8220, 2, HW_AUDIO_TRACE_WRITE, 2, HW_AUDIO_GBA_IO_BASE + 0x82, 0x0002, &frame),
              HW_AUDIO_TRACE_OK,
              "SOUNDCNT_H full PSG volume is accepted");
    ASSERT_EQ(apply_event(audio, 40000, 0, HW_AUDIO_TRACE_WRITE, 2, HW_AUDIO_GBA_IO_BASE + 0x84, 0x0080, &frame),
              HW_AUDIO_TRACE_OK,
              "SOUNDCNT_X re-enables PSG after an off interval");

    ASSERT_EQ(apply_event(audio, 40000, 1, HW_AUDIO_TRACE_WRITE, 1, HW_AUDIO_GBA_IO_BASE + 0x62, 0x80, &frame),
              HW_AUDIO_TRACE_OK,
              "same-cycle SOUND1CNT_H write consumes the preserved square clock");
    ASSERT_EQ(apply_event(audio, 40000, 2, HW_AUDIO_TRACE_WRITE, 1, HW_AUDIO_GBA_IO_BASE + 0x63, 0xF0, &frame),
              HW_AUDIO_TRACE_OK,
              "SOUND1CNT_H loads full envelope volume");
    ASSERT_EQ(apply_event(audio, 40000, 3, HW_AUDIO_TRACE_WRITE, 1, HW_AUDIO_GBA_IO_BASE + 0x64, 0x00, &frame),
              HW_AUDIO_TRACE_OK,
              "SOUND1CNT_X selects the default square period");
    ASSERT_EQ(apply_event(audio, 40000, 4, HW_AUDIO_TRACE_WRITE, 1, HW_AUDIO_GBA_IO_BASE + 0x65, 0x80, &frame),
              HW_AUDIO_TRACE_OK,
              "SOUND1CNT_X triggers square 1");
    ASSERT_EQ(apply_event(audio, 40000, 5, HW_AUDIO_TRACE_SAMPLE, 0, 0, 0, &frame),
              HW_AUDIO_TRACE_OK,
              "sample at the trigger cycle is accepted");
    ASSERT_EQ(frame.left, 0, "absolute mGBA square phase starts on duty index one");
    ASSERT_EQ(frame.right, 0, "absolute square phase is identical on both routed sides");

    ASSERT_EQ(apply_event(audio, 163840, 0, HW_AUDIO_TRACE_SAMPLE, 0, 0, 0, &frame),
              HW_AUDIO_TRACE_OK,
              "later square phase sample is accepted");
    ASSERT_EQ(frame.left, 11520, "absolute mGBA square phase reaches duty index five");
    ASSERT_EQ(frame.right, 11520, "later square phase remains centered");
    hw_audio_destroy(audio);
}

/* Pinned mGBA GBA-mode noise resets both feedback and clock origin on NR44;
 * these observations include the matrix's first differing noise sample. */
static void test_trace_noise_trigger_feedback_and_clock_origin(void)
{
    printf("Testing hw_audio trace contracts: noise trigger feedback and clock origin...\n");
    HwAudio* audio = hw_audio_create(48000.0f);
    HwAudioNativeFrame frame;
    hw_audio_trace_reset(audio);

    ASSERT_EQ(apply_event(audio, 0, 0, HW_AUDIO_TRACE_WRITE, 2, HW_AUDIO_GBA_IO_BASE + 0x80, 0x8877, &frame),
              HW_AUDIO_TRACE_OK,
              "SOUNDCNT_L routes noise to both sides at full master volume");
    ASSERT_EQ(apply_event(audio, 0, 1, HW_AUDIO_TRACE_WRITE, 2, HW_AUDIO_GBA_IO_BASE + 0x82, 0x0002, &frame),
              HW_AUDIO_TRACE_OK,
              "SOUNDCNT_H selects the pinned PSG gain");
    ASSERT_EQ(apply_event(audio, 0, 2, HW_AUDIO_TRACE_WRITE, 2, HW_AUDIO_GBA_IO_BASE + 0x84, 0x0080, &frame),
              HW_AUDIO_TRACE_OK,
              "SOUNDCNT_X enables PSG");
    ASSERT_EQ(apply_event(audio, 0, 3, HW_AUDIO_TRACE_WRITE, 1, HW_AUDIO_GBA_IO_BASE + 0x79, 0xF1, &frame),
              HW_AUDIO_TRACE_OK,
              "SOUND4CNT_L loads volume-15 noise envelope");
    ASSERT_EQ(apply_event(audio, 0, 4, HW_AUDIO_TRACE_WRITE, 1, HW_AUDIO_GBA_IO_BASE + 0x7C, 0x00, &frame),
              HW_AUDIO_TRACE_OK,
              "SOUND4CNT_H selects the 32-cycle noise period");
    ASSERT_EQ(apply_event(audio, 0, 5, HW_AUDIO_TRACE_WRITE, 1, HW_AUDIO_GBA_IO_BASE + 0x7D, 0x80, &frame),
              HW_AUDIO_TRACE_OK,
              "initial SOUND4CNT_X trigger is accepted");
    ASSERT_EQ(apply_event(audio, 19, 0, HW_AUDIO_TRACE_WRITE, 1, HW_AUDIO_GBA_IO_BASE + 0x7D, 0x80, &frame),
              HW_AUDIO_TRACE_OK,
              "retrigger before the first noise clock is accepted");
    ASSERT_EQ(apply_event(audio, 44, 0, HW_AUDIO_TRACE_SAMPLE, 0, 0, 0, &frame),
              HW_AUDIO_TRACE_OK,
              "sample before the retriggered clock is accepted");
    ASSERT_EQ(frame.left, 0, "NR44 reset prevents a stale pre-trigger noise clock on left");
    ASSERT_EQ(frame.right, 0, "NR44 reset prevents a stale pre-trigger noise clock on right");

    ASSERT_EQ(apply_event(audio, 607323, 0, HW_AUDIO_TRACE_WRITE, 1, HW_AUDIO_GBA_IO_BASE + 0x7D, 0x80, &frame),
              HW_AUDIO_TRACE_OK,
              "matrix noise trigger at cycle 607323 is accepted");
    ASSERT_EQ(apply_event(audio, 607744, 0, HW_AUDIO_TRACE_SAMPLE, 0, 0, 0, &frame),
              HW_AUDIO_TRACE_OK,
              "matrix noise sample before the first mismatch is accepted");
    ASSERT_EQ(frame.left, 11520, "thirteen post-trigger mGBA feedback clocks produce left PCM16 11520");
    ASSERT_EQ(frame.right, 11520, "thirteen post-trigger mGBA feedback clocks produce right PCM16 11520");
    ASSERT_EQ(apply_event(audio, 608000, 0, HW_AUDIO_TRACE_SAMPLE, 0, 0, 0, &frame),
              HW_AUDIO_TRACE_OK,
              "matrix first-difference noise sample is accepted");
    ASSERT_EQ(frame.left, 11520, "twenty-one GBA-feedback clocks reproduce the pinned left PCM16 value");
    ASSERT_EQ(frame.right, 11520, "twenty-one GBA-feedback clocks reproduce the pinned right PCM16 value");
    hw_audio_destroy(audio);
}

/* The frame event stays reset-time aligned while NR52 is off, so an enabled
 * envelope observes the absolute step-seven event rather than a rebased one. */
static void test_trace_frame_envelope_uses_absolute_nr52_cadence(void)
{
    printf("Testing hw_audio trace contracts: absolute NR52 frame cadence...\n");
    HwAudio* audio = hw_audio_create(48000.0f);
    HwAudioNativeFrame frame;
    hw_audio_trace_reset(audio);

    ASSERT_EQ(apply_event(audio, 6483, 0, HW_AUDIO_TRACE_WRITE, 2, HW_AUDIO_GBA_IO_BASE + 0x84, 0x0080, &frame),
              HW_AUDIO_TRACE_OK,
              "SOUNDCNT_X enables after a non-frame-aligned off interval");
    ASSERT_EQ(apply_event(audio, 6483, 1, HW_AUDIO_TRACE_WRITE, 2, HW_AUDIO_GBA_IO_BASE + 0x80, 0x2277, &frame),
              HW_AUDIO_TRACE_OK,
              "SOUNDCNT_L routes square 2 to both sides");
    ASSERT_EQ(apply_event(audio, 6483, 2, HW_AUDIO_TRACE_WRITE, 2, HW_AUDIO_GBA_IO_BASE + 0x82, 0x0002, &frame),
              HW_AUDIO_TRACE_OK,
              "SOUNDCNT_H selects full PSG gain");
    ASSERT_EQ(apply_event(audio, 6483, 3, HW_AUDIO_TRACE_WRITE, 1, HW_AUDIO_GBA_IO_BASE + 0x68, 0x80, &frame),
              HW_AUDIO_TRACE_OK,
              "SOUND2CNT_L selects a high 50-percent duty phase");
    ASSERT_EQ(apply_event(audio, 6483, 4, HW_AUDIO_TRACE_WRITE, 1, HW_AUDIO_GBA_IO_BASE + 0x69, 0x21, &frame),
              HW_AUDIO_TRACE_OK,
              "SOUND2CNT_H loads a decreasing pace-one envelope");
    ASSERT_EQ(apply_event(audio, 6483, 5, HW_AUDIO_TRACE_WRITE, 1, HW_AUDIO_GBA_IO_BASE + 0x6C, 0x00, &frame),
              HW_AUDIO_TRACE_OK,
              "SOUND2CNT_X low byte selects the 32,768-cycle square period");
    ASSERT_EQ(apply_event(audio, 6483, 6, HW_AUDIO_TRACE_WRITE, 1, HW_AUDIO_GBA_IO_BASE + 0x6D, 0x80, &frame),
              HW_AUDIO_TRACE_OK,
              "SOUND2CNT_X trigger arms square 2");
    ASSERT_EQ(apply_event(audio, 261888, 0, HW_AUDIO_TRACE_SAMPLE, 0, 0, 0, &frame),
              HW_AUDIO_TRACE_OK,
              "sample immediately before the absolute step-seven event is accepted");
    ASSERT_EQ(frame.left, 1536, "pre-event pace-one envelope volume two is audible on left");
    ASSERT_EQ(frame.right, 1536, "pre-event pace-one envelope volume two is audible on right");
    ASSERT_EQ(apply_event(audio, 262400, 0, HW_AUDIO_TRACE_SAMPLE, 0, 0, 0, &frame),
              HW_AUDIO_TRACE_OK,
              "sample immediately after the absolute step-seven event is accepted");
    ASSERT_EQ(frame.left, 768, "absolute step seven decrements the envelope on left without NR52 delay");
    ASSERT_EQ(frame.right, 768, "absolute step seven decrements the envelope on right without NR52 delay");
    hw_audio_destroy(audio);
}

/* No-ROM replay enables NR52 before mGBA's scheduled cycle-zero callback.
 * Preserve that callback so later envelope boundaries retain the full-core epoch. */
static void test_trace_cycle_zero_frame_event_matches_mgba_epoch(void)
{
    printf("Testing hw_audio trace contracts: cycle-zero mGBA frame event...\n");
    HwAudio* audio = hw_audio_create(48000.0f);
    HwAudioNativeFrame frame;
    hw_audio_trace_reset(audio);

    ASSERT_EQ(apply_event(audio, 0, 0, HW_AUDIO_TRACE_WRITE, 2, HW_AUDIO_GBA_IO_BASE + 0x84, 0x0080, &frame),
              HW_AUDIO_TRACE_OK,
              "cycle-zero SOUNDCNT_X enables PSG");
    ASSERT_EQ(apply_event(audio, 0, 1, HW_AUDIO_TRACE_WRITE, 2, HW_AUDIO_GBA_IO_BASE + 0x80, 0x0077, &frame),
              HW_AUDIO_TRACE_OK,
              "cycle-zero SOUNDCNT_L setup is accepted");
    ASSERT_EQ(apply_event(audio, 0, 2, HW_AUDIO_TRACE_WRITE, 2, HW_AUDIO_GBA_IO_BASE + 0x82, 0x0002, &frame),
              HW_AUDIO_TRACE_OK,
              "cycle-zero SOUNDCNT_H setup is accepted");
    ASSERT_EQ(apply_event(audio, 280896, 0, HW_AUDIO_TRACE_WRITE, 1, HW_AUDIO_GBA_IO_BASE + 0x68, 0x80, &frame),
              HW_AUDIO_TRACE_OK,
              "Sq2 duty transaction is accepted at the lifecycle boundary");
    ASSERT_EQ(apply_event(audio, 280896, 1, HW_AUDIO_TRACE_WRITE, 1, HW_AUDIO_GBA_IO_BASE + 0x6C, 0x0B, &frame),
              HW_AUDIO_TRACE_OK,
              "Sq2 frequency low byte is accepted");
    ASSERT_EQ(apply_event(audio, 280896, 2, HW_AUDIO_TRACE_WRITE, 1, HW_AUDIO_GBA_IO_BASE + 0x6D, 0x06, &frame),
              HW_AUDIO_TRACE_OK,
              "Sq2 frequency high byte is accepted");
    ASSERT_EQ(apply_event(audio, 280896, 3, HW_AUDIO_TRACE_WRITE, 2, HW_AUDIO_GBA_IO_BASE + 0x80, 0x2277, &frame),
              HW_AUDIO_TRACE_OK,
              "Sq2 stereo routing is accepted");
    ASSERT_EQ(apply_event(audio, 280896, 4, HW_AUDIO_TRACE_WRITE, 1, HW_AUDIO_GBA_IO_BASE + 0x69, 0xF1, &frame),
              HW_AUDIO_TRACE_OK,
              "Sq2 decreasing envelope is accepted");
    ASSERT_EQ(apply_event(audio, 280896, 5, HW_AUDIO_TRACE_WRITE, 1, HW_AUDIO_GBA_IO_BASE + 0x6D, 0x86, &frame),
              HW_AUDIO_TRACE_OK,
              "Sq2 trigger is accepted");
    ASSERT_EQ(apply_event(audio, 495104, 0, HW_AUDIO_TRACE_SAMPLE, 0, 0, 0, &frame),
              HW_AUDIO_TRACE_OK,
              "post-envelope native sample is accepted");
    ASSERT_EQ(frame.left, 10752, "cycle-zero callback places Sq2 at mGBA's volume-fourteen boundary");
    ASSERT_EQ(frame.right, 10752, "cycle-zero frame epoch remains centered");
    hw_audio_destroy(audio);
}

/* The four SOUNDBIAS sampling-cycle encodings select all native DAC intervals. */
static void test_trace_soundbias_selects_all_dac_intervals(void)
{
    printf("Testing hw_audio trace contracts: SOUNDBIAS intervals...\n");
    static const struct
    {
        uint16_t soundbias;
        int rate;
        int cycles_per_sample;
    } fixtures[] = {
        {0x0200, 32768, 512},
        {0x4200, 65536, 256},
        {0x8200, 131072, 128},
        {0xC200, 262144, 64},
    };
    HwAudio* audio = hw_audio_create(48000.0f);
    HwAudioNativeFrame frame;
    hw_audio_trace_reset(audio);

    for (size_t index = 0; index < sizeof(fixtures) / sizeof(fixtures[0]); index++)
    {
        ASSERT_EQ(apply_event(audio,
                              index,
                              0,
                              HW_AUDIO_TRACE_WRITE,
                              2,
                              HW_AUDIO_GBA_IO_BASE + 0x88,
                              fixtures[index].soundbias,
                              &frame),
                  HW_AUDIO_TRACE_OK,
                  "SOUNDBIAS write is accepted");
        ASSERT_EQ(hw_audio_internal_rate(audio), fixtures[index].rate, "SOUNDBIAS selects native DAC rate");
        ASSERT_EQ(HW_AUDIO_GBA_CLOCK_HZ / hw_audio_internal_rate(audio),
                  fixtures[index].cycles_per_sample,
                  "SOUNDBIAS selects GBA-cycle interval");
    }
    hw_audio_destroy(audio);
}

/* FIFO words are little-endian independently for A and B, including their stereo routes. */
static void test_trace_fifo_stereo_little_endian(void)
{
    printf("Testing hw_audio trace contracts: FIFO A/B stereo byte order...\n");
    static const int16_t expected_left[] = {-24576, -384, 192, 24384};
    static const int16_t expected_right[] = {24192, 576, 384, -768};
    HwAudio* audio = hw_audio_create(48000.0f);
    HwAudioNativeFrame frame;
    hw_audio_trace_reset(audio);
    configure_directsound(audio, 0x120C);

    ASSERT_EQ(apply_event(audio, 1, 0, HW_AUDIO_TRACE_WRITE, 4, HW_AUDIO_GBA_IO_BASE + 0xA0, 0x7F01FE80, &frame),
              HW_AUDIO_TRACE_OK,
              "FIFO A accepts its four-byte fixture word");
    ASSERT_EQ(apply_event(audio, 1, 1, HW_AUDIO_TRACE_WRITE, 4, HW_AUDIO_GBA_IO_BASE + 0xA4, 0xFC02037E, &frame),
              HW_AUDIO_TRACE_OK,
              "FIFO B accepts its four-byte fixture word");

    for (uint32_t index = 0; index < 4; index++)
    {
        uint64_t cycle = 2u + index;
        ASSERT_EQ(apply_event(audio, cycle, 0, HW_AUDIO_TRACE_TIMER, 0, 0, 0, &frame),
                  HW_AUDIO_TRACE_OK,
                  "shared timer clocks both FIFOs");
        ASSERT_EQ(apply_event(audio, cycle, 1, HW_AUDIO_TRACE_SAMPLE, 0, 0, 0, &frame),
                  HW_AUDIO_TRACE_OK,
                  "sample after FIFO clock is accepted");
        ASSERT_EQ(frame.left, expected_left[index], "FIFO A byte order and left-only route are exact");
        ASSERT_EQ(frame.right, expected_right[index], "FIFO B byte order and right-only route are exact");
    }
    hw_audio_destroy(audio);
}

/* A timer late in one native interval updates that interval's mGBA FIFO slot. */
static void test_trace_fifo_schedule_resolves_current_sample_block(void)
{
    printf("Testing hw_audio trace contracts: mGBA FIFO sample blocks...\n");
    static const HwAudioTraceEvent events[] = {
        {0, 0, HW_AUDIO_TRACE_WRITE, 2, HW_AUDIO_GBA_IO_BASE + 0x88, 0x4200},
        {0, 1, HW_AUDIO_TRACE_WRITE, 2, HW_AUDIO_GBA_IO_BASE + 0x84, 0x0080},
        {0, 2, HW_AUDIO_TRACE_WRITE, 2, HW_AUDIO_GBA_IO_BASE + 0x82, 0x2008},
        {0, 3, HW_AUDIO_TRACE_WRITE, 4, HW_AUDIO_GBA_IO_BASE + 0xA4, 0x00000500},
        {0, 4, HW_AUDIO_TRACE_SAMPLE, 0, 0, 0},
        {100, 0, HW_AUDIO_TRACE_TIMER, 0, 0, 0},
        {256, 0, HW_AUDIO_TRACE_SAMPLE, 0, 0, 0},
        {512, 0, HW_AUDIO_TRACE_SAMPLE, 0, 0, 0},
        {768, 0, HW_AUDIO_TRACE_SAMPLE, 0, 0, 0},
        {900, 0, HW_AUDIO_TRACE_TIMER, 0, 0, 0},
        {1024, 0, HW_AUDIO_TRACE_SAMPLE, 0, 0, 0},
        {1280, 0, HW_AUDIO_TRACE_SAMPLE, 0, 0, 0},
        {1536, 0, HW_AUDIO_TRACE_SAMPLE, 0, 0, 0},
        {1792, 0, HW_AUDIO_TRACE_SAMPLE, 0, 0, 0},
    };
    HwAudioTraceFifoSample fifo_samples[8];
    size_t fifo_sample_count = 0;
    ASSERT_EQ(hw_audio_trace_schedule_fifo_samples(events,
                                                   sizeof(events) / sizeof(events[0]),
                                                   fifo_samples,
                                                   sizeof(fifo_samples) / sizeof(fifo_samples[0]),
                                                   &fifo_sample_count),
              HW_AUDIO_TRACE_OK,
              "FIFO sample block schedule is accepted");
    ASSERT_EQ(fifo_sample_count, 8, "every explicit SAMPLE receives one FIFO slot");
    ASSERT_EQ(fifo_samples[3].fifo_b, 5, "late timer updates the final slot of its current block");
    ASSERT_EQ(fifo_samples[4].fifo_b, 5, "block rollover carries the final FIFO slot forward");

    HwAudio* audio = hw_audio_create(48000.0f);
    HwAudioNativeFrame frame;
    hw_audio_trace_reset(audio);
    size_t sample_index = 0;
    for (size_t event_index = 0; event_index < sizeof(events) / sizeof(events[0]); event_index++)
    {
        const HwAudioTraceEvent* event = &events[event_index];
        HwAudioTraceStatus status =
            event->kind == HW_AUDIO_TRACE_SAMPLE
                ? hw_audio_trace_apply_fifo_sample(audio, event, &fifo_samples[sample_index++], &frame)
                : hw_audio_trace_apply(audio, event, &frame);
        ASSERT_EQ(status, HW_AUDIO_TRACE_OK, "scheduled FIFO replay event is accepted");
        if (event->kind == HW_AUDIO_TRACE_SAMPLE && event->cycle == 768)
        {
            ASSERT_EQ(frame.left, 960, "current-block FIFO byte reaches the routed native frame");
            ASSERT_EQ(frame.right, 0, "FIFO B remains left-only");
        }
    }
    hw_audio_destroy(audio);
}

/* An empty selected timer carries the final FIFO hold across its block suffix. */
static void test_trace_fifo_schedule_empty_clock_extends_held_suffix(void)
{
    printf("Testing hw_audio trace contracts: empty FIFO timer suffix...\n");
    static const HwAudioTraceEvent events[] = {
        {0, 0, HW_AUDIO_TRACE_WRITE, 2, HW_AUDIO_GBA_IO_BASE + 0x88, 0x4200},
        {0, 1, HW_AUDIO_TRACE_WRITE, 2, HW_AUDIO_GBA_IO_BASE + 0x84, 0x0080},
        {0, 2, HW_AUDIO_TRACE_WRITE, 2, HW_AUDIO_GBA_IO_BASE + 0x82, 0x0300},
        {0, 3, HW_AUDIO_TRACE_WRITE, 4, HW_AUDIO_GBA_IO_BASE + 0xA0, 0x7E050403},
        {0, 4, HW_AUDIO_TRACE_SAMPLE, 0, 0, 0},
        {256, 0, HW_AUDIO_TRACE_SAMPLE, 0, 0, 0},
        {300, 0, HW_AUDIO_TRACE_TIMER, 0, 0, 0},
        {301, 0, HW_AUDIO_TRACE_TIMER, 0, 0, 0},
        {302, 0, HW_AUDIO_TRACE_TIMER, 0, 0, 0},
        {500, 0, HW_AUDIO_TRACE_TIMER, 0, 0, 0},
        {512, 0, HW_AUDIO_TRACE_SAMPLE, 0, 0, 0},
        {768, 0, HW_AUDIO_TRACE_SAMPLE, 0, 0, 0},
        {800, 0, HW_AUDIO_TRACE_TIMER, 0, 0, 0},
    };
    HwAudioTraceFifoSample fifo_samples[4];
    size_t fifo_sample_count = 0;
    ASSERT_EQ(hw_audio_trace_schedule_fifo_samples(events,
                                                   sizeof(events) / sizeof(events[0]),
                                                   fifo_samples,
                                                   sizeof(fifo_samples) / sizeof(fifo_samples[0]),
                                                   &fifo_sample_count),
              HW_AUDIO_TRACE_OK,
              "FIFO empty-clock suffix schedule is accepted");
    ASSERT_EQ(fifo_sample_count, 4, "every explicit SAMPLE receives one FIFO slot");
    ASSERT_EQ(fifo_samples[1].fifo_a, 126, "last queued FIFO byte fills its earlier block suffix");
    ASSERT_EQ(fifo_samples[2].fifo_a, 126, "last queued FIFO byte reaches the preceding slot");
    ASSERT_EQ(fifo_samples[3].fifo_a, 0, "empty selected timer extends the exhausted FIFO hold");
}

/* Eight unclocked words alias mGBA's modulo-8 FIFO pointers as an empty FIFO. */
static void test_trace_fifo_schedule_full_pointer_alias_extends_held_suffix(void)
{
    printf("Testing hw_audio trace contracts: modulo FIFO pointer alias...\n");
    static const HwAudioTraceEvent events[] = {
        {0, 0, HW_AUDIO_TRACE_WRITE, 2, HW_AUDIO_GBA_IO_BASE + 0x88, 0x4200},
        {0, 1, HW_AUDIO_TRACE_WRITE, 2, HW_AUDIO_GBA_IO_BASE + 0x84, 0x0080},
        {0, 2, HW_AUDIO_TRACE_WRITE, 2, HW_AUDIO_GBA_IO_BASE + 0x82, 0x0300},
        {2294400, 0, HW_AUDIO_TRACE_WRITE, 4, HW_AUDIO_GBA_IO_BASE + 0xA0, 0x0000007F},
        {2294400, 1, HW_AUDIO_TRACE_WRITE, 4, HW_AUDIO_GBA_IO_BASE + 0xA0, 0x00000001},
        {2294400, 2, HW_AUDIO_TRACE_WRITE, 4, HW_AUDIO_GBA_IO_BASE + 0xA0, 0x00000002},
        {2294400, 3, HW_AUDIO_TRACE_WRITE, 4, HW_AUDIO_GBA_IO_BASE + 0xA0, 0x00000003},
        {2294400, 4, HW_AUDIO_TRACE_WRITE, 4, HW_AUDIO_GBA_IO_BASE + 0xA0, 0x00000004},
        {2294400, 5, HW_AUDIO_TRACE_WRITE, 4, HW_AUDIO_GBA_IO_BASE + 0xA0, 0x00000005},
        {2294400, 6, HW_AUDIO_TRACE_WRITE, 4, HW_AUDIO_GBA_IO_BASE + 0xA0, 0x00000006},
        {2294400, 7, HW_AUDIO_TRACE_WRITE, 4, HW_AUDIO_GBA_IO_BASE + 0xA0, 0x00000007},
        {2294485, 0, HW_AUDIO_TRACE_TIMER, 0, 0, 0},
        {2294528, 0, HW_AUDIO_TRACE_SAMPLE, 0, 0, 0},
    };
    HwAudioTraceFifoSample fifo_samples[1];
    size_t fifo_sample_count = 0;
    ASSERT_EQ(hw_audio_trace_schedule_fifo_samples(events,
                                                   sizeof(events) / sizeof(events[0]),
                                                   fifo_samples,
                                                   sizeof(fifo_samples) / sizeof(fifo_samples[0]),
                                                   &fifo_sample_count),
              HW_AUDIO_TRACE_OK,
              "full modulo-pointer FIFO schedule is accepted");
    ASSERT_EQ(fifo_sample_count, 1, "the selected block suffix receives its explicit sample");
    ASSERT_EQ(fifo_samples[0].fifo_a, 0, "aliased FIFO pointer holds the empty suffix instead of consuming word zero");
}

/* The terminal write in a candidate CGB batch closes the pending FIFO bin
 * before a later timer can change the sample that belongs to that batch. */
static void test_trace_fifo_schedule_cgb_batch_finalizes_pending_sample(void)
{
    printf("Testing hw_audio trace contracts: CGB batch sample boundary...\n");
    static const HwAudioTraceEvent events[] = {
        {0, 0, HW_AUDIO_TRACE_WRITE, 2, HW_AUDIO_GBA_IO_BASE + 0x88, 0x4200},
        {0, 1, HW_AUDIO_TRACE_WRITE, 2, HW_AUDIO_GBA_IO_BASE + 0x84, 0x0080},
        {0, 2, HW_AUDIO_TRACE_WRITE, 2, HW_AUDIO_GBA_IO_BASE + 0x82, 0x0300},
        {0, 3, HW_AUDIO_TRACE_WRITE, 4, HW_AUDIO_GBA_IO_BASE + 0xA0, 0x00000005},
        {100, 0, HW_AUDIO_TRACE_TIMER, 0, 0, 0},
        {256, 0, HW_AUDIO_TRACE_SAMPLE, 0, 0, 0},
        {300, 0, HW_AUDIO_TRACE_WRITE, 1, HW_AUDIO_GBA_IO_BASE + 0x65, 0},
        {400, 0, HW_AUDIO_TRACE_TIMER, 0, 0, 0},
    };

    static const HwAudioTraceEvent cgb_batch_events[] = {
        {300, 0, HW_AUDIO_TRACE_WRITE, 1, HW_AUDIO_GBA_IO_BASE + 0x60, 0},
        {300, 1, HW_AUDIO_TRACE_WRITE, 1, HW_AUDIO_GBA_IO_BASE + 0x65, 0},
        {300, 2, HW_AUDIO_TRACE_WRITE, 1, HW_AUDIO_GBA_IO_BASE + 0x68, 0},
        {300, 3, HW_AUDIO_TRACE_WRITE, 1, HW_AUDIO_GBA_IO_BASE + 0x6D, 0},
        {300, 4, HW_AUDIO_TRACE_WRITE, 1, HW_AUDIO_GBA_IO_BASE + 0x70, 0},
        {300, 5, HW_AUDIO_TRACE_WRITE, 1, HW_AUDIO_GBA_IO_BASE + 0x75, 0},
        {300, 6, HW_AUDIO_TRACE_WRITE, 1, HW_AUDIO_GBA_IO_BASE + 0x78, 0},
        {300, 7, HW_AUDIO_TRACE_WRITE, 1, HW_AUDIO_GBA_IO_BASE + 0x7D, 0},
        {300, 8, HW_AUDIO_TRACE_WRITE, 1, HW_AUDIO_GBA_IO_BASE + 0x80, 0},
        {300, 9, HW_AUDIO_TRACE_WRITE, 1, HW_AUDIO_GBA_IO_BASE + 0x81, 0},
        {300, 10, HW_AUDIO_TRACE_WRITE, 4, HW_AUDIO_GBA_IO_BASE + 0x90, 0},
        {300, 11, HW_AUDIO_TRACE_WRITE, 1, HW_AUDIO_GBA_IO_BASE + 0x9F, 0},
    };

    static const HwAudioTraceEvent non_cgb_events[] = {
        {300, 12, HW_AUDIO_TRACE_WRITE, 2, HW_AUDIO_GBA_IO_BASE + 0x82, 0},
        {300, 13, HW_AUDIO_TRACE_WRITE, 1, HW_AUDIO_GBA_IO_BASE + 0x84, 0},
        {300, 14, HW_AUDIO_TRACE_WRITE, 1, HW_AUDIO_GBA_IO_BASE + 0x88, 0},
        {300, 15, HW_AUDIO_TRACE_WRITE, 4, HW_AUDIO_GBA_IO_BASE + 0xA0, 0},
        {300, 16, HW_AUDIO_TRACE_WRITE, 4, HW_AUDIO_GBA_IO_BASE + 0xA4, 0},
        {300, 17, HW_AUDIO_TRACE_TIMER, 0, 0, 0},
    };
    for (size_t index = 0; index < sizeof(cgb_batch_events) / sizeof(cgb_batch_events[0]); index++)
        ASSERT(hw_audio_trace_event_is_cgb_batch_write(&cgb_batch_events[index]),
               "PSG, routing, and Wave RAM writes belong to a candidate CGB batch");
    for (size_t index = 0; index < sizeof(non_cgb_events) / sizeof(non_cgb_events[0]); index++)
        ASSERT(!hw_audio_trace_event_is_cgb_batch_write(&non_cgb_events[index]),
               "DirectSound, power, bias, and TIMER events remain outside candidate CGB batches");
    HwAudioTraceFifoSample fifo_samples[1];
    size_t fifo_sample_count = 0;
    ASSERT_EQ(hw_audio_trace_schedule_fifo_samples(events,
                                                   sizeof(events) / sizeof(events[0]),
                                                   fifo_samples,
                                                   sizeof(fifo_samples) / sizeof(fifo_samples[0]),
                                                   &fifo_sample_count),
              HW_AUDIO_TRACE_OK,
              "CGB-batch sample boundary schedule is accepted");
    ASSERT_EQ(fifo_sample_count, 1, "the pending candidate SAMPLE is finalized");
    ASSERT_EQ(fifo_samples[0].fifo_a, 5, "a later timer cannot rewrite the finalized candidate SAMPLE");
}

/* Candidate SAMPLE records have no explicit callback deadline, so their
 * terminal CGB write is committed before the staged DAC value is observed. */
static void test_trace_staged_sample_observes_terminal_write(void)
{
    printf("Testing hw_audio trace contracts: post-write staged sample...\n");
    static const HwAudioTraceEvent events[] = {
        {0, 0, HW_AUDIO_TRACE_WRITE, 2, HW_AUDIO_GBA_IO_BASE + 0x84, 0x0080},
        {0, 1, HW_AUDIO_TRACE_WRITE, 2, HW_AUDIO_GBA_IO_BASE + 0x80, 0x2277},
        {0, 2, HW_AUDIO_TRACE_WRITE, 1, HW_AUDIO_GBA_IO_BASE + 0x68, 0x80},
        {0, 3, HW_AUDIO_TRACE_WRITE, 1, HW_AUDIO_GBA_IO_BASE + 0x69, 0xF8},
        {0, 4, HW_AUDIO_TRACE_WRITE, 1, HW_AUDIO_GBA_IO_BASE + 0x6C, 0x0B},
        {256, 0, HW_AUDIO_TRACE_SAMPLE, 0, 0, 0},
        {300, 0, HW_AUDIO_TRACE_WRITE, 1, HW_AUDIO_GBA_IO_BASE + 0x6D, 0x86},
        {512, 0, HW_AUDIO_TRACE_SAMPLE, 0, 0, 0},
    };
    HwAudio* audio = hw_audio_create(48000.0f);
    HwAudioNativeFrame frame;
    HwAudioTraceFifoSample fifo_sample = {0};
    hw_audio_trace_reset(audio);

    for (size_t index = 0; index < 5; index++)
        ASSERT_EQ(
            hw_audio_trace_apply(audio, &events[index], &frame), HW_AUDIO_TRACE_OK, "square setup event is accepted");
    ASSERT_EQ(
        hw_audio_trace_stage_sample(audio, &events[5], &frame), HW_AUDIO_TRACE_OK, "candidate square SAMPLE is staged");
    ASSERT(hw_audio_trace_event_is_cgb_batch_write(&events[6]), "NR24 trigger belongs to the candidate square batch");
    ASSERT_EQ(hw_audio_trace_apply(audio, &events[6], &frame),
              HW_AUDIO_TRACE_OK,
              "terminal NR24 mutation is applied before observation");
    ASSERT_EQ(hw_audio_trace_observe_sample(audio, events[5].cycle, &fifo_sample, &frame),
              HW_AUDIO_TRACE_OK,
              "staged candidate SAMPLE is observed after the terminal mutation");
    ASSERT_EQ(frame.cycle, UINT64_C(256), "observed sample keeps its staged cycle");
    ASSERT(frame.left > 0, "post-write square sample is audible on left");
    ASSERT(frame.right > 0, "post-write square sample is audible on right");
    ASSERT_EQ(hw_audio_trace_apply(audio, &events[7], &frame), HW_AUDIO_TRACE_OK, "next square SAMPLE is accepted");
    ASSERT(frame.left > 0, "triggered square remains audible on the next left sample");
    ASSERT(frame.right > 0, "triggered square remains audible on the next right sample");
    hw_audio_destroy(audio);
}

/* Pinned mGBA emits this source-phase shape when GBAAudioSample's mutable
 * lastSample/sampleIndex state starts its next block at cycle 6400. */
static void test_trace_fifo_schedule_accepts_explicit_source_phase(void)
{
    printf("Testing hw_audio trace contracts: explicit mGBA source phase...\n");
    static const HwAudioTraceEvent events[] = {
        {0, UINT32_C(0x80000000), HW_AUDIO_TRACE_SAMPLE, 0, 0, 0},
        {115, UINT32_C(0x80000000), HW_AUDIO_TRACE_WRITE, 2, HW_AUDIO_GBA_IO_BASE + 0x82, 0x0000},
        {115, UINT32_C(0x80010000), HW_AUDIO_TRACE_WRITE, 2, HW_AUDIO_GBA_IO_BASE + 0x84, 0x0000},
        {115, UINT32_C(0x80020000), HW_AUDIO_TRACE_WRITE, 2, HW_AUDIO_GBA_IO_BASE + 0x88, 0x0200},
        {512, UINT32_C(0x80000000), HW_AUDIO_TRACE_SAMPLE, 0, 0, 0},
        {1024, UINT32_C(0x80000000), HW_AUDIO_TRACE_SAMPLE, 0, 0, 0},
        {1536, UINT32_C(0x80000000), HW_AUDIO_TRACE_SAMPLE, 0, 0, 0},
        {2048, UINT32_C(0x80000000), HW_AUDIO_TRACE_SAMPLE, 0, 0, 0},
        {2560, UINT32_C(0x80000000), HW_AUDIO_TRACE_SAMPLE, 0, 0, 0},
        {3072, UINT32_C(0x80000000), HW_AUDIO_TRACE_SAMPLE, 0, 0, 0},
        {3584, UINT32_C(0x80000000), HW_AUDIO_TRACE_SAMPLE, 0, 0, 0},
        {4096, UINT32_C(0x80000000), HW_AUDIO_TRACE_SAMPLE, 0, 0, 0},
        {4608, UINT32_C(0x80000000), HW_AUDIO_TRACE_SAMPLE, 0, 0, 0},
        {5120, UINT32_C(0x80000000), HW_AUDIO_TRACE_SAMPLE, 0, 0, 0},
        {5632, UINT32_C(0x80000000), HW_AUDIO_TRACE_SAMPLE, 0, 0, 0},
        {6400, UINT32_C(0x80000000), HW_AUDIO_TRACE_SAMPLE, 0, 0, 0},
        {6483, UINT32_C(0x80000000), HW_AUDIO_TRACE_WRITE, 2, HW_AUDIO_GBA_IO_BASE + 0x84, 0x008F},
        {6517, UINT32_C(0x80000000), HW_AUDIO_TRACE_WRITE, 2, HW_AUDIO_GBA_IO_BASE + 0x82, 0xA90E},
        {6556, UINT32_C(0x80000000), HW_AUDIO_TRACE_WRITE, 2, HW_AUDIO_GBA_IO_BASE + 0x88, 0x4200},
        {6656, UINT32_C(0x80000000), HW_AUDIO_TRACE_SAMPLE, 0, 0, 0},
        {6912, UINT32_C(0x80000000), HW_AUDIO_TRACE_SAMPLE, 0, 0, 0},
        {7168, UINT32_C(0x80000000), HW_AUDIO_TRACE_SAMPLE, 0, 0, 0},
        {7424, UINT32_C(0x80000000), HW_AUDIO_TRACE_SAMPLE, 0, 0, 0},
    };
    HwAudioTraceFifoSample fifo_samples[17];
    size_t fifo_sample_count = 0;
    ASSERT_EQ(hw_audio_trace_schedule_fifo_samples(events,
                                                   sizeof(events) / sizeof(events[0]),
                                                   fifo_samples,
                                                   sizeof(fifo_samples) / sizeof(fifo_samples[0]),
                                                   &fifo_sample_count),
              HW_AUDIO_TRACE_OK,
              "explicit source-phase SAMPLE events are scheduled");
    ASSERT_EQ(fifo_sample_count, 17, "every source-phase SAMPLE receives a FIFO slot");
}

/* A FIFO advances only on its selected timer and becomes silent after its word drains. */
static void test_trace_timer_selection_and_empty_fifo_silence(void)
{
    printf("Testing hw_audio trace contracts: timer selection and empty silence...\n");
    HwAudio* audio = hw_audio_create(48000.0f);
    HwAudioNativeFrame frame;
    hw_audio_trace_reset(audio);
    configure_directsound(audio, 0x0704);

    ASSERT_EQ(apply_event(audio, 1, 0, HW_AUDIO_TRACE_WRITE, 4, HW_AUDIO_GBA_IO_BASE + 0xA0, 0x00000005, &frame),
              HW_AUDIO_TRACE_OK,
              "FIFO A accepts the hold fixture byte");
    ASSERT_EQ(apply_event(audio, 2, 0, HW_AUDIO_TRACE_TIMER, 0, 0, 0, &frame),
              HW_AUDIO_TRACE_OK,
              "unselected timer is accepted");
    ASSERT_EQ(apply_event(audio, 2, 1, HW_AUDIO_TRACE_SAMPLE, 0, 0, 0, &frame),
              HW_AUDIO_TRACE_OK,
              "sample after unselected timer is accepted");
    ASSERT_EQ(frame.left, 0, "unselected timer leaves FIFO A hold silent");

    ASSERT_EQ(apply_event(audio, 3, 0, HW_AUDIO_TRACE_TIMER, 0, 0, 1, &frame),
              HW_AUDIO_TRACE_OK,
              "selected timer is accepted");
    ASSERT_EQ(apply_event(audio, 3, 1, HW_AUDIO_TRACE_SAMPLE, 0, 0, 0, &frame),
              HW_AUDIO_TRACE_OK,
              "sample after selected timer is accepted");
    ASSERT_EQ(frame.left, 960, "selected timer consumes FIFO A's first byte");

    for (uint64_t cycle = 4u; cycle <= 7u; ++cycle)
    {
        ASSERT_EQ(apply_event(audio, cycle, 0, HW_AUDIO_TRACE_TIMER, 0, 0, 1, &frame),
                  HW_AUDIO_TRACE_OK,
                  "selected timer may drain zero bytes and then clock an empty FIFO");
        ASSERT_EQ(apply_event(audio, cycle, 1, HW_AUDIO_TRACE_SAMPLE, 0, 0, 0, &frame),
                  HW_AUDIO_TRACE_OK,
                  "sample after zero or empty FIFO clock is accepted");
        ASSERT_EQ(frame.left, 0, "zero bytes and the exhausted positive FIFO word are silent");
    }
    hw_audio_destroy(audio);
}

/* FIFO reset discards queued words but preserves mGBA's current internal word. */
static void test_trace_fifo_reset_preserves_internal_word(void)
{
    printf("Testing hw_audio trace contracts: FIFO reset internal word...\n");
    HwAudio* audio = hw_audio_create(48000.0f);
    HwAudioNativeFrame frame;
    hw_audio_trace_reset(audio);
    configure_directsound(audio, 0x0304);

    ASSERT_EQ(apply_event(audio, 1, 0, HW_AUDIO_TRACE_WRITE, 4, HW_AUDIO_GBA_IO_BASE + 0xA0, 0x00000B07, &frame),
              HW_AUDIO_TRACE_OK,
              "FIFO A accepts reset fixture bytes");
    ASSERT_EQ(apply_event(audio, 2, 0, HW_AUDIO_TRACE_TIMER, 0, 0, 0, &frame),
              HW_AUDIO_TRACE_OK,
              "timer makes the first FIFO byte audible");
    ASSERT_EQ(apply_event(audio, 2, 1, HW_AUDIO_TRACE_SAMPLE, 0, 0, 0, &frame),
              HW_AUDIO_TRACE_OK,
              "sample before FIFO reset is accepted");
    ASSERT_EQ(frame.left, 1344, "pre-reset FIFO hold is audible");

    ASSERT_EQ(apply_event(audio, 3, 0, HW_AUDIO_TRACE_WRITE, 2, HW_AUDIO_GBA_IO_BASE + 0x82, 0x0B04, &frame),
              HW_AUDIO_TRACE_OK,
              "SOUNDCNT_H FIFO A reset is accepted");
    ASSERT_EQ(apply_event(audio, 3, 1, HW_AUDIO_TRACE_SAMPLE, 0, 0, 0, &frame),
              HW_AUDIO_TRACE_OK,
              "sample immediately after FIFO reset is accepted");
    ASSERT_EQ(frame.left, 1344, "FIFO reset preserves the held sample");

    ASSERT_EQ(apply_event(audio, 4, 0, HW_AUDIO_TRACE_TIMER, 0, 0, 0, &frame),
              HW_AUDIO_TRACE_OK,
              "timer after FIFO reset is accepted");
    ASSERT_EQ(apply_event(audio, 4, 1, HW_AUDIO_TRACE_SAMPLE, 0, 0, 0, &frame),
              HW_AUDIO_TRACE_OK,
              "sample after reset and timer is accepted");
    ASSERT_EQ(frame.left, 2112, "FIFO reset preserves unread bytes in the current internal word");
    hw_audio_destroy(audio);
}

/* Byte writes must fit their declared bus width and leave their trace position available on rejection. */
static void test_trace_byte_write_value_must_fit(void)
{
    printf("Testing hw_audio trace contracts: byte write range...\n");
    HwAudio* audio = hw_audio_create(48000.0f);
    HwAudioNativeFrame frame;
    hw_audio_trace_reset(audio);

    ASSERT_EQ(apply_event(
                  audio, 0, 0, HW_AUDIO_TRACE_WRITE, 1, HW_AUDIO_GBA_IO_BASE + 0x62, (uint32_t)UINT8_MAX + 1u, &frame),
              HW_AUDIO_TRACE_INVALID_ARGUMENT,
              "oversized byte register write is rejected before mutation");
    ASSERT_EQ(apply_event(audio, 0, 0, HW_AUDIO_TRACE_WRITE, 1, HW_AUDIO_GBA_IO_BASE + 0x62, UINT8_MAX, &frame),
              HW_AUDIO_TRACE_OK,
              "maximum byte register write is accepted at the rejected event position");

    hw_audio_trace_reset(audio);
    ASSERT_EQ(apply_event(
                  audio, 0, 0, HW_AUDIO_TRACE_WRITE, 1, HW_AUDIO_GBA_IO_BASE + 0x90, (uint32_t)UINT8_MAX + 1u, &frame),
              HW_AUDIO_TRACE_INVALID_ARGUMENT,
              "oversized Wave RAM byte write is rejected before mutation");
    ASSERT_EQ(apply_event(audio, 0, 0, HW_AUDIO_TRACE_WRITE, 1, HW_AUDIO_GBA_IO_BASE + 0x90, UINT8_MAX, &frame),
              HW_AUDIO_TRACE_OK,
              "maximum Wave RAM byte write is accepted at the rejected event position");

    hw_audio_trace_reset(audio);
    ASSERT_EQ(apply_event(
                  audio, 0, 0, HW_AUDIO_TRACE_WRITE, 2, HW_AUDIO_GBA_IO_BASE + 0x80, (uint32_t)UINT16_MAX + 1u, &frame),
              HW_AUDIO_TRACE_INVALID_ARGUMENT,
              "oversized halfword register write is rejected before mutation");
    ASSERT_EQ(apply_event(audio, 0, 0, HW_AUDIO_TRACE_WRITE, 2, HW_AUDIO_GBA_IO_BASE + 0x80, UINT16_MAX, &frame),
              HW_AUDIO_TRACE_OK,
              "maximum halfword register write is accepted at the rejected event position");
    hw_audio_destroy(audio);
}

/* Signed-32 cycle validation must fail before either replay path mutates position. */
static void test_trace_cycle_range_is_bounded(void)
{
    printf("Testing hw_audio trace contracts: signed cycle range...\n");
    HwAudio* audio = hw_audio_create(48000.0f);
    HwAudioNativeFrame frame;
    hw_audio_trace_reset(audio);
    uint64_t invalid_cycle = (uint64_t)INT32_MAX + 1u;

    ASSERT_EQ(apply_event(audio, invalid_cycle, 0, HW_AUDIO_TRACE_SAMPLE, 0, 0, 0, &frame),
              HW_AUDIO_TRACE_INVALID_ARGUMENT,
              "immediate replay rejects cycles outside mGBA's signed timing domain");
    ASSERT_EQ(apply_event(audio, 0, 0, HW_AUDIO_TRACE_SAMPLE, 0, 0, 0, &frame),
              HW_AUDIO_TRACE_OK,
              "rejected cycle leaves the initial trace position available");

    HwAudioTraceEvent event = {
        .cycle = invalid_cycle,
        .kind = HW_AUDIO_TRACE_SAMPLE,
    };
    HwAudioTraceFifoSample sample;
    size_t sample_count = 0;
    ASSERT_EQ(hw_audio_trace_schedule_fifo_samples(&event, 1, &sample, 1, &sample_count),
              HW_AUDIO_TRACE_INVALID_ARGUMENT,
              "FIFO scheduler rejects cycles outside mGBA's signed timing domain");
    hw_audio_destroy(audio);
}

/* One native DAC slot can pair with exactly one explicit SAMPLE event. */
static void test_trace_fifo_schedule_rejects_duplicate_sample_slot(void)
{
    printf("Testing hw_audio trace contracts: duplicate SAMPLE slot...\n");
    static const HwAudioTraceEvent events[] = {
        {0, 0, HW_AUDIO_TRACE_SAMPLE, 0, 0, 0},
        {0, 1, HW_AUDIO_TRACE_SAMPLE, 0, 0, 0},
    };
    HwAudioTraceFifoSample samples[2];
    size_t sample_count = 0;
    ASSERT_EQ(hw_audio_trace_schedule_fifo_samples(events, 2, samples, 2, &sample_count),
              HW_AUDIO_TRACE_INVALID_ARGUMENT,
              "two markers cannot pair with the same native DAC slot");

    static const HwAudioTraceEvent resolution_events[] = {
        {0, 0, HW_AUDIO_TRACE_WRITE, 2, HW_AUDIO_GBA_IO_BASE + 0x88, 0x4200},
        {256, 0, HW_AUDIO_TRACE_SAMPLE, 0, 0, 0},
        {400, 0, HW_AUDIO_TRACE_WRITE, 2, HW_AUDIO_GBA_IO_BASE + 0x88, 0x0200},
        {512, 0, HW_AUDIO_TRACE_SAMPLE, 0, 0, 0},
    };
    sample_count = 0;
    ASSERT_EQ(hw_audio_trace_schedule_fifo_samples(resolution_events, 4, samples, 2, &sample_count),
              HW_AUDIO_TRACE_OK,
              "distinct cycles may reuse a slot index after a resolution change");
    ASSERT_EQ(sample_count, 2, "both resolution-epoch SAMPLE events are retained");
}

/* Parse one in-memory trace through the same shared grammar used by both drivers. */
static HwAudioTraceTextStatus parse_trace_text_fixture(const char* text, unsigned* error_line)
{
    FILE* input = tmpfile();
    ASSERT(input != NULL, "temporary trace fixture opens");
    if (!input)
        return HW_AUDIO_TRACE_TEXT_READ_FAILED;
    bool written = fputs(text, input) >= 0;
    ASSERT(written, "temporary trace fixture is written");
    if (!written)
    {
        fclose(input);
        return HW_AUDIO_TRACE_TEXT_READ_FAILED;
    }
    rewind(input);
    HwAudioTraceTextStatus status = hw_audio_trace_text_read(input, NULL, NULL, error_line);
    ASSERT_EQ(fclose(input), 0, "temporary trace fixture closes");
    return status;
}

/* Shared trace parsing accepts only checked unsigned canonical numeric tokens. */
static void test_trace_text_rejects_signed_and_overflowing_numbers(void)
{
    printf("Testing hw_audio trace contracts: checked text numbers...\n");
    unsigned error_line = 0;
    ASSERT_EQ(parse_trace_text_fixture("PORYAAAA_AUDIO_TRACE 1\n"
                                       "CLOCK 16777216\n"
                                       "BEGIN 0 0\n"
                                       "SAMPLE 0 1\n"
                                       "END 0 2\n",
                                       &error_line),
              HW_AUDIO_TRACE_TEXT_OK,
              "canonical unsigned trace tokens are accepted");

    ASSERT_EQ(parse_trace_text_fixture("PORYAAAA_AUDIO_TRACE 1\n"
                                       "CLOCK 16777216\n"
                                       "BEGIN 0 0\n"
                                       "SAMPLE 0 -1\n"
                                       "END 0 2\n",
                                       &error_line),
              HW_AUDIO_TRACE_TEXT_INVALID_EVENT,
              "signed order token is rejected");
    ASSERT_EQ(error_line, 4, "signed order rejection identifies its source line");

    ASSERT_EQ(parse_trace_text_fixture("PORYAAAA_AUDIO_TRACE 1\n"
                                       "CLOCK 16777216\n"
                                       "BEGIN 0 0\n"
                                       "SAMPLE 18446744073709551616 1\n"
                                       "END 0 2\n",
                                       &error_line),
              HW_AUDIO_TRACE_TEXT_INVALID_EVENT,
              "overflowing cycle token is rejected");
    ASSERT_EQ(error_line, 4, "overflowing cycle rejection identifies its source line");

    ASSERT_EQ(parse_trace_text_fixture("PORYAAAA_AUDIO_TRACE 1\n"
                                       "CLOCK 16777216\n"
                                       "BEGIN 0 0\n"
                                       "WRITE 0 1 2 04000080 0x00001177\n"
                                       "END 0 2\n",
                                       &error_line),
              HW_AUDIO_TRACE_TEXT_INVALID_EVENT,
              "hexadecimal address requires the canonical 0x prefix");
    ASSERT_EQ(error_line, 4, "noncanonical hexadecimal rejection identifies its source line");
}

/* GBA Wave RAM writes target the bank opposite NR30, and a trigger leaves the
 * old sample latched until mGBA's first delayed wave clock. */
static void test_trace_wave_ram_bank_and_trigger_delay(void)
{
    printf("Testing hw_audio trace contracts: Wave RAM bank and trigger delay...\n");
    HwAudio* audio = hw_audio_create(48000.0f);
    HwAudioNativeFrame frame;
    hw_audio_trace_reset(audio);

    ASSERT_EQ(apply_event(audio, 0, 0, HW_AUDIO_TRACE_WRITE, 2, HW_AUDIO_GBA_IO_BASE + 0x80, 0x4477, &frame),
              HW_AUDIO_TRACE_OK,
              "SOUNDCNT_L routes wave to both stereo sides");
    ASSERT_EQ(apply_event(audio, 0, 1, HW_AUDIO_TRACE_WRITE, 2, HW_AUDIO_GBA_IO_BASE + 0x82, 0x0002, &frame),
              HW_AUDIO_TRACE_OK,
              "SOUNDCNT_H sets full PSG volume");
    ASSERT_EQ(apply_event(audio, 0, 2, HW_AUDIO_TRACE_WRITE, 2, HW_AUDIO_GBA_IO_BASE + 0x84, 0x0080, &frame),
              HW_AUDIO_TRACE_OK,
              "SOUNDCNT_X enables PSG");
    ASSERT_EQ(apply_event(audio, 0, 3, HW_AUDIO_TRACE_WRITE, 2, HW_AUDIO_GBA_IO_BASE + 0x70, 0x0040, &frame),
              HW_AUDIO_TRACE_OK,
              "NR30 selects bank one so Wave RAM writes target bank zero");
    ASSERT_EQ(apply_event(audio, 0, 4, HW_AUDIO_TRACE_WRITE, 4, HW_AUDIO_GBA_IO_BASE + 0x90, 0x67452312, &frame),
              HW_AUDIO_TRACE_OK,
              "four-byte Wave RAM write is accepted");
    ASSERT_EQ(apply_event(audio, 0, 5, HW_AUDIO_TRACE_WRITE, 2, HW_AUDIO_GBA_IO_BASE + 0x70, 0x0080, &frame),
              HW_AUDIO_TRACE_OK,
              "wave DAC selects the populated bank zero");
    ASSERT_EQ(apply_event(audio, 0, 6, HW_AUDIO_TRACE_WRITE, 2, HW_AUDIO_GBA_IO_BASE + 0x72, 0x2000, &frame),
              HW_AUDIO_TRACE_OK,
              "wave volume is configured");
    ASSERT_EQ(apply_event(audio, 0, 7, HW_AUDIO_TRACE_WRITE, 2, HW_AUDIO_GBA_IO_BASE + 0x74, 0x87F8, &frame),
              HW_AUDIO_TRACE_OK,
              "wave trigger selects an eight-cycle frequency denominator");

    ASSERT_EQ(apply_event(audio, 87, 0, HW_AUDIO_TRACE_SAMPLE, 0, 0, 0, &frame),
              HW_AUDIO_TRACE_OK,
              "sample before the first delayed wave clock is accepted");
    ASSERT_EQ(frame.left, 0, "wave trigger preserves the old silent sample through cycle 87");
    ASSERT_EQ(frame.right, 0, "trigger latency is identical on both stereo sides");

    ASSERT_EQ(apply_event(audio, 88, 0, HW_AUDIO_TRACE_SAMPLE, 0, 0, 0, &frame),
              HW_AUDIO_TRACE_OK,
              "sample at the first delayed wave clock is accepted");
    ASSERT_EQ(frame.left, 768, "first clock exposes the high nibble of the low-address byte");
    ASSERT_EQ(frame.right, 768, "visible-bank Wave RAM data is centered");

    ASSERT_EQ(apply_event(audio, 100, 0, HW_AUDIO_TRACE_WRITE, 1, HW_AUDIO_GBA_IO_BASE + 0x75, 0x07, &frame),
              HW_AUDIO_TRACE_OK,
              "active no-trigger NR34 write is accepted");
    ASSERT_EQ(apply_event(audio, 152, 0, HW_AUDIO_TRACE_SAMPLE, 0, 0, 0, &frame),
              HW_AUDIO_TRACE_OK,
              "sample at the old next-update time is accepted");
    ASSERT_EQ(frame.left, 768, "active NR34 write reschedules the next wave clock");
    ASSERT_EQ(frame.right, 768, "rescheduled wave hold remains centered");

    ASSERT_EQ(apply_event(audio, 188, 0, HW_AUDIO_TRACE_SAMPLE, 0, 0, 0, &frame),
              HW_AUDIO_TRACE_OK,
              "sample at the rescheduled wave clock is accepted");
    ASSERT_EQ(frame.left, 1536, "rescheduled clock reaches the low nibble");
    ASSERT_EQ(frame.right, 1536, "rescheduled Wave RAM nibble remains centered");

    ASSERT_EQ(apply_event(audio, 316, 0, HW_AUDIO_TRACE_SAMPLE, 0, 0, 0, &frame),
              HW_AUDIO_TRACE_OK,
              "sample after two more wave clocks is accepted");
    ASSERT_EQ(frame.left, 2304, "little-endian order reaches the next byte's low nibble");
    ASSERT_EQ(frame.right, 2304, "Wave RAM bus order remains centered");
    hw_audio_destroy(audio);
}

/* An active NR30 write selects its bank before mGBA forces the overdue Wave clock. */
static void test_trace_wave_nr30_selects_bank_before_clock(void)
{
    printf("Testing hw_audio trace contracts: NR30 bank-before-clock order...\n");
    HwAudio* audio = hw_audio_create(48000.0f);
    HwAudioNativeFrame frame;
    hw_audio_trace_reset(audio);

    ASSERT_EQ(apply_event(audio, 0, 0, HW_AUDIO_TRACE_WRITE, 2, HW_AUDIO_GBA_IO_BASE + 0x80, 0x4477, &frame),
              HW_AUDIO_TRACE_OK,
              "wave is routed to both stereo sides");
    ASSERT_EQ(apply_event(audio, 0, 1, HW_AUDIO_TRACE_WRITE, 2, HW_AUDIO_GBA_IO_BASE + 0x82, 0x0002, &frame),
              HW_AUDIO_TRACE_OK,
              "full PSG volume is configured");
    ASSERT_EQ(apply_event(audio, 0, 2, HW_AUDIO_TRACE_WRITE, 2, HW_AUDIO_GBA_IO_BASE + 0x84, 0x0080, &frame),
              HW_AUDIO_TRACE_OK,
              "PSG is enabled");
    ASSERT_EQ(apply_event(audio, 0, 3, HW_AUDIO_TRACE_WRITE, 2, HW_AUDIO_GBA_IO_BASE + 0x70, 0x0040, &frame),
              HW_AUDIO_TRACE_OK,
              "bank zero is exposed for CPU writes");
    ASSERT_EQ(apply_event(audio, 0, 4, HW_AUDIO_TRACE_WRITE, 4, HW_AUDIO_GBA_IO_BASE + 0x90, 0x00000012, &frame),
              HW_AUDIO_TRACE_OK,
              "bank zero receives its fixture nibble");
    ASSERT_EQ(apply_event(audio, 0, 5, HW_AUDIO_TRACE_WRITE, 2, HW_AUDIO_GBA_IO_BASE + 0x70, 0x0000, &frame),
              HW_AUDIO_TRACE_OK,
              "bank one is exposed for CPU writes");
    ASSERT_EQ(apply_event(audio, 0, 6, HW_AUDIO_TRACE_WRITE, 4, HW_AUDIO_GBA_IO_BASE + 0x90, 0x00000034, &frame),
              HW_AUDIO_TRACE_OK,
              "bank one receives a distinct fixture nibble");
    ASSERT_EQ(apply_event(audio, 0, 7, HW_AUDIO_TRACE_WRITE, 2, HW_AUDIO_GBA_IO_BASE + 0x70, 0x0080, &frame),
              HW_AUDIO_TRACE_OK,
              "wave starts from bank zero");
    ASSERT_EQ(apply_event(audio, 0, 8, HW_AUDIO_TRACE_WRITE, 2, HW_AUDIO_GBA_IO_BASE + 0x72, 0x2000, &frame),
              HW_AUDIO_TRACE_OK,
              "wave volume is configured");
    ASSERT_EQ(apply_event(audio, 0, 9, HW_AUDIO_TRACE_WRITE, 2, HW_AUDIO_GBA_IO_BASE + 0x74, 0x87F8, &frame),
              HW_AUDIO_TRACE_OK,
              "wave is triggered");

    ASSERT_EQ(apply_event(audio, 88, 0, HW_AUDIO_TRACE_WRITE, 4, HW_AUDIO_GBA_IO_BASE + 0xA0, 0, &frame),
              HW_AUDIO_TRACE_OK,
              "intervening FIFO write does not force Wave");
    ASSERT_EQ(apply_event(audio, 88, 1, HW_AUDIO_TRACE_WRITE, 2, HW_AUDIO_GBA_IO_BASE + 0x70, 0x00C0, &frame),
              HW_AUDIO_TRACE_OK,
              "active NR30 write selects bank one at the pending clock");
    ASSERT_EQ(apply_event(audio, 88, 2, HW_AUDIO_TRACE_SAMPLE, 0, 0, 0, &frame),
              HW_AUDIO_TRACE_OK,
              "sample after active NR30 write is accepted");
    ASSERT_EQ(frame.left, 2304, "forced Wave clock reads the newly selected bank");
    ASSERT_EQ(frame.right, 2304, "NR30 bank-before-clock order remains centered");
    hw_audio_destroy(audio);
}

/* Wave RAM writes force pending 64-sample clocks before mutating either bank. */
static void test_trace_wave_ram_write_forces_pending_clock(void)
{
    printf("Testing hw_audio trace contracts: Wave RAM write force order...\n");
    HwAudio* audio = hw_audio_create(48000.0f);
    HwAudioNativeFrame frame;
    hw_audio_trace_reset(audio);

    ASSERT_EQ(apply_event(audio, 0, 0, HW_AUDIO_TRACE_WRITE, 2, HW_AUDIO_GBA_IO_BASE + 0x80, 0x4477, &frame),
              HW_AUDIO_TRACE_OK,
              "wave is routed to both stereo sides");
    ASSERT_EQ(apply_event(audio, 0, 1, HW_AUDIO_TRACE_WRITE, 2, HW_AUDIO_GBA_IO_BASE + 0x82, 0x0002, &frame),
              HW_AUDIO_TRACE_OK,
              "full PSG volume is configured");
    ASSERT_EQ(apply_event(audio, 0, 2, HW_AUDIO_TRACE_WRITE, 2, HW_AUDIO_GBA_IO_BASE + 0x84, 0x0080, &frame),
              HW_AUDIO_TRACE_OK,
              "PSG is enabled");
    ASSERT_EQ(apply_event(audio, 0, 3, HW_AUDIO_TRACE_WRITE, 2, HW_AUDIO_GBA_IO_BASE + 0x70, 0x0040, &frame),
              HW_AUDIO_TRACE_OK,
              "bank zero is exposed for CPU writes");
    ASSERT_EQ(apply_event(audio, 0, 4, HW_AUDIO_TRACE_WRITE, 4, HW_AUDIO_GBA_IO_BASE + 0x90, 0x00000012, &frame),
              HW_AUDIO_TRACE_OK,
              "bank zero receives its fixture");
    ASSERT_EQ(apply_event(audio, 0, 5, HW_AUDIO_TRACE_WRITE, 2, HW_AUDIO_GBA_IO_BASE + 0x70, 0x0000, &frame),
              HW_AUDIO_TRACE_OK,
              "bank one is exposed for CPU writes");
    ASSERT_EQ(apply_event(audio, 0, 6, HW_AUDIO_TRACE_WRITE, 4, HW_AUDIO_GBA_IO_BASE + 0x90, 0x00000034, &frame),
              HW_AUDIO_TRACE_OK,
              "bank one receives its original fixture");
    ASSERT_EQ(apply_event(audio, 0, 7, HW_AUDIO_TRACE_WRITE, 2, HW_AUDIO_GBA_IO_BASE + 0x70, 0x00A0, &frame),
              HW_AUDIO_TRACE_OK,
              "64-sample playback starts from bank zero");
    ASSERT_EQ(apply_event(audio, 0, 8, HW_AUDIO_TRACE_WRITE, 2, HW_AUDIO_GBA_IO_BASE + 0x72, 0x2000, &frame),
              HW_AUDIO_TRACE_OK,
              "wave volume is configured");
    ASSERT_EQ(apply_event(audio, 0, 9, HW_AUDIO_TRACE_WRITE, 2, HW_AUDIO_GBA_IO_BASE + 0x74, 0x87F8, &frame),
              HW_AUDIO_TRACE_OK,
              "64-sample wave is triggered");

    ASSERT_EQ(apply_event(audio, 88, 0, HW_AUDIO_TRACE_WRITE, 4, HW_AUDIO_GBA_IO_BASE + 0xA0, 0, &frame),
              HW_AUDIO_TRACE_OK,
              "FIFO write leaves the first Wave clock pending");
    ASSERT_EQ(apply_event(audio, 88, 1, HW_AUDIO_TRACE_WRITE, 4, HW_AUDIO_GBA_IO_BASE + 0x90, 0x00000056, &frame),
              HW_AUDIO_TRACE_OK,
              "Wave RAM write forces old data before replacing bank one");
    ASSERT_EQ(apply_event(audio, 88, 2, HW_AUDIO_TRACE_WRITE, 2, HW_AUDIO_GBA_IO_BASE + 0x70, 0x00C0, &frame),
              HW_AUDIO_TRACE_OK,
              "playback selects the newly written bank one");
    ASSERT_EQ(apply_event(audio, 88, 3, HW_AUDIO_TRACE_WRITE, 2, HW_AUDIO_GBA_IO_BASE + 0x74, 0x87F8, &frame),
              HW_AUDIO_TRACE_OK,
              "bank one is retriggered");
    ASSERT_EQ(apply_event(audio, 176, 0, HW_AUDIO_TRACE_SAMPLE, 0, 0, 0, &frame),
              HW_AUDIO_TRACE_OK,
              "first clock of the replaced bank is observed");
    ASSERT_EQ(frame.left, 3840, "Wave RAM replacement remains unrotated after the preceding force");
    ASSERT_EQ(frame.right, 3840, "Wave RAM force-before-write order remains centered");
    hw_audio_destroy(audio);
}

/* NR52 disable consumes no residual Wave clocks beyond the preceding observed sample. */
static void test_trace_wave_nr52_disable_preserves_residual_ram_phase(void)
{
    printf("Testing hw_audio trace contracts: NR52 disable Wave phase...\n");
    HwAudio* audio = hw_audio_create(48000.0f);
    HwAudioNativeFrame frame;
    hw_audio_trace_reset(audio);

    ASSERT_EQ(apply_event(audio, 0, 0, HW_AUDIO_TRACE_WRITE, 2, HW_AUDIO_GBA_IO_BASE + 0x80, 0x4477, &frame),
              HW_AUDIO_TRACE_OK,
              "wave is routed to both stereo sides");
    ASSERT_EQ(apply_event(audio, 0, 1, HW_AUDIO_TRACE_WRITE, 2, HW_AUDIO_GBA_IO_BASE + 0x82, 0x0002, &frame),
              HW_AUDIO_TRACE_OK,
              "full PSG volume is configured");
    ASSERT_EQ(apply_event(audio, 0, 2, HW_AUDIO_TRACE_WRITE, 2, HW_AUDIO_GBA_IO_BASE + 0x84, 0x0080, &frame),
              HW_AUDIO_TRACE_OK,
              "PSG is enabled");
    ASSERT_EQ(apply_event(audio, 0, 3, HW_AUDIO_TRACE_WRITE, 2, HW_AUDIO_GBA_IO_BASE + 0x70, 0x0040, &frame),
              HW_AUDIO_TRACE_OK,
              "bank zero is exposed for CPU writes");
    ASSERT_EQ(apply_event(audio, 0, 4, HW_AUDIO_TRACE_WRITE, 4, HW_AUDIO_GBA_IO_BASE + 0x90, 0x67452312, &frame),
              HW_AUDIO_TRACE_OK,
              "bank zero receives the ordered Wave fixture");
    ASSERT_EQ(apply_event(audio, 0, 5, HW_AUDIO_TRACE_WRITE, 2, HW_AUDIO_GBA_IO_BASE + 0x70, 0x0080, &frame),
              HW_AUDIO_TRACE_OK,
              "wave DAC selects bank zero");
    ASSERT_EQ(apply_event(audio, 0, 6, HW_AUDIO_TRACE_WRITE, 2, HW_AUDIO_GBA_IO_BASE + 0x72, 0x2000, &frame),
              HW_AUDIO_TRACE_OK,
              "wave volume is configured");
    ASSERT_EQ(apply_event(audio, 0, 7, HW_AUDIO_TRACE_WRITE, 2, HW_AUDIO_GBA_IO_BASE + 0x74, 0x87F8, &frame),
              HW_AUDIO_TRACE_OK,
              "wave is triggered");
    ASSERT_EQ(apply_event(audio, 88, 0, HW_AUDIO_TRACE_SAMPLE, 0, 0, 0, &frame),
              HW_AUDIO_TRACE_OK,
              "first Wave clock is observed");
    ASSERT_EQ(frame.left, 768, "first fixture nibble is latched");

    ASSERT_EQ(apply_event(audio, 152, 0, HW_AUDIO_TRACE_WRITE, 4, HW_AUDIO_GBA_IO_BASE + 0xA0, 0, &frame),
              HW_AUDIO_TRACE_OK,
              "intervening FIFO write retains the overdue Wave clock");

    ASSERT_EQ(apply_event(audio, 160, 0, HW_AUDIO_TRACE_WRITE, 2, HW_AUDIO_GBA_IO_BASE + 0x84, 0x0000, &frame),
              HW_AUDIO_TRACE_OK,
              "NR52 disables PSG after the observed sample");
    ASSERT_EQ(apply_event(audio, 160, 1, HW_AUDIO_TRACE_WRITE, 2, HW_AUDIO_GBA_IO_BASE + 0x84, 0x0080, &frame),
              HW_AUDIO_TRACE_OK,
              "PSG is re-enabled");
    ASSERT_EQ(apply_event(audio, 160, 2, HW_AUDIO_TRACE_WRITE, 2, HW_AUDIO_GBA_IO_BASE + 0x70, 0x0080, &frame),
              HW_AUDIO_TRACE_OK,
              "preserved bank zero is selected");
    ASSERT_EQ(apply_event(audio, 160, 3, HW_AUDIO_TRACE_WRITE, 2, HW_AUDIO_GBA_IO_BASE + 0x72, 0x2000, &frame),
              HW_AUDIO_TRACE_OK,
              "wave volume is restored");
    ASSERT_EQ(apply_event(audio, 160, 4, HW_AUDIO_TRACE_WRITE, 2, HW_AUDIO_GBA_IO_BASE + 0x74, 0x87F8, &frame),
              HW_AUDIO_TRACE_OK,
              "wave is retriggered");
    ASSERT_EQ(apply_event(audio, 248, 0, HW_AUDIO_TRACE_SAMPLE, 0, 0, 0, &frame),
              HW_AUDIO_TRACE_OK,
              "first post-enable Wave clock is observed");
    ASSERT_EQ(frame.left, 1536, "NR52 disable did not consume the second fixture nibble");
    ASSERT_EQ(frame.right, 1536, "preserved residual Wave phase remains centered");
    hw_audio_destroy(audio);
}

/* Frame-sequencer length expiry must stop Wave rotation inside one large trace delta. */
static void test_trace_wave_length_expiry_gates_sparse_delta(void)
{
    printf("Testing hw_audio trace contracts: Wave length expiry inside sparse delta...\n");
    HwAudio* audio = hw_audio_create(48000.0f);
    HwAudioNativeFrame frame;
    hw_audio_trace_reset(audio);

    ASSERT_EQ(apply_event(audio, 0, 0, HW_AUDIO_TRACE_WRITE, 2, HW_AUDIO_GBA_IO_BASE + 0x80, 0x4477, &frame),
              HW_AUDIO_TRACE_OK,
              "wave is routed to both stereo sides");
    ASSERT_EQ(apply_event(audio, 0, 1, HW_AUDIO_TRACE_WRITE, 2, HW_AUDIO_GBA_IO_BASE + 0x82, 0x0002, &frame),
              HW_AUDIO_TRACE_OK,
              "full PSG volume is configured");
    ASSERT_EQ(apply_event(audio, 0, 2, HW_AUDIO_TRACE_WRITE, 2, HW_AUDIO_GBA_IO_BASE + 0x84, 0x0080, &frame),
              HW_AUDIO_TRACE_OK,
              "PSG is enabled at frame step seven");
    ASSERT_EQ(apply_event(audio, 0, 3, HW_AUDIO_TRACE_WRITE, 2, HW_AUDIO_GBA_IO_BASE + 0x70, 0x0040, &frame),
              HW_AUDIO_TRACE_OK,
              "bank zero is exposed for CPU writes");
    ASSERT_EQ(apply_event(audio, 0, 4, HW_AUDIO_TRACE_WRITE, 4, HW_AUDIO_GBA_IO_BASE + 0x90, 0x67452312, &frame),
              HW_AUDIO_TRACE_OK,
              "bank zero receives the sparse-delta fixture");
    ASSERT_EQ(apply_event(audio, 0, 5, HW_AUDIO_TRACE_WRITE, 2, HW_AUDIO_GBA_IO_BASE + 0x70, 0x0080, &frame),
              HW_AUDIO_TRACE_OK,
              "wave DAC selects bank zero");
    ASSERT_EQ(apply_event(audio, 0, 6, HW_AUDIO_TRACE_WRITE, 2, HW_AUDIO_GBA_IO_BASE + 0x72, 0x20FF, &frame),
              HW_AUDIO_TRACE_OK,
              "wave length is one frame-sequencer tick");
    ASSERT_EQ(apply_event(audio, 0, 7, HW_AUDIO_TRACE_WRITE, 2, HW_AUDIO_GBA_IO_BASE + 0x74, 0xC7F8, &frame),
              HW_AUDIO_TRACE_OK,
              "length-gated wave is triggered");

    ASSERT_EQ(apply_event(audio, 65568, 0, HW_AUDIO_TRACE_WRITE, 2, HW_AUDIO_GBA_IO_BASE + 0x74, 0xC7F8, &frame),
              HW_AUDIO_TRACE_OK,
              "sparse delta reaches length expiry before retrigger");
    ASSERT_EQ(apply_event(audio, 65656, 0, HW_AUDIO_TRACE_SAMPLE, 0, 0, 0, &frame),
              HW_AUDIO_TRACE_OK,
              "first sample after sparse-delta retrigger is accepted");
    ASSERT_EQ(frame.left, 0, "length expiry stopped rotation before the empty final bank nibble");
    ASSERT_EQ(frame.right, 0, "length-gated sparse rotation remains centered");
    ASSERT_EQ(apply_event(audio, 65720, 0, HW_AUDIO_TRACE_SAMPLE, 0, 0, 0, &frame),
              HW_AUDIO_TRACE_OK,
              "next Wave clock after sparse-delta retrigger is accepted");
    ASSERT_EQ(frame.left, 768, "rotation resumes at the first populated nibble");
    ASSERT_EQ(frame.right, 768, "post-expiry Wave rotation remains centered");
    hw_audio_destroy(audio);
}

/* Repeated NR52-off writes clear bank and size fields written while powered down. */
static void test_trace_wave_repeated_nr52_off_clears_bank(void)
{
    printf("Testing hw_audio trace contracts: repeated NR52-off Wave state...\n");
    HwAudio* audio = hw_audio_create(48000.0f);
    HwAudioNativeFrame frame;
    hw_audio_trace_reset(audio);

    ASSERT_EQ(apply_event(audio, 0, 0, HW_AUDIO_TRACE_WRITE, 2, HW_AUDIO_GBA_IO_BASE + 0x70, 0x0060, &frame),
              HW_AUDIO_TRACE_OK,
              "powered-off NR30 sets bank and size");
    ASSERT_EQ(apply_event(audio, 0, 1, HW_AUDIO_TRACE_WRITE, 2, HW_AUDIO_GBA_IO_BASE + 0x84, 0x0000, &frame),
              HW_AUDIO_TRACE_OK,
              "repeated NR52-off write clears powered-down channel state");
    ASSERT_EQ(apply_event(audio, 0, 2, HW_AUDIO_TRACE_WRITE, 2, HW_AUDIO_GBA_IO_BASE + 0x84, 0x0080, &frame),
              HW_AUDIO_TRACE_OK,
              "PSG is enabled");
    ASSERT_EQ(apply_event(audio, 0, 3, HW_AUDIO_TRACE_WRITE, 2, HW_AUDIO_GBA_IO_BASE + 0x80, 0x4477, &frame),
              HW_AUDIO_TRACE_OK,
              "wave is routed to both stereo sides");
    ASSERT_EQ(apply_event(audio, 0, 4, HW_AUDIO_TRACE_WRITE, 2, HW_AUDIO_GBA_IO_BASE + 0x82, 0x0002, &frame),
              HW_AUDIO_TRACE_OK,
              "full PSG volume is configured");
    ASSERT_EQ(apply_event(audio, 0, 5, HW_AUDIO_TRACE_WRITE, 4, HW_AUDIO_GBA_IO_BASE + 0x90, 0x00000012, &frame),
              HW_AUDIO_TRACE_OK,
              "cleared bank zero selection routes CPU write to bank one");
    ASSERT_EQ(apply_event(audio, 0, 6, HW_AUDIO_TRACE_WRITE, 2, HW_AUDIO_GBA_IO_BASE + 0x70, 0x00C0, &frame),
              HW_AUDIO_TRACE_OK,
              "wave DAC selects populated bank one");
    ASSERT_EQ(apply_event(audio, 0, 7, HW_AUDIO_TRACE_WRITE, 2, HW_AUDIO_GBA_IO_BASE + 0x72, 0x2000, &frame),
              HW_AUDIO_TRACE_OK,
              "wave volume is configured");
    ASSERT_EQ(apply_event(audio, 0, 8, HW_AUDIO_TRACE_WRITE, 2, HW_AUDIO_GBA_IO_BASE + 0x74, 0x87F8, &frame),
              HW_AUDIO_TRACE_OK,
              "wave is triggered");
    ASSERT_EQ(apply_event(audio, 88, 0, HW_AUDIO_TRACE_SAMPLE, 0, 0, 0, &frame),
              HW_AUDIO_TRACE_OK,
              "first bank-one Wave clock is observed");
    ASSERT_EQ(frame.left, 768, "repeated NR52-off cleared stale bank selection");
    ASSERT_EQ(frame.right, 768, "cleared bank selection remains centered");
    hw_audio_destroy(audio);
}

/* SOUNDBIAS changes the unsigned DAC clip window without adding a silent DC offset. */
static void test_trace_soundbias_clipping(void)
{
    printf("Testing hw_audio trace contracts: SOUNDBIAS clipping...\n");
    HwAudio* audio = hw_audio_create(48000.0f);
    HwAudioNativeFrame frame;
    hw_audio_trace_reset(audio);
    configure_directsound(audio, 0x220C);

    ASSERT_EQ(apply_event(audio, 1, 0, HW_AUDIO_TRACE_WRITE, 4, HW_AUDIO_GBA_IO_BASE + 0xA0, 0x0000007F, &frame),
              HW_AUDIO_TRACE_OK,
              "positive FIFO A fixture is accepted");
    ASSERT_EQ(apply_event(audio, 1, 1, HW_AUDIO_TRACE_WRITE, 4, HW_AUDIO_GBA_IO_BASE + 0xA4, 0x0000007F, &frame),
              HW_AUDIO_TRACE_OK,
              "positive FIFO B fixture is accepted");
    ASSERT_EQ(apply_event(audio, 2, 0, HW_AUDIO_TRACE_TIMER, 0, 0, 0, &frame),
              HW_AUDIO_TRACE_OK,
              "positive fixtures are clocked");
    ASSERT_EQ(apply_event(audio, 2, 1, HW_AUDIO_TRACE_SAMPLE, 0, 0, 0, &frame),
              HW_AUDIO_TRACE_OK,
              "positive clipping sample is accepted");
    ASSERT_EQ(frame.left, 24528, "default bias clips summed positive DirectSound at the DAC ceiling");
    ASSERT_EQ(frame.right, 0, "unrouted side remains silent at default bias");

    ASSERT_EQ(apply_event(audio, 3, 0, HW_AUDIO_TRACE_WRITE, 2, HW_AUDIO_GBA_IO_BASE + 0x88, 0x0000, &frame),
              HW_AUDIO_TRACE_OK,
              "zero-bias write is accepted");
    ASSERT_EQ(apply_event(audio, 3, 1, HW_AUDIO_TRACE_SAMPLE, 0, 0, 0, &frame),
              HW_AUDIO_TRACE_OK,
              "zero-bias clipping sample is accepted");
    ASSERT_EQ(frame.left, -16768, "zero bias preserves mGBA's final int16 wrap for +1016 DAC units");
    ASSERT_EQ(frame.right, 0, "bias changes do not introduce silent-side DC");

    hw_audio_trace_reset(audio);
    configure_directsound(audio, 0x220C);
    ASSERT_EQ(apply_event(audio, 1, 0, HW_AUDIO_TRACE_WRITE, 4, HW_AUDIO_GBA_IO_BASE + 0xA0, 0x00000080, &frame),
              HW_AUDIO_TRACE_OK,
              "negative FIFO A fixture is accepted");
    ASSERT_EQ(apply_event(audio, 1, 1, HW_AUDIO_TRACE_WRITE, 4, HW_AUDIO_GBA_IO_BASE + 0xA4, 0x00000080, &frame),
              HW_AUDIO_TRACE_OK,
              "negative FIFO B fixture is accepted");
    ASSERT_EQ(apply_event(audio, 2, 0, HW_AUDIO_TRACE_TIMER, 0, 0, 0, &frame),
              HW_AUDIO_TRACE_OK,
              "negative fixtures are clocked");
    ASSERT_EQ(apply_event(audio, 2, 1, HW_AUDIO_TRACE_SAMPLE, 0, 0, 0, &frame),
              HW_AUDIO_TRACE_OK,
              "negative clipping sample is accepted");
    ASSERT_EQ(frame.left, -24576, "default bias clips summed negative DirectSound at the DAC floor");

    ASSERT_EQ(apply_event(audio, 3, 0, HW_AUDIO_TRACE_WRITE, 2, HW_AUDIO_GBA_IO_BASE + 0x88, 0x03FE, &frame),
              HW_AUDIO_TRACE_OK,
              "high-bias write is accepted");
    ASSERT_EQ(apply_event(audio, 3, 1, HW_AUDIO_TRACE_SAMPLE, 0, 0, 0, &frame),
              HW_AUDIO_TRACE_OK,
              "high-bias negative clipping sample is accepted");
    ASSERT_EQ(frame.left, 16480, "high bias preserves mGBA's final int16 wrap below -32768");
    ASSERT_EQ(frame.right, 0, "bias changes preserve silent unrouted output");
    hw_audio_destroy(audio);
}

/* Trace validation rejects a lower same-cycle order before that event can mutate state. */
static void test_trace_same_cycle_reorder_is_rejected(void)
{
    printf("Testing hw_audio trace contracts: same-cycle reorder rejection...\n");
    HwAudio* audio = hw_audio_create(48000.0f);
    HwAudioNativeFrame frame;
    hw_audio_trace_reset(audio);

    ASSERT_EQ(apply_event(audio, 12, 5, HW_AUDIO_TRACE_SAMPLE, 0, 0, 0, &frame),
              HW_AUDIO_TRACE_OK,
              "initial same-cycle SAMPLE is accepted");
    ASSERT_EQ(apply_event(audio, 12, 4, HW_AUDIO_TRACE_WRITE, 2, HW_AUDIO_GBA_IO_BASE + 0x82, 0x0304, &frame),
              HW_AUDIO_TRACE_OUT_OF_ORDER,
              "lower same-cycle order is rejected as an ambiguous trace");
    ASSERT_EQ(apply_event(audio, 12, 6, HW_AUDIO_TRACE_SAMPLE, 0, 0, 0, &frame),
              HW_AUDIO_TRACE_OK,
              "strictly later same-cycle order remains usable after rejection");
    ASSERT(frame.cycle == 12, "accepted SAMPLE retains its absolute cycle");
    hw_audio_destroy(audio);
}

/* DirectSound timer observations do not partition mGBA's batched noise run. */
static void test_trace_timer_does_not_partition_noise_clock(void)
{
    printf("Testing hw_audio trace contracts: timer does not partition noise clock...\n");
    HwAudio* audio = hw_audio_create(48000.0f);
    HwAudioNativeFrame frame;
    hw_audio_trace_reset(audio);

    ASSERT_EQ(apply_event(audio, 0, 1, HW_AUDIO_TRACE_WRITE, 2, HW_AUDIO_GBA_IO_BASE + 0x80, 0x8877, &frame),
              HW_AUDIO_TRACE_OK,
              "SOUNDCNT_L routes noise to both sides");
    ASSERT_EQ(apply_event(audio, 0, 2, HW_AUDIO_TRACE_WRITE, 2, HW_AUDIO_GBA_IO_BASE + 0x84, 0x0080, &frame),
              HW_AUDIO_TRACE_OK,
              "SOUNDCNT_X enables PSG");
    ASSERT_EQ(apply_event(audio, 0, 3, HW_AUDIO_TRACE_WRITE, 1, HW_AUDIO_GBA_IO_BASE + 0x79, 0xF0, &frame),
              HW_AUDIO_TRACE_OK,
              "SOUND4CNT_L loads a constant volume-15 envelope");
    ASSERT_EQ(apply_event(audio, 0, 4, HW_AUDIO_TRACE_WRITE, 1, HW_AUDIO_GBA_IO_BASE + 0x7C, 0x00, &frame),
              HW_AUDIO_TRACE_OK,
              "SOUND4CNT_H selects the 32-cycle noise period");
    ASSERT_EQ(apply_event(audio, 0, 5, HW_AUDIO_TRACE_WRITE, 1, HW_AUDIO_GBA_IO_BASE + 0x7D, 0x80, &frame),
              HW_AUDIO_TRACE_OK,
              "SOUND4CNT_H trigger starts noise");
    ASSERT_EQ(apply_event(audio, 224, 0, HW_AUDIO_TRACE_SAMPLE, 0, 0, 0, &frame),
              HW_AUDIO_TRACE_OK,
              "seven noise clocks establish the partition-sensitive LFSR state");
    ASSERT_EQ(apply_event(audio, 320, 0, HW_AUDIO_TRACE_TIMER, 0, 0, 0, &frame),
              HW_AUDIO_TRACE_OK,
              "DirectSound timer observation is accepted");
    ASSERT_EQ(apply_event(audio, 480, 0, HW_AUDIO_TRACE_SAMPLE, 0, 0, 0, &frame),
              HW_AUDIO_TRACE_OK,
              "next native sample is accepted");
    ASSERT_EQ(frame.left, 0, "timer does not change the mGBA batched noise sample on left");
    ASSERT_EQ(frame.right, 0, "timer does not change the mGBA batched noise sample on right");
    hw_audio_destroy(audio);
}

void test_hw_audio_trace_contracts_run_all(void)
{
    test_trace_sample_and_write_same_cycle_order();
    test_trace_mid_sample_write_uses_exact_cycle_phase();
    test_trace_square_phase_spans_nr52_power_off();
    test_trace_noise_trigger_feedback_and_clock_origin();
    test_trace_frame_envelope_uses_absolute_nr52_cadence();
    test_trace_cycle_zero_frame_event_matches_mgba_epoch();
    test_trace_soundbias_selects_all_dac_intervals();
    test_trace_fifo_stereo_little_endian();
    test_trace_fifo_schedule_resolves_current_sample_block();
    test_trace_fifo_schedule_empty_clock_extends_held_suffix();
    test_trace_fifo_schedule_full_pointer_alias_extends_held_suffix();
    test_trace_fifo_schedule_cgb_batch_finalizes_pending_sample();
    test_trace_staged_sample_observes_terminal_write();
    test_trace_fifo_schedule_accepts_explicit_source_phase();
    test_trace_timer_selection_and_empty_fifo_silence();
    test_trace_fifo_reset_preserves_internal_word();
    test_trace_byte_write_value_must_fit();
    test_trace_cycle_range_is_bounded();
    test_trace_fifo_schedule_rejects_duplicate_sample_slot();
    test_trace_text_rejects_signed_and_overflowing_numbers();
    test_trace_wave_ram_bank_and_trigger_delay();
    test_trace_wave_nr30_selects_bank_before_clock();
    test_trace_wave_ram_write_forces_pending_clock();
    test_trace_wave_nr52_disable_preserves_residual_ram_phase();
    test_trace_wave_length_expiry_gates_sparse_delta();
    test_trace_wave_repeated_nr52_off_clears_bank();
    test_trace_soundbias_clipping();
    test_trace_same_cycle_reorder_is_rejected();
    test_trace_timer_does_not_partition_noise_clock();
}

#ifdef PORYAAAA_HW_AUDIO_TRACE_CONTRACTS_TEST_MAIN
int tests_run = 0;
int tests_passed = 0;

int main(void)
{
    test_hw_audio_trace_contracts_run_all();
    printf("hw_audio trace contracts: %d/%d tests passed\n", tests_passed, tests_run);
    return tests_passed == tests_run ? 0 : 1;
}
#endif
