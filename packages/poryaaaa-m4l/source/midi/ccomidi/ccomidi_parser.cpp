#include "ccomidi_parser.h"

namespace ccomidi {

ParserOutput parse_byte(ParserState &s, std::uint8_t byte)
{
    ParserOutput out;

    if (byte >= 0xF0) {
        out.bytes[0] = byte;
        out.length   = 1;
        return out;
    }

    if (byte & 0x80) {
        s.status   = byte;
        s.count    = 0;
        s.expected = (std::uint8_t)data_byte_count(byte);
        return out;
    }

    if (s.status == 0) return out;
    if (s.count < 2) {
        s.data[s.count++] = byte;
    }
    if (s.count >= s.expected) {
        out.bytes[0] = s.status;
        out.bytes[1] = s.data[0];
        out.length   = 2;
        if (s.expected > 1) {
            out.bytes[2] = s.data[1];
            out.length   = 3;
        }
        s.count = 0;
    }
    return out;
}

}  // namespace ccomidi
