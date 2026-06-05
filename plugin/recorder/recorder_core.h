#ifndef CCOMIDI_RECORDER_CORE_H
#define CCOMIDI_RECORDER_CORE_H

#include <cstdint>
#include <mutex>
#include <vector>

namespace ccomidi {

struct MidiRecord {
  double beats = 0.0;
  std::uint8_t status = 0;
  std::uint8_t data1 = 0;
  std::uint8_t data2 = 0;
};

class RecorderCore {
public:
  struct Snapshot {
    std::vector<MidiRecord> midi;
  };

  void reset();
  void reserve(std::size_t midiCapacity);

  void push_event_at_beats(double beats, std::uint8_t status,
                           std::uint8_t data1, std::uint8_t data2);

  std::size_t midi_event_count() const;

  Snapshot snapshot() const;

private:
  mutable std::mutex mutex_;
  std::vector<MidiRecord> midi_;
};

} // namespace ccomidi

#endif
