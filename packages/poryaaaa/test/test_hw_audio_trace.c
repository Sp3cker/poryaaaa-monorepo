#include "hw_audio/hw_audio_trace.h"
#include "test_assert.h"

#include <stdbool.h>
#include <stdio.h>

/* Apply one trace event while keeping same-cycle order explicit in each test. */
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

/* Prove reset and explicit SAMPLE events produce canonical signed stereo. */
static void test_trace_reset_starts_silent(void)
{
    printf("Testing hw_audio trace: hardware reset starts silent...\n");
    HwAudio* audio = hw_audio_create(48000.0f);
    HwAudioNativeFrame frame;
    hw_audio_trace_reset(audio);

    HwAudioTraceStatus status = apply_event(audio, 0, 0, HW_AUDIO_TRACE_SAMPLE, 0, 0, 0, &frame);

    ASSERT_EQ(status, HW_AUDIO_TRACE_OK, "reset SAMPLE is accepted");
    ASSERT_EQ(frame.left, 0, "reset left output is zero");
    ASSERT_EQ(frame.right, 0, "reset right output is zero");
    ASSERT(frame.cycle == 0, "native frame retains absolute GBA cycle");
    hw_audio_destroy(audio);
}

/* Prove a GBA register trace can drive audible PSG without the host resampler. */
static void test_trace_square_register_replay_is_audible(void)
{
    printf("Testing hw_audio trace: register writes drive native square output...\n");
    HwAudio* audio = hw_audio_create(48000.0f);
    HwAudioNativeFrame frame;
    hw_audio_trace_reset(audio);

    ASSERT_EQ(apply_event(audio, 0, 0, HW_AUDIO_TRACE_WRITE, 2, 0x04000080, 0x1177, &frame),
              HW_AUDIO_TRACE_OK,
              "SOUNDCNT_L is accepted");
    ASSERT_EQ(apply_event(audio, 0, 1, HW_AUDIO_TRACE_WRITE, 2, 0x04000082, 0x0002, &frame),
              HW_AUDIO_TRACE_OK,
              "SOUNDCNT_H is accepted");
    ASSERT_EQ(apply_event(audio, 0, 2, HW_AUDIO_TRACE_WRITE, 2, 0x04000084, 0x0080, &frame),
              HW_AUDIO_TRACE_OK,
              "SOUNDCNT_X enables PSG");
    ASSERT_EQ(apply_event(audio, 0, 3, HW_AUDIO_TRACE_WRITE, 2, 0x04000062, 0xF080, &frame),
              HW_AUDIO_TRACE_OK,
              "SOUND1CNT_H configures duty and envelope");
    ASSERT_EQ(apply_event(audio, 0, 4, HW_AUDIO_TRACE_WRITE, 2, 0x04000064, 0x87F8, &frame),
              HW_AUDIO_TRACE_OK,
              "SOUND1CNT_X triggers square 1");

    bool audible = false;
    for (uint32_t index = 0; index < 32; index++)
    {
        ASSERT_EQ(apply_event(audio, (uint64_t)index * 512u, index + 5u, HW_AUDIO_TRACE_SAMPLE, 0, 0, 0, &frame),
                  HW_AUDIO_TRACE_OK,
                  "ordered native SAMPLE is accepted");
        ASSERT_EQ(frame.left, frame.right, "centered square trace has equal stereo sides");
        audible |= frame.left != 0;
    }
    ASSERT(audible, "native square trace produces nonzero PCM16");
    hw_audio_destroy(audio);
}

