#include "legatobend/legatobend_core.h"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <vector>

using ccomidi::legatobend::BendCurve;
using ccomidi::legatobend::LegatoBendCore;
using ccomidi::legatobend::MidiEvent;

namespace {

int g_testsRun = 0;
int g_testsPassed = 0;

#define ASSERT_EQ(actual, expected, msg)                                       \
  do {                                                                         \
    ++g_testsRun;                                                              \
    if ((actual) != (expected)) {                                              \
      std::fprintf(stderr, "FAIL: %s: expected %d, got %d (line %d)\n", msg,   \
                   static_cast<int>(expected), static_cast<int>(actual),       \
                   __LINE__);                                                  \
    } else {                                                                   \
      ++g_testsPassed;                                                         \
    }                                                                          \
  } while (0)

#define RUN(test)                                                              \
  do {                                                                         \
    test();                                                                    \
  } while (0)

MidiEvent msg(std::uint8_t status, std::uint8_t data1, std::uint8_t data2) {
  return MidiEvent{status, data1, data2};
}

MidiEvent note_on(int note, int velocity = 100, int channel = 0) {
  return msg(static_cast<std::uint8_t>(0x90 | channel),
             static_cast<std::uint8_t>(note),
             static_cast<std::uint8_t>(velocity));
}

MidiEvent note_off(int note, int channel = 0) {
  return msg(static_cast<std::uint8_t>(0x80 | channel),
             static_cast<std::uint8_t>(note), 0);
}

MidiEvent cc(int controller, int value, int channel = 0) {
  return msg(static_cast<std::uint8_t>(0xB0 | channel),
             static_cast<std::uint8_t>(controller),
             static_cast<std::uint8_t>(value));
}

void feed(LegatoBendCore *core, MidiEvent event, std::vector<MidiEvent> *out) {
  core->process(event, out);
}

void advance_ticks(LegatoBendCore *core, int count,
                   std::vector<MidiEvent> *out) {
  for (int i = 0; i < count; ++i)
    core->advance(5.0, out);
}

void assert_events(const std::vector<MidiEvent> &actual,
                   const std::vector<MidiEvent> &expected, const char *label) {
  ASSERT_EQ(actual.size(), expected.size(), label);
  const std::size_t count =
      actual.size() < expected.size() ? actual.size() : expected.size();
  for (std::size_t i = 0; i < count; ++i) {
    ASSERT_EQ(actual[i].status, expected[i].status, label);
    ASSERT_EQ(actual[i].data1, expected[i].data1, label);
    ASSERT_EQ(actual[i].data2, expected[i].data2, label);
  }
}

void test_non_legato_anchor_passes_without_reset() {
  auto core = LegatoBendCore{};
  auto out = std::vector<MidiEvent>{};
  feed(&core, note_on(60), &out);
  feed(&core, note_off(60), &out);
  assert_events(out, {note_on(60), note_off(60)}, "anchor on/off only");
}

void test_short_glide_returns_from_in_progress_bend() {
  auto core = LegatoBendCore{};
  auto out = std::vector<MidiEvent>{};
  feed(&core, cc(0x14, 32), &out);
  feed(&core, note_on(60), &out);
  feed(&core, note_on(64), &out);
  advance_ticks(&core, 6, &out);
  feed(&core, note_off(64), &out);
  advance_ticks(&core, 16, &out);
  feed(&core, note_off(60), &out);
  assert_events(out,
                {cc(0x14, 32), note_on(60), msg(0xE0, 0, 65), msg(0xE0, 0, 66),
                 msg(0xE0, 0, 67), msg(0xE0, 0, 66), msg(0xE0, 0, 65),
                 msg(0xE0, 0, 64), note_off(60)},
                "short glide returns before anchor release");
}

void test_anchor_release_during_glide_defers_note_off_until_target_release() {
  auto core = LegatoBendCore{};
  auto out = std::vector<MidiEvent>{};
  feed(&core, cc(0x14, 32), &out);
  feed(&core, note_on(60), &out);
  feed(&core, note_on(64), &out);
  advance_ticks(&core, 4, &out);
  feed(&core, note_off(60), &out);
  feed(&core, note_off(64), &out);
  assert_events(out,
                {cc(0x14, 32), note_on(60), msg(0xE0, 0, 65), msg(0xE0, 0, 66),
                 note_off(60)},
                "target release ends released anchor");
}

void test_retargeted_glide_ignores_older_target_release() {
  auto core = LegatoBendCore{};
  auto out = std::vector<MidiEvent>{};
  feed(&core, cc(0x14, 32), &out);
  feed(&core, note_on(60), &out);
  feed(&core, note_on(64), &out);
  advance_ticks(&core, 4, &out);
  feed(&core, note_on(67), &out);
  feed(&core, note_off(64), &out);
  feed(&core, note_off(60), &out);
  advance_ticks(&core, 16, &out);
  feed(&core, note_off(67), &out);
  assert_events(out,
                {cc(0x14, 32), note_on(60), msg(0xE0, 0, 65), msg(0xE0, 0, 66),
                 msg(0xE0, 0, 67), msg(0xE0, 0, 68), msg(0xE0, 0, 69),
                 msg(0xE0, 0, 70), msg(0xE0, 0, 71), msg(0xE0, 0, 72),
                 msg(0xE0, 0, 73), msg(0xE0, 0, 74), msg(0xE0, 0, 75),
                 msg(0xE0, 0, 76), msg(0xE0, 0, 77), msg(0xE0, 0, 78),
                 note_off(60)},
                "latest target owns retargeted phrase");
}

void test_bend_range_cc_scales_target_to_mp2k_semitones() {
  auto core = LegatoBendCore{};
  auto out = std::vector<MidiEvent>{};
  feed(&core, cc(0x14, 32), &out);
  feed(&core, note_on(60), &out);
  feed(&core, note_on(61), &out);
  advance_ticks(&core, 16, &out);
  feed(&core, note_off(61), &out);
  feed(&core, note_off(60), &out);
  assert_events(out,
                {cc(0x14, 32), note_on(60), msg(0xE0, 0, 65), msg(0xE0, 0, 66),
                 note_off(60), msg(0xE0, 0, 64)},
                "BENDR 32 maps one semitone to two pitch bend units");
}

void test_easing_curve_reaches_same_target() {
  auto core = LegatoBendCore{};
  auto out = std::vector<MidiEvent>{};
  core.set_bend_curve(BendCurve::Easing);
  feed(&core, cc(0x14, 32), &out);
  feed(&core, note_on(60), &out);
  feed(&core, note_on(64), &out);
  advance_ticks(&core, 16, &out);
  ASSERT_EQ(out.back().data2, 72, "easing reaches the same target");
}

void test_zero_bend_time_passes_notes_through() {
  auto core = LegatoBendCore{};
  auto out = std::vector<MidiEvent>{};
  core.set_bend_time_ms(0.0);
  feed(&core, note_on(60), &out);
  feed(&core, note_on(64), &out);
  feed(&core, note_off(64), &out);
  feed(&core, note_off(60), &out);
  assert_events(out, {note_on(60), note_on(64), note_off(64), note_off(60)},
                "disabled core passes through");
}

} // namespace

int main() {
  RUN(test_non_legato_anchor_passes_without_reset);
  RUN(test_short_glide_returns_from_in_progress_bend);
  RUN(test_anchor_release_during_glide_defers_note_off_until_target_release);
  RUN(test_retargeted_glide_ignores_older_target_release);
  RUN(test_bend_range_cc_scales_target_to_mp2k_semitones);
  RUN(test_easing_curve_reaches_same_target);
  RUN(test_zero_bend_time_passes_notes_through);

  std::printf("[ccomidi_legatobend_core] %d/%d passed\n", g_testsPassed,
              g_testsRun);
  return g_testsPassed == g_testsRun ? EXIT_SUCCESS : EXIT_FAILURE;
}
