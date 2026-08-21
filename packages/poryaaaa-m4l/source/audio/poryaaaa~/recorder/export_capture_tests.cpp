#include "recorder/export_capture.h"
#include "recorder/midi_buffer.h"

#include <chrono>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

using ccomidi::CaptureState;
using ccomidi::ExportCapture;
using ccomidi::MidiBuffer;
using ccomidi::MidiEvent;

namespace
{

int failures = 0;

std::ostream& operator<<(std::ostream& os, CaptureState state)
{
    switch (state)
    {
    case CaptureState::Idle:
        return os << "Idle";
    case CaptureState::Exporting:
        return os << "Exporting";
    case CaptureState::Captured:
        return os << "Captured";
    }
    return os << "Unknown";
}

void fail(const char* expr, const char* file, int line, const std::string& msg = "")
{
    std::cerr << file << ":" << line << ": failed: " << expr;
    if (!msg.empty())
    {
        std::cerr << " (" << msg << ")";
    }
    std::cerr << "\n";
    ++failures;
}

#define CHECK(expr)                                                                                                    \
    do                                                                                                                 \
    {                                                                                                                  \
        if (!(expr))                                                                                                   \
            fail(#expr, __FILE__, __LINE__);                                                                           \
    } while (0)
#define CHECK_MSG(expr, msg)                                                                                           \
    do                                                                                                                 \
    {                                                                                                                  \
        if (!(expr))                                                                                                   \
            fail(#expr, __FILE__, __LINE__, (msg));                                                                    \
    } while (0)

template <typename A, typename B>
void check_eq(
    const A& actual, const B& expected, const char* actualExpr, const char* expectedExpr, const char* file, int line)
{
    if (!(actual == expected))
    {
        std::ostringstream msg;
        msg << actualExpr << "=" << actual << ", " << expectedExpr << "=" << expected;
        fail("equality", file, line, msg.str());
    }
}

#define CHECK_EQ(actual, expected) check_eq((actual), (expected), #actual, #expected, __FILE__, __LINE__)

void check_event(const MidiEvent& event, double beats, uint8_t status, uint8_t d1, uint8_t d2)
{
    CHECK(std::fabs(event.beats - beats) < 0.000001);
    CHECK_EQ(event.status, status);
    CHECK_EQ(event.d1, d1);
    CHECK_EQ(event.d2, d2);
}

ExportCapture::TimePoint at_ms(int ms)
{
    return ExportCapture::TimePoint(std::chrono::milliseconds(ms));
}

void test_midi_buffer_prune_keeps_boundary()
{
    MidiBuffer buffer;
    buffer.push(0.99, 0x90, 60, 100);
    buffer.push(1.00, 0x90, 61, 100);
    buffer.push(1.25, 0x80, 60, 0);

    buffer.prune_before(1.00);

    auto events = buffer.snapshot();
    CHECK_EQ(events.size(), static_cast<std::size_t>(2));
    check_event(events[0], 1.00, 0x90, 61, 100);
    check_event(events[1], 1.25, 0x80, 60, 0);
}

void test_midi_buffer_append_from_snapshot_is_independent()
{
    MidiBuffer source;
    MidiBuffer dest;
    source.push(2.0, 0x90, 64, 127);
    dest.push(1.0, 0x90, 60, 90);

    dest.append_from(source);
    source.reset();
    source.push(3.0, 0x90, 67, 100);

    auto events = dest.snapshot();
    CHECK_EQ(events.size(), static_cast<std::size_t>(2));
    check_event(events[0], 1.0, 0x90, 60, 90);
    check_event(events[1], 2.0, 0x90, 64, 127);
}

void test_midi_buffer_self_append_duplicates_without_deadlock()
{
    MidiBuffer buffer;
    buffer.push(1.0, 0x90, 60, 100);
    buffer.push(2.0, 0x80, 60, 0);

    buffer.append_from(buffer);

    auto events = buffer.snapshot();
    CHECK_EQ(events.size(), static_cast<std::size_t>(4));
    check_event(events[0], 1.0, 0x90, 60, 100);
    check_event(events[1], 2.0, 0x80, 60, 0);
    check_event(events[2], 1.0, 0x90, 60, 100);
    check_event(events[3], 2.0, 0x80, 60, 0);
}

void test_record_on_starts_capture_immediately()
{
    ExportCapture capture;
    capture.beats(0.0);
    capture.record_on(at_ms(0));

    CHECK_EQ(capture.state(), CaptureState::Exporting);
    capture.capture_event(0x90, 60, 100);
    CHECK_EQ(capture.state(), CaptureState::Exporting);

    auto events = capture.snapshot();
    CHECK_EQ(events.size(), static_cast<std::size_t>(1));
    check_event(events[0], 0.0, 0x90, 60, 100);
}

void test_record_off_resets_export_state_and_buffer()
{
    ExportCapture capture;
    capture.beats(0.0);
    capture.record_on(at_ms(0));
    capture.capture_event(0x90, 60, 100);
    CHECK_EQ(capture.size(), static_cast<std::size_t>(1));

    capture.record_off();
    CHECK_EQ(capture.state(), CaptureState::Idle);
    CHECK_EQ(capture.size(), static_cast<std::size_t>(0));
    capture.beats(1.5);
    capture.capture_event(0x90, 61, 100);
    CHECK_EQ(capture.size(), static_cast<std::size_t>(0));
}

void test_finish_export_freezes_buffer_for_dump()
{
    ExportCapture capture;
    capture.beats(0.0);
    capture.record_on(at_ms(0));
    capture.capture_event(0x90, 60, 100);
    CHECK_EQ(capture.size(), static_cast<std::size_t>(1));

    capture.finish_export();
    CHECK_EQ(capture.state(), CaptureState::Captured);
    capture.beats(1.5);
    capture.capture_event(0x90, 61, 100);
    CHECK_EQ(capture.size(), static_cast<std::size_t>(1));
}

void test_rearming_clears_previous_capture()
{
    ExportCapture capture;
    capture.beats(0.0);
    capture.record_on(at_ms(0));
    capture.capture_event(0x90, 60, 100);
    CHECK_EQ(capture.size(), static_cast<std::size_t>(1));

    capture.record_on(at_ms(300));
    CHECK_EQ(capture.state(), CaptureState::Exporting);
    CHECK_EQ(capture.size(), static_cast<std::size_t>(0));
}

// Recording again after toggling off must not inherit the prior take's timestamp.
void test_record_toggle_resets_latched_beat()
{
    ExportCapture capture;
    capture.beats(42.5);
    capture.record_on(at_ms(0));
    capture.capture_event(0x90, 60, 100);
    auto firstTake = capture.snapshot();
    CHECK_EQ(firstTake.size(), static_cast<std::size_t>(1));
    check_event(firstTake[0], 0.0, 0x90, 60, 100);
    capture.record_off();
    CHECK_EQ(capture.state(), CaptureState::Idle);
    CHECK_EQ(capture.size(), static_cast<std::size_t>(0));
    CHECK_EQ(capture.current_beat(), 0.0);

    capture.record_on(at_ms(300));
    capture.capture_event(0x90, 61, 100);
    auto events = capture.snapshot();
    CHECK_EQ(events.size(), static_cast<std::size_t>(1));
    check_event(events[0], 0.0, 0x90, 61, 100);
}

} // namespace

int main()
{
    test_midi_buffer_prune_keeps_boundary();
    test_midi_buffer_append_from_snapshot_is_independent();
    test_midi_buffer_self_append_duplicates_without_deadlock();
    test_record_on_starts_capture_immediately();
    test_record_off_resets_export_state_and_buffer();
    test_finish_export_freezes_buffer_for_dump();
    test_rearming_clears_previous_capture();
    test_record_toggle_resets_latched_beat();

    if (failures != 0)
    {
        std::cerr << failures << " recorder test failure(s)\n";
        return EXIT_FAILURE;
    }

    std::cout << "porya_recorder_tests: all tests passed\n";
    return EXIT_SUCCESS;
}
