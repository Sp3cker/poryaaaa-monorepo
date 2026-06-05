#ifndef CCOMIDI_MIDI_BUFFER_H
#define CCOMIDI_MIDI_BUFFER_H

// Replaces RecorderCore. Strictly smaller: just a vector of
// {beats, status, d1, d2} stamped at MIDI-arrival time with whatever
// totalised-beats value the audio/scheduler thread last latched from
// plugsync~. It stores raw parsed channel-voice events, including program
// changes, but no tempo, sample rate, loop, voicemap, or separate PC state.
// File IO writes a fixed-layout little-endian binary blob that [v8]
// parses at Save time. JS owns ticks/SMF/tempo/loop/validation.

#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

namespace ccomidi {

struct MidiEvent {
    double  beats;    // from plugsync~ "Ticks (1 PPQ)" outlet; integer part = beat
    uint8_t status;
    uint8_t d1;
    uint8_t d2;
};

class MidiBuffer {
public:
    void reset();
    void push(double beats, uint8_t status, uint8_t d1, uint8_t d2);
    void push(const MidiEvent &event);
    void append_from(const MidiBuffer &other);
    void prune_before(double minBeat);
    std::size_t size() const;
    std::vector<MidiEvent> snapshot() const;

    // Writes the buffer to `path` as a fixed-layout little-endian binary file.
    // Format (PRBY v1):
    //   offset  size  field
    //   ------  ----  -----
    //   0       4     magic     = "PRBY"  (0x59 0x42 0x52 0x50 in file bytes)
    //   4       2     version   = 1
    //   6       2     reserved  = 0
    //   8       8     count     = uint64 LE
    //   16      ...   events    = count * 12 bytes:
    //                              double beats (8) ; u8 status ; u8 d1 ; u8 d2 ; u8 _pad
    // Returns true on success.
    bool dump_to_file(const std::string &path) const;

private:
    mutable std::mutex     mutex_;
    std::vector<MidiEvent> events_;
};

} // namespace ccomidi

#endif
