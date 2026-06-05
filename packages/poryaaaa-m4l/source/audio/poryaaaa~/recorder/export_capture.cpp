#include "recorder/export_capture.h"

#include <algorithm>

namespace ccomidi {

ExportCapture::ExportCapture(ExportCaptureConfig config)
    : state_(CaptureState::Idle),
      config_(config),
      currentBeat_(0.0),
      tempoBpm_(120.0),
      lastDetectBeat_(0.0),
      lastDetectWall_(Clock::now()),
      fastBeatSamples_(0) {}

void ExportCapture::set_tempo(double bpm) {
    tempoBpm_ = std::max(1.0, bpm);
}

void ExportCapture::record_on(TimePoint now) {
    midiBuffer_.reset();
    preBuffer_.reset();
    state_ = CaptureState::PendingExport;
    reset_detector(currentBeat_, now);
}

void ExportCapture::record_off() {
    clear();
}

void ExportCapture::finish_export() {
    if (state_ == CaptureState::Exporting) {
        state_ = CaptureState::Captured;
    }
}

bool ExportCapture::beats(double beats, TimePoint now) {
    currentBeat_ = beats;

    if (state_ != CaptureState::PendingExport) {
        return false;
    }

    double beatDelta = beats - lastDetectBeat_;
    double wallSeconds =
        std::chrono::duration<double>(now - lastDetectWall_).count();

    if (beatDelta <= 0.0) {
        reset_detector(beats, now);
        return false;
    }

    if (beatDelta < config_.minDetectBeatDelta || wallSeconds <= 0.0) {
        return false;
    }

    double observedBps = beatDelta / wallSeconds;
    double expectedBps = tempoBpm_ / 60.0;
    if (observedBps > expectedBps * config_.exportSpeedMultiplier) {
        fastBeatSamples_ += 1;
        if (fastBeatSamples_ >= config_.requiredFastSamples) {
            begin_export_capture();
            return true;
        }
    } else {
        fastBeatSamples_ = 0;
    }

    lastDetectBeat_ = beats;
    lastDetectWall_ = now;
    return false;
}

void ExportCapture::capture_event(uint8_t status, uint8_t d1, uint8_t d2) {
    MidiEvent event{
        currentBeat_,
        status,
        static_cast<uint8_t>(d1 & 0x7F),
        static_cast<uint8_t>(d2 & 0x7F),
    };

    switch (state_) {
        case CaptureState::PendingExport:
            preBuffer_.push(event);
            preBuffer_.prune_before(event.beats - config_.prebufferBeats);
            break;
        case CaptureState::Exporting:
            midiBuffer_.push(event);
            break;
        case CaptureState::Idle:
        case CaptureState::Captured:
            break;
    }
}

void ExportCapture::clear(TimePoint now) {
    midiBuffer_.reset();
    preBuffer_.reset();
    state_ = CaptureState::Idle;
    reset_detector(currentBeat_, now);
}

CaptureState ExportCapture::state() const {
    return state_;
}

double ExportCapture::current_beat() const {
    return currentBeat_;
}

std::size_t ExportCapture::size() const {
    return midiBuffer_.size();
}

std::size_t ExportCapture::prebuffer_size() const {
    return preBuffer_.size();
}

std::vector<MidiEvent> ExportCapture::snapshot() const {
    return midiBuffer_.snapshot();
}

bool ExportCapture::dump_to_file(const std::string &path) const {
    return midiBuffer_.dump_to_file(path);
}

void ExportCapture::reset_detector(double beats, TimePoint now) {
    lastDetectBeat_ = beats;
    lastDetectWall_ = now;
    fastBeatSamples_ = 0;
}

void ExportCapture::begin_export_capture() {
    midiBuffer_.reset();
    midiBuffer_.append_from(preBuffer_);
    state_ = CaptureState::Exporting;
}

} // namespace ccomidi
