#include "hw_audio/hw_audio_trace.h"
#include "hw_audio/hw_audio_trace_text.h"
#include "test_assert.h"

#include <stdbool.h>
#include <stdio.h>

typedef struct
{
    HwAudio* audio;
    HwAudioNativeFrame last_frame;
    size_t applied_events;
    HwAudioTraceStatus status;
} TraceReplayFixture;

static bool replay_trace_record(void* context, const HwAudioTraceTextRecord* record)
{
    TraceReplayFixture* fixture = context;
    if (record->kind != HW_AUDIO_TRACE_TEXT_EVENT)
        return true;
    fixture->status = hw_audio_trace_apply(fixture->audio, &record->event, &fixture->last_frame);
    if (fixture->status != HW_AUDIO_TRACE_OK)
        return false;
    ++fixture->applied_events;
    return true;
}

static HwAudioTraceTextStatus
parse_trace_fixture(const char* text, HwAudioTraceTextVisitor visitor, void* context, unsigned* error_line)
{
    FILE* input = tmpfile();
    ASSERT(input != NULL, "temporary trace fixture opens");
    if (!input)
        return HW_AUDIO_TRACE_TEXT_READ_FAILED;
    ASSERT(fputs(text, input) >= 0, "temporary trace fixture is written");
    rewind(input);
    HwAudioTraceTextStatus status = hw_audio_trace_text_read(input, visitor, context, error_line);
    ASSERT_EQ(fclose(input), 0, "temporary trace fixture closes");
    return status;
}

/* The trace tool's one retained smoke parses canonical text then replays a
 * WRITE plus SAMPLE through its trace-enabled HwAudio variant. */
static void test_trace_text_write_sample_round_trip(void)
{
    printf("Testing trace tool smoke: WRITE and SAMPLE parser/replay round trip...\n");
    TraceReplayFixture fixture = {0};
    fixture.audio = hw_audio_create(65536.0f);
    ASSERT(fixture.audio != NULL, "trace HwAudio allocation succeeds");
    if (!fixture.audio)
        return;
    hw_audio_trace_reset(fixture.audio);

    unsigned error_line = 0;
    const char* trace = "PORYAAAA_AUDIO_TRACE 1\n"
                        "CLOCK 16777216\n"
                        "BEGIN 0 0\n"
                        "WRITE 1 0 2 0x04000084 0x00000080\n"
                        "SAMPLE 128 0\n"
                        "END 129 0\n";
    ASSERT_EQ(parse_trace_fixture(trace, replay_trace_record, &fixture, &error_line),
              HW_AUDIO_TRACE_TEXT_OK,
              "canonical WRITE and SAMPLE trace parses and replays");
    ASSERT_EQ(fixture.status, HW_AUDIO_TRACE_OK, "replay accepts every parsed event");
    ASSERT_EQ((int)fixture.applied_events, 2, "replay receives the WRITE and SAMPLE events");
    ASSERT_EQ((int)fixture.last_frame.cycle, 128, "replayed SAMPLE retains its absolute cycle");
    hw_audio_destroy(fixture.audio);
}

static void test_trace_text_rejects_signed_and_overflowing_numbers(void)
{
    printf("Testing trace tool smoke: checked unsigned text numbers...\n");
    unsigned error_line = 0;
    ASSERT_EQ(parse_trace_fixture("PORYAAAA_AUDIO_TRACE 1\n"
                                  "CLOCK 16777216\n"
                                  "BEGIN 0 0\n"
                                  "SAMPLE 0 -1\n"
                                  "END 1 0\n",
                                  NULL,
                                  NULL,
                                  &error_line),
              HW_AUDIO_TRACE_TEXT_INVALID_EVENT,
              "signed trace order is rejected");
    ASSERT_EQ(error_line, 4, "signed trace order rejection identifies its line");
    ASSERT_EQ(parse_trace_fixture("PORYAAAA_AUDIO_TRACE 1\n"
                                  "CLOCK 16777216\n"
                                  "BEGIN 0 0\n"
                                  "SAMPLE 18446744073709551616 0\n"
                                  "END 1 0\n",
                                  NULL,
                                  NULL,
                                  &error_line),
              HW_AUDIO_TRACE_TEXT_INVALID_EVENT,
              "overflowing trace cycle is rejected");
    ASSERT_EQ(error_line, 4, "overflowing trace cycle rejection identifies its line");
}

/* Keep the opt-in parser/replay coverage grouped behind one runner hook. */
void test_hw_audio_trace_run_all(void)
{
    test_trace_text_write_sample_round_trip();
    test_trace_text_rejects_signed_and_overflowing_numbers();
}

#ifdef PORYAAAA_HW_AUDIO_TRACE_TEST_MAIN
int tests_run = 0;
int tests_passed = 0;

int main(void)
{
    test_hw_audio_trace_run_all();
    printf("hw_audio trace tool smoke: %d/%d tests passed\n", tests_passed, tests_run);
    return tests_passed == tests_run ? 0 : 1;
}
#endif
