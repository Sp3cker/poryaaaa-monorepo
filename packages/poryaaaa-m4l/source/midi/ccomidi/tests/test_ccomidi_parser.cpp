/*
 * Unit tests for the ccomidi MIDI byte parser (running-status state machine).
 * No Max SDK linkage — runs as a plain executable.
 */

#include "../ccomidi_parser.h"
#include "../ccomidi_bend.h"

#include <cstdio>
#include <cstdlib>
#include <vector>

using ccomidi::ParserState;
using ccomidi::ParserOutput;
using ccomidi::BendBytes;
using ccomidi::encode_raw_bend_value;
using ccomidi::parse_byte;

namespace {

int g_run = 0;
int g_pass = 0;
const char *g_currentTest = "(none)";

#define ASSERT_EQ(actual, expected, msg)                                       \
    do {                                                                       \
        ++g_run;                                                               \
        long long _a = (long long)(actual);                                    \
        long long _e = (long long)(expected);                                  \
        if (_a != _e) {                                                        \
            std::fprintf(stderr, "FAIL [%s]: %s: expected %lld, got %lld "     \
                                 "(line %d)\n",                                \
                         g_currentTest, msg, _e, _a, __LINE__);                \
        } else {                                                               \
            ++g_pass;                                                          \
        }                                                                      \
    } while (0)

#define RUN(t)                                                                 \
    do {                                                                       \
        g_currentTest = #t;                                                    \
        t();                                                                   \
    } while (0)

/* feed a byte and append everything emitted to `out` */
void feed(ParserState &s, std::uint8_t byte, std::vector<std::uint8_t> &out)
{
    ParserOutput o = parse_byte(s, byte);
    for (std::uint8_t i = 0; i < o.length; ++i) out.push_back(o.bytes[i]);
}

/* ----- channel-voice messages ----- */

void test_note_on_emits_three_bytes_with_original_channel()
{
    ParserState s{};
    std::vector<std::uint8_t> out;
    feed(s, 0x95, out);        /* status — buffered, no emit */
    feed(s, 60,   out);        /* d1 — still buffered */
    feed(s, 100,  out);        /* d2 — emits */
    ASSERT_EQ(out.size(), 3, "note on emits 3 bytes");
    if (out.size() < 3) return;
    ASSERT_EQ(out[0], 0x95, "status preserves incoming channel");
    ASSERT_EQ(out[1], 60,   "key passes through");
    ASSERT_EQ(out[2], 100,  "velocity passes through");
}

void test_running_status_repeats_status_byte()
{
    ParserState s{};
    std::vector<std::uint8_t> out;
    feed(s, 0x93, out);
    feed(s, 60,   out);
    feed(s, 100,  out);        /* first note: 0x93 60 100 */
    feed(s, 62,   out);
    feed(s, 110,  out);        /* second note via running status: 0x93 62 110 */
    ASSERT_EQ(out.size(), 6, "two running-status notes emit 6 bytes");
    if (out.size() < 6) return;
    ASSERT_EQ(out[0], 0x93, "first note status (ch 3)");
    ASSERT_EQ(out[3], 0x93, "second note status repeated (ch 3)");
    ASSERT_EQ(out[4], 62,   "second note key");
    ASSERT_EQ(out[5], 110,  "second note velocity");
}

void test_program_change_uses_one_data_byte()
{
    ParserState s{};
    std::vector<std::uint8_t> out;
    feed(s, 0xC7, out);
    feed(s, 25,   out);        /* PC: emits after 1 data byte */
    ASSERT_EQ(out.size(), 2, "program change emits 2 bytes total");
    if (out.size() < 2) return;
    ASSERT_EQ(out[0], 0xC7, "PC status preserves incoming channel");
    ASSERT_EQ(out[1], 25,   "PC program passes through");
}

void test_channel_aftertouch_uses_one_data_byte()
{
    ParserState s{};
    std::vector<std::uint8_t> out;
    feed(s, 0xD9, out);
    feed(s, 80,   out);
    ASSERT_EQ(out.size(), 2, "channel aftertouch emits 2 bytes");
    if (out.size() < 2) return;
    ASSERT_EQ(out[0], 0xD9, "AT status preserves incoming channel");
    ASSERT_EQ(out[1], 80,   "AT pressure passes through");
}

void test_pitch_bend_emits_three_bytes()
{
    ParserState s{};
    std::vector<std::uint8_t> out;
    feed(s, 0xE2, out);
    feed(s, 0x40, out);
    feed(s, 0x20, out);
    ASSERT_EQ(out.size(), 3, "bend emits 3 bytes");
    if (out.size() < 3) return;
    ASSERT_EQ(out[0], 0xE2, "bend status preserves incoming channel");
    ASSERT_EQ(out[1], 0x40, "bend lo (LSB) passes through");
    ASSERT_EQ(out[2], 0x20, "bend hi (MSB) passes through");
}

void test_cc_emits_three_bytes()
{
    ParserState s{};
    std::vector<std::uint8_t> out;
    feed(s, 0xB1, out);        /* CC, ch 1 */
    feed(s, 7,    out);        /* CC #7 (volume) */
    feed(s, 100,  out);
    ASSERT_EQ(out.size(), 3, "CC emits 3 bytes");
    if (out.size() < 3) return;
    ASSERT_EQ(out[0], 0xB1, "CC status preserves incoming channel");
    ASSERT_EQ(out[1], 7,    "CC# passes through");
    ASSERT_EQ(out[2], 100,  "CC value passes through");
}

void test_xcmd_cc_passes_through_unchanged()
{
    /* XCMD lives on CCs 29/30/31 in this codebase — payload and status must
     * traverse the parser unchanged. */
    ParserState s{};
    std::vector<std::uint8_t> out;
    feed(s, 0xB4, out);
    feed(s, 30,   out);        /* CC #30 — XCMD */
    feed(s, 77,   out);
    ASSERT_EQ(out.size(), 3, "XCMD CC emits 3 bytes");
    if (out.size() < 3) return;
    ASSERT_EQ(out[0], 0xB4, "XCMD status preserved");
    ASSERT_EQ(out[1], 30,   "XCMD CC# preserved");
    ASSERT_EQ(out[2], 77,   "XCMD value preserved");
}

void test_raw_bend_value_encodes_center_at_64()
{
    // The UI bend dial sends 0 to mean "center" (which corresponds to MIDI MSB=64).
    // Negative dial values (<0) must produce MSB <64 (pitch down).
    // Positive values produce MSB >64 (pitch up).
    BendBytes center = encode_raw_bend_value(0);
    ASSERT_EQ(center.lsb, 0, "dial 0 (center) lsb");
    ASSERT_EQ(center.msb, 64, "dial 0 (center) msb == 64");

    BendBytes neg = encode_raw_bend_value(-1);
    ASSERT_EQ(neg.lsb, 0, "dial -1 lsb");
    ASSERT_EQ(neg.msb, 63, "dial -1 < center msb");

    BendBytes pos = encode_raw_bend_value(1);
    ASSERT_EQ(pos.lsb, 0, "dial +1 lsb");
    ASSERT_EQ(pos.msb, 65, "dial +1 > center msb");

    // Full negative and positive of the current dial range
    BendBytes min = encode_raw_bend_value(-64);
    ASSERT_EQ(min.lsb, 0, "dial -64 lsb");
    ASSERT_EQ(min.msb, 0, "dial -64 → full down");

    BendBytes max = encode_raw_bend_value(64);
    ASSERT_EQ(max.lsb, 0, "dial +64 lsb");
    ASSERT_EQ(max.msb, 127, "dial +64 → full up (clamped)");
}

void test_raw_bend_value_clamps()
{
    BendBytes low = encode_raw_bend_value(-100);
    ASSERT_EQ(low.lsb, 0, "way below range lsb");
    ASSERT_EQ(low.msb, 0, "way below range msb");

    BendBytes high = encode_raw_bend_value(100);
    ASSERT_EQ(high.lsb, 0, "way above range lsb");
    ASSERT_EQ(high.msb, 127, "way above range msb (clamped)");
}

/* ----- system common / real-time ----- */

void test_realtime_byte_passes_through_verbatim()
{
    ParserState s{};
    std::vector<std::uint8_t> out;
    feed(s, 0xF8, out);        /* timing clock */
    ASSERT_EQ(out.size(), 1, "realtime byte emits 1 byte");
    if (out.size() < 1) return;
    ASSERT_EQ(out[0], 0xF8, "realtime byte verbatim");
}

void test_realtime_byte_does_not_disturb_running_status()
{
    ParserState s{};
    std::vector<std::uint8_t> out;
    feed(s, 0x96, out);
    feed(s, 60,   out);
    feed(s, 100,  out);        /* first note: 0x96 60 100 */
    feed(s, 0xF8, out);        /* clock interrupts but should not reset state */
    feed(s, 64,   out);
    feed(s, 90,   out);        /* running status: 0x96 64 90 */
    ASSERT_EQ(out.size(), 7, "note + clock + note via running status");
    if (out.size() < 7) return;
    ASSERT_EQ(out[3], 0xF8, "clock byte appears verbatim mid-stream");
    ASSERT_EQ(out[4], 0x96, "running status survives realtime byte");
    ASSERT_EQ(out[5], 64,   "running-status second note key");
    ASSERT_EQ(out[6], 90,   "running-status second note vel");
}

/* ----- malformed input ----- */

void test_orphan_data_byte_is_dropped()
{
    /* data byte arriving before any status byte must be ignored, never emitted */
    ParserState s{};
    std::vector<std::uint8_t> out;
    feed(s, 60, out);
    feed(s, 100, out);
    ASSERT_EQ(out.size(), 0, "orphan data bytes produce no output");
}

void test_status_byte_resets_partial_message()
{
    /* If a new status byte arrives mid-message, the previous partial is
     * abandoned (per MIDI spec) and the new message starts. */
    ParserState s{};
    std::vector<std::uint8_t> out;
    feed(s, 0x92, out);
    feed(s, 60,   out);        /* first data byte buffered */
    feed(s, 0xC2, out);        /* new status — abandon partial note on */
    feed(s, 25,   out);        /* PC completes immediately */
    ASSERT_EQ(out.size(), 2, "only the PC emits, partial note is dropped");
    if (out.size() < 2) return;
    ASSERT_EQ(out[0], 0xC2, "PC status emerged");
    ASSERT_EQ(out[1], 25,   "PC program emerged");
}

}  // namespace

int main()
{
    RUN(test_note_on_emits_three_bytes_with_original_channel);
    RUN(test_running_status_repeats_status_byte);
    RUN(test_program_change_uses_one_data_byte);
    RUN(test_channel_aftertouch_uses_one_data_byte);
    RUN(test_pitch_bend_emits_three_bytes);
    RUN(test_cc_emits_three_bytes);
    RUN(test_xcmd_cc_passes_through_unchanged);
    RUN(test_raw_bend_value_encodes_center_at_64);
    RUN(test_raw_bend_value_clamps);
    RUN(test_realtime_byte_passes_through_verbatim);
    RUN(test_realtime_byte_does_not_disturb_running_status);
    RUN(test_orphan_data_byte_is_dropped);
    RUN(test_status_byte_resets_partial_message);

    std::fprintf(stderr, "[ccomidi_parser] %d/%d passed\n", g_pass, g_run);
    return (g_pass == g_run) ? 0 : 1;
}
