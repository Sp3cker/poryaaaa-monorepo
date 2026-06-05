#ifndef PORYAAAA_EXPORT_CAPTURE_H
#define PORYAAAA_EXPORT_CAPTURE_H

#include "recorder/midi_buffer.h"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace ccomidi {

enum class CaptureState {
    Idle,
    PendingExport,
    Exporting,
    Captured,
};

struct ExportCaptureConfig {
    double prebufferBeats = 4.0;
    double exportSpeedMultiplier = 2.0;
    double minDetectBeatDelta = 0.25;
    int requiredFastSamples = 2;
};

class ExportCapture {
public:
    using Clock = std::chrono::steady_clock;
    using TimePoint = Clock::time_point;

    explicit ExportCapture(ExportCaptureConfig config = {});

    void set_tempo(double bpm);
    void record_on(TimePoint now = Clock::now());
    void record_off();
    void finish_export();
    bool beats(double beats, TimePoint now = Clock::now());
    void capture_event(uint8_t status, uint8_t d1, uint8_t d2);
    void clear(TimePoint now = Clock::now());

    CaptureState state() const;
    double current_beat() const;
    std::size_t size() const;
    std::size_t prebuffer_size() const;
    std::vector<MidiEvent> snapshot() const;
    bool dump_to_file(const std::string &path) const;

private:
    void reset_detector(double beats, TimePoint now);
    void begin_export_capture();

    MidiBuffer midiBuffer_;
    MidiBuffer preBuffer_;
    CaptureState state_;
    ExportCaptureConfig config_;
    double currentBeat_;
    double tempoBpm_;
    double lastDetectBeat_;
    TimePoint lastDetectWall_;
    int fastBeatSamples_;
};

} // namespace ccomidi

#endif
