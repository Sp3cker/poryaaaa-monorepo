#ifndef CCOMIDI_LEGATOBEND_PARAM_IDS_H
#define CCOMIDI_LEGATOBEND_PARAM_IDS_H

#include <cstdint>

#include <clap/id.h>

namespace ccomidi::legatobend {

constexpr clap_id kParamBendTimeMs = 0;
constexpr clap_id kParamBendCurve = 1;
constexpr std::uint32_t kParamCount = 2;
constexpr double kDefaultBendTimeMs = 80.0;
constexpr double kMaxBendTimeMs = 2000.0;

} // namespace ccomidi::legatobend

#endif
