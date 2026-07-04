#include "ccomidi_parser.h"

namespace ccomidi
{

ParserOutput parse_byte(ParserState& s, std::uint8_t byte)
{
    ParserOutput out;

    if (byte >= 0xF0)
    {
        out.bytes[0] = byte;
        out.length = 1;
        return out;
    }

    if (byte & 0x80)
    {
        s.status = byte;
        s.count = 0;
        s.expected = (std::uint8_t)data_byte_count(byte);
        return out;
    }

    if (s.status == 0)
        return out;
    if (s.count < 2)
    {
        s.data[s.count++] = byte;
    }
    if (s.count >= s.expected)
    {
        out.bytes[0] = s.status;
        out.bytes[1] = s.data[0];
        out.length = 2;
        if (s.expected > 1)
        {
            out.bytes[2] = s.data[1];
            out.length = 3;
        }
        s.count = 0;
    }
    return out;
}

bool should_emit_startup_snapshot(StartupSnapshotState& s, const ParserOutput& out)
{
    /* The fallback must run on a complete channel-voice message, not on raw
     * status/data bytes or realtime clock bytes, so the setup snapshot is
     * emitted immediately before real musical MIDI enters poryaaaa~. */
    if (s.primed || out.length == 0)
        return false;

    std::uint8_t status = out.bytes[0];
    if (status < 0x80 || status >= 0xF0)
        return false;

    s.primed = true;
    return true;
}

void reset_startup_snapshot(StartupSnapshotState& s)
{
    s.primed = false;
}

} // namespace ccomidi