/* Prove FIFO writes drain only on the selected timer and preserve signed bytes. */
static void test_trace_fifo_timer_replay(void)
{
    printf("Testing hw_audio trace: FIFO drains on selected timer...\n");
    HwAudio* audio = hw_audio_create(48000.0f);
    HwAudioNativeFrame frame;
    hw_audio_trace_reset(audio);

    ASSERT_EQ(apply_event(audio, 0, 0, HW_AUDIO_TRACE_WRITE, 2, 0x04000082, 0x0F04, &frame),
              HW_AUDIO_TRACE_OK,
              "DMA A routes both sides and selects timer 1");
    ASSERT_EQ(apply_event(audio, 0, 1, HW_AUDIO_TRACE_WRITE, 4, 0x040000A0, 0x7F0100FF, &frame),
              HW_AUDIO_TRACE_OK,
              "FIFO A accepts one little-endian DMA word");
    ASSERT_EQ(apply_event(audio, 1, 0, HW_AUDIO_TRACE_TIMER, 0, 0, 0, &frame),
              HW_AUDIO_TRACE_OK,
              "unselected timer 0 is accepted");
    ASSERT_EQ(apply_event(audio, 1, 1, HW_AUDIO_TRACE_SAMPLE, 0, 0, 0, &frame),
              HW_AUDIO_TRACE_OK,
              "sample after unselected timer is accepted");
    ASSERT_EQ(frame.left, 0, "unselected timer leaves FIFO A hold silent");
    ASSERT_EQ(frame.right, 0, "unselected timer leaves both sides silent");

    ASSERT_EQ(apply_event(audio, 2, 0, HW_AUDIO_TRACE_TIMER, 0, 0, 1, &frame),
              HW_AUDIO_TRACE_OK,
              "selected timer 1 drains FIFO A");
    ASSERT_EQ(apply_event(audio, 2, 1, HW_AUDIO_TRACE_SAMPLE, 0, 0, 0, &frame),
              HW_AUDIO_TRACE_OK,
              "sample after selected timer is accepted");
    ASSERT_EQ(frame.left, -192, "FIFO byte -1 uses mGBA DirectSound integer scale on left");
    ASSERT_EQ(frame.right, -192, "FIFO byte -1 uses mGBA DirectSound integer scale on right");
    hw_audio_destroy(audio);
}

/* Prove malformed ordering and FIFO overflow fail instead of corrupting a capture. */
static void test_trace_validation_rejects_ambiguous_input(void)
{
    printf("Testing hw_audio trace: invalid ordering and FIFO overflow are rejected...\n");
    HwAudio* audio = hw_audio_create(48000.0f);
    HwAudioNativeFrame frame;
    hw_audio_trace_reset(audio);

    ASSERT_EQ(apply_event(audio, 10, 4, HW_AUDIO_TRACE_SAMPLE, 0, 0, 0, &frame),
              HW_AUDIO_TRACE_OK,
              "first ordered event is accepted");
    ASSERT_EQ(apply_event(audio, 10, 4, HW_AUDIO_TRACE_SAMPLE, 0, 0, 0, &frame),
              HW_AUDIO_TRACE_OUT_OF_ORDER,
              "duplicate same-cycle order is rejected");

    hw_audio_trace_reset(audio);
    for (uint32_t index = 0; index < 8; index++)
    {
        ASSERT_EQ(apply_event(audio, index, 0, HW_AUDIO_TRACE_WRITE, 4, 0x040000A0, index, &frame),
                  HW_AUDIO_TRACE_OK,
                  "32-byte FIFO capacity accepts eight words");
    }
    ASSERT_EQ(apply_event(audio, 8, 0, HW_AUDIO_TRACE_WRITE, 4, 0x040000A0, 8, &frame),
              HW_AUDIO_TRACE_FIFO_OVERFLOW,
              "ninth word is rejected before overwriting unread FIFO data");
    hw_audio_destroy(audio);
}

/* Keep trace coverage grouped behind one runner hook. */
void test_hw_audio_trace_run_all(void)
{
    test_trace_reset_starts_silent();
    test_trace_square_register_replay_is_audible();
    test_trace_fifo_timer_replay();
    test_trace_validation_rejects_ambiguous_input();
}

#ifdef PORYAAAA_HW_AUDIO_TRACE_TEST_MAIN
int tests_run = 0;
int tests_passed = 0;

/* Provide a focused executable when unrelated package fixtures are unavailable. */
int main(void)
{
    test_hw_audio_trace_run_all();
    printf("hw_audio trace: %d/%d tests passed\n", tests_passed, tests_run);
    return tests_passed == tests_run ? 0 : 1;
}
#endif
