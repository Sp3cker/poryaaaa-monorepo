#include "ccomidi_bend.h"

namespace ccomidi {

BendBytes encode_raw_bend_value(long value)
{
    // The bend dial sends 0 to mean center ("64" in the 7-bit MSB sense).
    // We add 64 so dial==0 produces the proper MIDI pitch-bend center (MSB=64, LSB=0).
    // Values below 0 on the dial produce MSB < 64 (downward bend / pitch down).
    // This is the convention used by the current Pitch Bend dial (-64..+64 range).
    long msb = value + 64;
    if (msb < 0) msb = 0;
    if (msb > 127) msb = 127;

    return {
        0,                    // LSB is always 0 (we use MSB only for this control)
        (std::uint8_t)msb,
    };
}

}  // namespace ccomidi
