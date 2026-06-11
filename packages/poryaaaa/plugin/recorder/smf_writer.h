#ifndef CCOMIDI_SMF_WRITER_H
#define CCOMIDI_SMF_WRITER_H

#include <cstdint>
#include <string>

#include "recorder/recorder_core.h"

namespace ccomidi {

struct SmfWriteOptions {
  std::uint16_t ppq = 96;
  double tempoBpm = 0.0;

  // Explicit initial state for ccomidi/m4a parameters (non-default values).
  // These are emitted at tick 0 on the corresponding channel tracks, before
  // other events. The caller is responsible for only including values that
  // differ from m4a defaults (e.g. BENDR != 2, TUNE != 0, specific programs,
  // initial VOL/PAN if wanted, XCMD state, etc.). This supports the M4L
  // voicemap + initial CCs and the "preserve pre-anchor state at tick 0"
  // requirement.
  struct InitialProgram {
    std::uint8_t channel = 0;
    std::uint8_t program = 0;
  };
  std::vector<InitialProgram> initialPrograms;

  struct InitialCc {
    std::uint8_t channel = 0;
    std::uint8_t cc = 0;
    std::uint8_t value = 0;
  };
  std::vector<InitialCc> initialCcs;
};

bool write_smf1(const std::string &path,
                const RecorderCore::Snapshot &snapshot,
                const SmfWriteOptions &options);

} // namespace ccomidi

#endif
