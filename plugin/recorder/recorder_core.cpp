#include "recorder/recorder_core.h"

namespace ccomidi {

void RecorderCore::reset() {
  std::lock_guard<std::mutex> lock(mutex_);
  midi_.clear();
}

void RecorderCore::reserve(std::size_t midiCapacity) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (midiCapacity > midi_.capacity())
    midi_.reserve(midiCapacity);
}

void RecorderCore::push_event_at_beats(double beats,
                                       std::uint8_t status, std::uint8_t data1,
                                       std::uint8_t data2) {
  std::lock_guard<std::mutex> lock(mutex_);
  MidiRecord record = {};
  record.beats = beats;
  record.status = status;
  record.data1 = data1;
  record.data2 = data2;
  midi_.push_back(record);
}

std::size_t RecorderCore::midi_event_count() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return midi_.size();
}

RecorderCore::Snapshot RecorderCore::snapshot() const {
  std::lock_guard<std::mutex> lock(mutex_);
  Snapshot snap;
  snap.midi = midi_;
  return snap;
}

} // namespace ccomidi
