#include "legatobend_parser.hpp"

namespace ccomidi_legatobend {

namespace {

auto data_byte_count(std::uint8_t status) -> std::uint8_t
{
    auto high = std::uint8_t(status & 0xF0);
    return (high == 0xC0 || high == 0xD0) ? 1 : 2;
}

auto system_data_byte_count(std::uint8_t status) -> std::uint8_t
{
    if (status == 0xF1 || status == 0xF3) return 1;
    if (status == 0xF2) return 2;
    return 0;
}

void clear_channel_parse(ParserState& state)
{
    state.status = 0;
    state.count = 0;
    state.expected = 0;
}

}  // namespace

auto parse_byte(ParserState& state, std::uint8_t byte) -> MidiMessage
{
    auto out = MidiMessage{};
    if (byte >= 0xF8) {
        out.bytes[0] = byte;
        out.length = 1;
        return out;
    }
    if (state.in_sysex) {
        out.bytes[0] = byte;
        out.length = 1;
        state.in_sysex = byte != 0xF7;
        return out;
    }
    if (byte & 0x80) {
        state.system_status = 0;
        state.system_count = 0;
        state.system_expected = 0;
        if (byte >= 0xF0) {
            clear_channel_parse(state);
            state.in_sysex = byte == 0xF0;
            state.system_expected = system_data_byte_count(byte);
            if (state.system_expected > 0) {
                state.system_status = byte;
                return out;
            }
            out.bytes[0] = byte;
            out.length = 1;
            return out;
        }
        state.status = byte;
        state.count = 0;
        state.expected = data_byte_count(byte);
        return out;
    }
    if (state.system_status != 0) {
        state.data[state.system_count++] = byte;
        if (state.system_count >= state.system_expected) {
            out.bytes[0] = state.system_status;
            out.bytes[1] = state.data[0];
            out.length = 2;
            if (state.system_expected > 1) {
                out.bytes[2] = state.data[1];
                out.length = 3;
            }
            state.system_status = 0;
            state.system_count = 0;
            state.system_expected = 0;
        }
        return out;
    }
    if (state.status == 0) return out;
    if (state.count < state.data.size()) {
        state.data[state.count++] = byte;
    }
    if (state.count >= state.expected) {
        out.bytes[0] = state.status;
        out.bytes[1] = state.data[0];
        out.length = 2;
        if (state.expected > 1) {
            out.bytes[2] = state.data[1];
            out.length = 3;
        }
        state.count = 0;
    }
    return out;
}

}  // namespace ccomidi_legatobend
