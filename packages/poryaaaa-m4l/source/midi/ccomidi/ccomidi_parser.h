#pragma once

#include <cstdint>

/*
 * Pure-C++ MIDI byte parser used by the ccomidi Max external.
 *
 * Decoupled from the Max SDK so it can be unit-tested without launching Max.
 * The Max wrapper feeds bytes from [midiin] one at a time; this returns the
 * (up to 3) bytes that should be emitted to [midiout] for each input byte.
 *
 * Channel-voice messages (0x80..0xEF) preserve their incoming status byte.
 * System common / real-time bytes (>= 0xF0) pass through verbatim and do not
 * disturb running-status state.
 */

namespace ccomidi {

struct ParserState {
    std::uint8_t status   = 0;  /* current running status, 0 = none */
    std::uint8_t data[2]  = {0, 0};
    std::uint8_t count    = 0;  /* data bytes accumulated for current message */
    std::uint8_t expected = 0;  /* data bytes expected for current message */
};

struct ParserOutput {
    std::uint8_t bytes[3] = {0, 0, 0};
    std::uint8_t length   = 0;  /* 0..3 */
};

inline int data_byte_count(std::uint8_t status)
{
    std::uint8_t high = status & 0xF0;
    return (high == 0xC0 || high == 0xD0) ? 1 : 2;
}

ParserOutput parse_byte(ParserState &s, std::uint8_t byte);

}  // namespace ccomidi
