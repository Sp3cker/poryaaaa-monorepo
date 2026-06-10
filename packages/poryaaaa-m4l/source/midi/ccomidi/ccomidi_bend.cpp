#include "ccomidi_bend.h"

namespace ccomidi {

namespace {

long clamp_data_byte(long value)
{
    if (value < 0) return 0;
    if (value > 127) return 127;
    return value;
}

}  // namespace

BendBytes encode_raw_bend_value(long value)
{
    return {
        0,
        (std::uint8_t)clamp_data_byte(value),
    };
}

}  // namespace ccomidi
