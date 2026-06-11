#pragma once

#include <array>
#include <cstdint>

namespace ccomidi_legatobend {

struct MidiMessage {
    std::array<std::uint8_t, 3> bytes = {0, 0, 0};
    std::uint8_t length = 0;
};

struct ParserState {
    std::uint8_t status = 0;
    std::array<std::uint8_t, 2> data = {0, 0};
    std::uint8_t count = 0;
    std::uint8_t expected = 0;
    std::uint8_t system_status = 0;
    std::uint8_t system_count = 0;
    std::uint8_t system_expected = 0;
    bool in_sysex = false;
};

MidiMessage parse_byte(ParserState& state, std::uint8_t byte);

}  // namespace ccomidi_legatobend
