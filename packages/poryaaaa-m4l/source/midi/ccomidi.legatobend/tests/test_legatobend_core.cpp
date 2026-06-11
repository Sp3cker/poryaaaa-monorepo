#include "../legatobend_core.hpp"
#include "../legatobend_parser.hpp"

#include <cstdint>
#include <cstdio>
#include <vector>

using ccomidi_legatobend::LegatoBendCore;
using ccomidi_legatobend::BendCurve;
using ccomidi_legatobend::MidiMessage;
using ccomidi_legatobend::ParserState;
using ccomidi_legatobend::parse_byte;

namespace {

int g_run = 0;
int g_pass = 0;
const char* g_current_test = "(none)";

#define ASSERT_EQ(actual, expected, msg)                                        \
    do {                                                                        \
        ++g_run;                                                                \
        auto _a = (long long)(actual);                                          \
        auto _e = (long long)(expected);                                        \
        if (_a != _e) {                                                         \
            std::fprintf(stderr, "FAIL [%s]: %s: expected %lld, got %lld "      \
                                 "(line %d)\n",                                \
                         g_current_test, msg, _e, _a, __LINE__);                \
        } else {                                                                \
            ++g_pass;                                                           \
        }                                                                       \
    } while (0)

#define RUN(t)                                                                  \
    do {                                                                        \
        g_current_test = #t;                                                     \
        t();                                                                    \
    } while (0)

auto msg(std::uint8_t status, std::uint8_t d1, std::uint8_t d2) -> MidiMessage
{
    return MidiMessage{{status, d1, d2}, 3};
}

auto note_on(int note, int velocity = 100, int channel = 0) -> MidiMessage
{
    return msg(std::uint8_t(0x90 | channel), std::uint8_t(note), std::uint8_t(velocity));
}

auto note_off(int note, int channel = 0) -> MidiMessage
{
    return msg(std::uint8_t(0x80 | channel), std::uint8_t(note), 0);
}

auto cc(int controller, int value, int channel = 0) -> MidiMessage
{
    return msg(std::uint8_t(0xB0 | channel), std::uint8_t(controller), std::uint8_t(value));
}

void feed(LegatoBendCore& core, MidiMessage const& message, std::vector<std::uint8_t>& out)
{
    core.process(message, out);
}

void feed_byte(ParserState& parser, LegatoBendCore& core, std::uint8_t byte,
               std::vector<std::uint8_t>& out)
{
    if (!core.enabled()) {
        out.push_back(byte);
        return;
    }
    core.process(parse_byte(parser, byte), out);
}

void feed_bytes(ParserState& parser, LegatoBendCore& core,
                std::vector<std::uint8_t> const& bytes, std::vector<std::uint8_t>& out)
{
    for (auto byte : bytes) {
        feed_byte(parser, core, byte, out);
    }
}

void advance_ticks(LegatoBendCore& core, int ticks, std::vector<std::uint8_t>& out)
{
    for (auto i = 0; i < ticks; ++i) {
        core.advance(5.0, out);
    }
}

void assert_bytes(std::vector<std::uint8_t> const& actual,
                  std::vector<std::uint8_t> const& expected, const char* msg)
{
    ASSERT_EQ(actual.size(), expected.size(), msg);
    if (actual.size() != expected.size()) return;
    for (auto i = std::size_t{0}; i < expected.size(); ++i) {
        ASSERT_EQ(actual[i], expected[i], msg);
    }
}

void test_disabled_mode_passes_messages_through()
{
    auto core = LegatoBendCore{};
    auto parser = ParserState{};
    auto out = std::vector<std::uint8_t>{};
    core.set_bend_time_ms(0);
    auto input = std::vector<std::uint8_t>{0x90, 60, 100, 60, 0};
    feed_bytes(parser, core, input, out);
    assert_bytes(out, input, "disabled byte stream passes through exactly");
}

void test_non_legato_anchor_passes_without_reset()
{
    auto core = LegatoBendCore{};
    auto out = std::vector<std::uint8_t>{};
    feed(core, note_on(60), out);
    feed(core, note_off(60), out);
    assert_bytes(out, {0x90, 60, 100, 0x80, 60, 0}, "anchor on/off only");
}

void test_short_glide_returns_from_in_progress_bend()
{
    auto core = LegatoBendCore{};
    auto out = std::vector<std::uint8_t>{};
    feed(core, cc(0x14, 32), out);
    feed(core, note_on(60), out);
    feed(core, note_on(64), out);
    advance_ticks(core, 6, out);
    feed(core, note_off(64), out);
    advance_ticks(core, 16, out);
    feed(core, note_off(60), out);
    assert_bytes(out,
                 {0xB0, 0x14, 32,
                  0x90, 60, 100,
                  0xE0, 0, 65,
                  0xE0, 0, 66,
                  0xE0, 0, 67,
                  0xE0, 0, 66,
                  0xE0, 0, 65,
                  0xE0, 0, 64,
                  0x80, 60, 0},
                 "short glide returns before anchor release");
}

void test_anchor_release_during_glide_defers_note_off_until_target_release()
{
    auto core = LegatoBendCore{};
    auto out = std::vector<std::uint8_t>{};
    feed(core, cc(0x14, 32), out);
    feed(core, note_on(60), out);
    feed(core, note_on(64), out);
    advance_ticks(core, 4, out);
    feed(core, note_off(60), out);
    feed(core, note_off(64), out);
    assert_bytes(out,
                 {0xB0, 0x14, 32,
                  0x90, 60, 100,
                  0xE0, 0, 65,
                  0xE0, 0, 66,
                  0x80, 60, 0},
                 "target release ends released anchor");
}

void test_retargeted_glide_ignores_older_target_release()
{
    auto core = LegatoBendCore{};
    auto out = std::vector<std::uint8_t>{};
    feed(core, cc(0x14, 32), out);
    feed(core, note_on(60), out);
    feed(core, note_on(64), out);
    advance_ticks(core, 4, out);
    feed(core, note_on(67), out);
    feed(core, note_off(64), out);
    feed(core, note_off(60), out);
    advance_ticks(core, 16, out);
    feed(core, note_off(67), out);
    assert_bytes(out,
                 {0xB0, 0x14, 32,
                  0x90, 60, 100,
                  0xE0, 0, 65,
                  0xE0, 0, 66,
                  0xE0, 0, 67,
                  0xE0, 0, 68,
                  0xE0, 0, 69,
                  0xE0, 0, 70,
                  0xE0, 0, 71,
                  0xE0, 0, 72,
                  0xE0, 0, 73,
                  0xE0, 0, 74,
                  0xE0, 0, 75,
                  0xE0, 0, 76,
                  0xE0, 0, 77,
                  0xE0, 0, 78,
                  0x80, 60, 0},
                 "latest target owns retargeted phrase");
}

void test_released_anchor_resets_bend_before_next_phrase()
{
    auto core = LegatoBendCore{};
    auto out = std::vector<std::uint8_t>{};
    feed(core, cc(0x14, 32), out);
    feed(core, note_on(60), out);
    feed(core, note_on(64), out);
    advance_ticks(core, 4, out);
    feed(core, note_off(60), out);
    feed(core, note_off(64), out);
    feed(core, note_on(72), out);
    assert_bytes(out,
                 {0xB0, 0x14, 32,
                  0x90, 60, 100,
                  0xE0, 0, 65,
                  0xE0, 0, 66,
                  0x80, 60, 0,
                  0xE0, 0, 64,
                  0x90, 72, 100},
                 "released anchor leaves bend for release tail until next phrase");
}

void test_bend_range_cc_scales_target_to_mp2k_semitones()
{
    auto core = LegatoBendCore{};
    auto out = std::vector<std::uint8_t>{};
    feed(core, cc(0x14, 32), out);
    feed(core, note_on(60), out);
    feed(core, note_on(61), out);
    advance_ticks(core, 16, out);
    feed(core, note_off(61), out);
    feed(core, note_off(60), out);
    assert_bytes(out,
                 {0xB0, 0x14, 32,
                  0x90, 60, 100,
                  0xE0, 0, 65,
                  0xE0, 0, 66,
                  0x80, 60, 0,
                  0xE0, 0, 64},
                 "BENDR 32 maps one semitone to two bend units");
}

void test_easing_curve_shapes_ramp_and_keeps_endpoint()
{
    auto core = LegatoBendCore{};
    auto out = std::vector<std::uint8_t>{};
    core.set_bend_curve(BendCurve::Easing);
    feed(core, cc(0x14, 16), out);
    feed(core, note_on(60), out);
    feed(core, note_on(64), out);
    advance_ticks(core, 16, out);
    assert_bytes(out,
                 {0xB0, 0x14, 16,
                  0x90, 60, 100,
                  0xE0, 0, 65,
                  0xE0, 0, 67,
                  0xE0, 0, 68,
                  0xE0, 0, 69,
                  0xE0, 0, 71,
                  0xE0, 0, 72,
                  0xE0, 0, 73,
                  0xE0, 0, 75,
                  0xE0, 0, 76,
                  0xE0, 0, 78,
                  0xE0, 0, 79,
                  0xE0, 0, 80},
                 "easing curve reaches same target with eased steps");
}

void test_note_on_velocity_zero_passthrough_is_not_normalized()
{
    auto core = LegatoBendCore{};
    auto out = std::vector<std::uint8_t>{};
    feed(core, note_on(60), out);
    feed(core, msg(0x90, 60, 0), out);
    assert_bytes(out, {0x90, 60, 100, 0x90, 60, 0}, "note-on-zero is preserved");
}

void test_parser_expands_running_status_when_enabled()
{
    auto core = LegatoBendCore{};
    auto parser = ParserState{};
    auto out = std::vector<std::uint8_t>{};
    feed_bytes(parser, core, {0x90, 60, 100, 60, 0}, out);
    assert_bytes(out, {0x90, 60, 100, 0x90, 60, 0}, "running status parses note-on-zero");
}

void test_parser_preserves_realtime_inside_channel_stream()
{
    auto core = LegatoBendCore{};
    auto parser = ParserState{};
    auto out = std::vector<std::uint8_t>{};
    feed_bytes(parser, core, {0x90, 60, 100, 0xF8, 0x80, 60, 0}, out);
    assert_bytes(out, {0x90, 60, 100, 0xF8, 0x80, 60, 0}, "realtime passthrough keeps phrase state");
}

void test_parser_preserves_sysex_bytes_and_clears_running_status()
{
    auto core = LegatoBendCore{};
    auto parser = ParserState{};
    auto out = std::vector<std::uint8_t>{};
    feed_bytes(parser, core, {0x90, 60, 100, 0xF0, 1, 2, 0xF7, 60, 0}, out);
    assert_bytes(out, {0x90, 60, 100, 0xF0, 1, 2, 0xF7}, "sysex passthrough clears running status");
}

}  // namespace

int main()
{
    RUN(test_disabled_mode_passes_messages_through);
    RUN(test_non_legato_anchor_passes_without_reset);
    RUN(test_short_glide_returns_from_in_progress_bend);
    RUN(test_anchor_release_during_glide_defers_note_off_until_target_release);
    RUN(test_retargeted_glide_ignores_older_target_release);
    RUN(test_released_anchor_resets_bend_before_next_phrase);
    RUN(test_bend_range_cc_scales_target_to_mp2k_semitones);
    RUN(test_easing_curve_shapes_ramp_and_keeps_endpoint);
    RUN(test_note_on_velocity_zero_passthrough_is_not_normalized);
    RUN(test_parser_expands_running_status_when_enabled);
    RUN(test_parser_preserves_realtime_inside_channel_stream);
    RUN(test_parser_preserves_sysex_bytes_and_clears_running_status);

    std::fprintf(stderr, "[ccomidi_legatobend] %d/%d passed\n", g_pass, g_run);
    return (g_pass == g_run) ? 0 : 1;
}
