#include "recorder/midi_buffer.h"

#include <algorithm>
#include <cstdio>
#include <cstring>

namespace ccomidi {

void MidiBuffer::reset() {
    std::lock_guard<std::mutex> lock(mutex_);
    events_.clear();
}

void MidiBuffer::push(double beats, uint8_t status, uint8_t d1, uint8_t d2) {
    push({beats, status, d1, d2});
}

void MidiBuffer::push(const MidiEvent &event) {
    std::lock_guard<std::mutex> lock(mutex_);
    events_.push_back(event);
}

void MidiBuffer::append_from(const MidiBuffer &other) {
    std::vector<MidiEvent> events = other.snapshot();
    std::lock_guard<std::mutex> lock(mutex_);
    events_.insert(events_.end(), events.begin(), events.end());
}

void MidiBuffer::prune_before(double minBeat) {
    std::lock_guard<std::mutex> lock(mutex_);
    events_.erase(
        std::remove_if(events_.begin(), events_.end(),
                       [minBeat](const MidiEvent &event) {
                           return event.beats < minBeat;
                       }),
        events_.end());
}

std::size_t MidiBuffer::size() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return events_.size();
}

std::vector<MidiEvent> MidiBuffer::snapshot() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return events_;
}

bool MidiBuffer::dump_to_file(const std::string &path) const {
    std::lock_guard<std::mutex> lock(mutex_);

    std::FILE *fp = std::fopen(path.c_str(), "wb");
    if (!fp) return false;

    // Header: "PRBY" magic, u16 version=1, u16 reserved=0, u64 count.
    // Little-endian write; on macOS / x86_64 / arm64 the host already is LE,
    // but spell it out so byte order isn't host-dependent.
    auto write_le_u16 = [&](std::uint16_t v) {
        std::uint8_t b[2] = {
            static_cast<std::uint8_t>(v & 0xFF),
            static_cast<std::uint8_t>((v >> 8) & 0xFF),
        };
        std::fwrite(b, 1, 2, fp);
    };
    auto write_le_u64 = [&](std::uint64_t v) {
        std::uint8_t b[8];
        for (int i = 0; i < 8; ++i) {
            b[i] = static_cast<std::uint8_t>((v >> (8 * i)) & 0xFF);
        }
        std::fwrite(b, 1, 8, fp);
    };

    const char magic[4] = {'P', 'R', 'B', 'Y'};
    std::fwrite(magic, 1, 4, fp);
    write_le_u16(1);   // version
    write_le_u16(0);   // reserved
    write_le_u64(static_cast<std::uint64_t>(events_.size()));

    // Per-event record: double beats (8) + status (1) + d1 (1) + d2 (1) + pad (1) = 12.
    // double is IEEE-754 binary64 little-endian on every Max-supported host
    // (x86_64, arm64 macOS, x64 Windows). Memcpy as bytes — no field-by-field
    // serialisation needed.
    for (const auto &e : events_) {
        std::uint8_t rec[12];
        std::memcpy(&rec[0], &e.beats, 8);
        rec[8]  = e.status;
        rec[9]  = e.d1;
        rec[10] = e.d2;
        rec[11] = 0;
        std::fwrite(rec, 1, 12, fp);
    }

    bool ok = std::fflush(fp) == 0;
    std::fclose(fp);
    return ok;
}

} // namespace ccomidi
