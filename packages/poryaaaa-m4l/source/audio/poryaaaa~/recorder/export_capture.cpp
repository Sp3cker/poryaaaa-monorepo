#include "recorder/export_capture.h"

namespace ccomidi
{

void ExportCapture::record_on(TimePoint)
{
    midiBuffer_.reset();
    state_ = CaptureState::Exporting;
}

void ExportCapture::record_off()
{
    clear();
}

void ExportCapture::finish_export()
{
    if (state_ == CaptureState::Exporting)
    {
        state_ = CaptureState::Captured;
    }
}

void ExportCapture::beats(double beats)
{
    currentBeat_ = beats;
}

void ExportCapture::capture_event(uint8_t status, uint8_t d1, uint8_t d2)
{
    MidiEvent event{
        currentBeat_,
        status,
        static_cast<uint8_t>(d1 & 0x7F),
        static_cast<uint8_t>(d2 & 0x7F),
    };

    switch (state_)
    {
    case CaptureState::Exporting:
        midiBuffer_.push(event);
        break;
    case CaptureState::Idle:
    case CaptureState::Captured:
        break;
    }
}

void ExportCapture::clear(TimePoint)
{
    midiBuffer_.reset();
    state_ = CaptureState::Idle;
}

CaptureState ExportCapture::state() const
{
    return state_;
}

double ExportCapture::current_beat() const
{
    return currentBeat_;
}

std::size_t ExportCapture::size() const
{
    return midiBuffer_.size();
}

std::vector<MidiEvent> ExportCapture::snapshot() const
{
    return midiBuffer_.snapshot();
}

bool ExportCapture::dump_to_file(const std::string& path) const
{
    return midiBuffer_.dump_to_file(path);
}

} // namespace ccomidi
