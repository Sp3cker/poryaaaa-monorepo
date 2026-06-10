#ifndef CCOMIDI_BEND_H
#define CCOMIDI_BEND_H

#include <cstdint>

namespace ccomidi {

struct BendBytes {
    std::uint8_t lsb;
    std::uint8_t msb;
};

BendBytes encode_raw_bend_value(long value);

}  // namespace ccomidi

#endif
