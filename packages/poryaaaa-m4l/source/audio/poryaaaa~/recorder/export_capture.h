#ifndef PORYAAAA_EXPORT_CAPTURE_H
#define PORYAAAA_EXPORT_CAPTURE_H

#include "recorder/midi_buffer.h"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace ccomidi
{

enum class CaptureState
{
    Idle,
    Exporting,
    Captured,
};

class ExportCapture
{
public:
    using Clock = std::chrono::steady_clock;
    using TimePoint = Clock::time_point;

    ExportCapture() = default;

    void record_on(TimePoint now = Clock::now());
    void record_off();
    void finish_export();
    void beats(double beats);
    void capture_event(uint8_t status, uint8_t d1, uint8_t d2);
    void clear(TimePoint now = Clock::now());

    CaptureState state() const;
    double current_beat() const;
    std::size_t size() const;
    std::vector<MidiEvent> snapshot() const;
    bool dump_to_file(const std::string& path) const;

private:
    MidiBuffer midiBuffer_;
    CaptureState state_ = CaptureState::Idle;
    double currentBeat_ = 0.0;
};

} // namespace ccomidi

#endif
