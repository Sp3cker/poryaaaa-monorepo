#include "recorder/smf_writer.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <unordered_set>
#include <vector>

#include "MidiFile.h"

namespace ccomidi {

namespace {

using uchar = unsigned char;

struct PendingMidiEvent {
  int tick = 0;
  std::uint64_t order = 0;
  MidiRecord record;
  bool coalesces = false;
  std::uint8_t coalesceKind = 0;
  std::uint8_t coalesceData1 = 0;
};



} // namespace

bool write_smf1(const std::string &path,
                const RecorderCore::Snapshot &snapshot,
                const SmfWriteOptions &options) {
  if (options.ppq == 0 || options.tempoBpm <= 0.0)
    return false;

  const std::uint16_t ppq = options.ppq;
  double anchorBeats = 0.0;
  for (const MidiRecord &record : snapshot.midi) {
    if ((record.status & 0xF0) == 0x90 && record.data2 > 0) {
      anchorBeats = std::floor(record.beats);
      break;
    }
  }

  try {
    constexpr int kConductorTrack = 0;

    int channelTrack[16];
    for (int i = 0; i < 16; ++i)
      channelTrack[i] = -1;
    int nextTrack = 1;
    for (const MidiRecord &m : snapshot.midi) {
      const std::uint8_t kind = m.status & 0xF0;
      if (!(kind >= 0x80 && kind <= 0xE0))
        continue;
      const std::uint8_t channel = m.status & 0x0F;
      if (channelTrack[channel] < 0)
        channelTrack[channel] = nextTrack++;
    }
    // Also consider channels that only have explicit initial ccomidi state
    // (e.g. voicemap PC or initial BENDR/TUNE/CCs with no captured events yet).
    // This ensures a music track is created so the tick-0 setup is emitted.
    for (const auto &ip : options.initialPrograms) {
      if (ip.channel < 16 && channelTrack[ip.channel] < 0)
        channelTrack[ip.channel] = nextTrack++;
    }
    for (const auto &ic : options.initialCcs) {
      if (ic.channel < 16 && channelTrack[ic.channel] < 0)
        channelTrack[ic.channel] = nextTrack++;
    }
    // Always keep at least one music track so MidiFile::write never calls
    // back() on an empty event list (UB) when nothing was recorded.
    const int totalTracks = nextTrack > 1 ? nextTrack : 2;

    smf::MidiFile midifile;
    // MidiFile default-constructs with one track.
    midifile.addTracks(totalTracks - 1);
    midifile.setTicksPerQuarterNote(ppq);

    midifile.addTrackName(kConductorTrack, 0, "Conductor");
    midifile.addTimeSignature(kConductorTrack, 0, 4, 4);
    midifile.addTempo(kConductorTrack, 0, options.tempoBpm);

    if (nextTrack == 1) {
      midifile.addTrackName(1, 0, "Music");
    } else {
      for (int ch = 0; ch < 16; ++ch) {
        if (channelTrack[ch] < 0)
          continue;
        char name[16];
        std::snprintf(name, sizeof(name), "Ch %d", ch + 1);
        midifile.addTrackName(channelTrack[ch], 0, name);
      }
    }

    std::vector<PendingMidiEvent> pendingEvents[16];
    std::uint64_t eventOrder = 0;

    // Helper to decide if this event type coalesces to latest value per (tick, kind, data1)
    auto coalesces = [](const MidiRecord &rec) {
      const std::uint8_t kind = rec.status & 0xF0;
      if (kind == 0xB0) {
        if ((rec.status & 0xF0) != 0xB0) return false; // redundant but for clarity
        const auto d1 = rec.data1;
        return !(d1 == 0x1D || d1 == 0x1E || d1 == 0x1F);
      }
      return kind == 0xC0 || kind == 0xE0;
    };

    auto append_event = [&](std::vector<PendingMidiEvent> &events, const MidiRecord &rec, int tick) {
      PendingMidiEvent ev;
      ev.tick = tick;
      ev.order = eventOrder++;
      ev.record = rec;
      ev.coalesces = coalesces(rec);
      ev.coalesceKind = rec.status & 0xF0;
      if (ev.coalesceKind == 0xB0)
        ev.coalesceData1 = rec.data1;

      if (ev.coalesces) {
        for (PendingMidiEvent &existing : events) {
          if (!existing.coalesces) continue;
          if (existing.tick == ev.tick &&
              existing.coalesceKind == ev.coalesceKind &&
              existing.coalesceData1 == ev.coalesceData1) {
            existing = ev;
            return;
          }
        }
      }
      events.push_back(ev);
    };

    // Inject any explicit initial ccomidi parameters (PC, BENDR, TUNE, other
    // non-default CCs, etc.) at tick 0. These get the earliest orders so they
    // lead any other tick-0 events. Captured pre-anchor events for the same
    // param will replace via the coalescing logic below.
    for (const auto &ip : options.initialPrograms) {
      if (ip.channel >= 16 || ip.program > 127) continue;
      if (channelTrack[ip.channel] < 0) continue;
      MidiRecord rec{};
      rec.status = 0xC0 | ip.channel;
      rec.data1 = ip.program;
      rec.data2 = 0;
      append_event(pendingEvents[ip.channel], rec, /*tick=*/0);
    }
    for (const auto &ic : options.initialCcs) {
      if (ic.channel >= 16 || ic.cc > 127 || ic.value > 127) continue;
      if (channelTrack[ic.channel] < 0) continue;
      MidiRecord rec{};
      rec.status = 0xB0 | ic.channel;
      rec.data1 = ic.cc;
      rec.data2 = ic.value;
      append_event(pendingEvents[ic.channel], rec, /*tick=*/0);
    }

    for (const MidiRecord &m : snapshot.midi) {
      const std::uint8_t kind = m.status & 0xF0;
      if (!(kind >= 0x80 && kind <= 0xE0))
        continue;
      const std::uint8_t channel = m.status & 0x0F;
      if (channelTrack[channel] < 0)
        continue;

      int tick;
      const bool is_note = (kind == 0x80 || kind == 0x90);
      if (is_note) {
        const double rel = m.beats - anchorBeats;
        const double t = (rel <= 0.0) ? 0.0 : rel * static_cast<double>(ppq);
        tick = static_cast<int>(std::floor(t + 0.5));
      } else {
        const double rel = m.beats - anchorBeats;
        const double t = (rel <= 0.0) ? 0.0 : rel * static_cast<double>(ppq);
        tick = static_cast<int>(std::floor(t + 1.0e-9));
      }
      if ((kind == 0xB0 || kind == 0xC0 || kind == 0xE0) && !is_note) {
        tick = tick - (tick % 4);
      }

      append_event(pendingEvents[channel], m, tick);
    }

    for (auto &events : pendingEvents) {
      std::stable_sort(events.begin(), events.end(),
                       [](const PendingMidiEvent &a, const PendingMidiEvent &b) {
                         if (a.tick != b.tick)
                           return a.tick < b.tick;
                         return a.order < b.order;
                       });
    }

    int lastTickPerChannel[16] = {0};
    std::unordered_set<std::uint16_t> heldNotes;
    for (int channel = 0; channel < 16; ++channel) {
      const int track = channelTrack[channel];
      if (track < 0)
        continue;

      for (const PendingMidiEvent &event : pendingEvents[channel]) {
        const MidiRecord &m = event.record;
        std::vector<uchar> bytes;
        bytes.push_back(static_cast<uchar>(m.status));
        bytes.push_back(static_cast<uchar>(m.data1));
        const std::uint8_t k = m.status & 0xF0;
        if (!(k == 0xC0 || k == 0xD0))
          bytes.push_back(static_cast<uchar>(m.data2));
        midifile.addEvent(track, event.tick, bytes);
        if (event.tick > lastTickPerChannel[channel])
          lastTickPerChannel[channel] = event.tick;

        const std::uint8_t kind = m.status & 0xF0;
        const std::uint16_t key =
            static_cast<std::uint16_t>((channel << 8) | m.data1);
        if (kind == 0x90 && m.data2 > 0)
          heldNotes.insert(key);
        else if (kind == 0x80 || (kind == 0x90 && m.data2 == 0))
          heldNotes.erase(key);
      }
    }

    for (std::uint16_t key : heldNotes) {
      const std::uint8_t channel = static_cast<std::uint8_t>((key >> 8) & 0x0F);
      const std::uint8_t note = static_cast<std::uint8_t>(key & 0x7F);
      const int track = channelTrack[channel];
      if (track < 0)
        continue;
      // Place synthetic release on the 4-grid after the channel's last event.
      // Using +1 (old) produced off-grid ticks; mid2agb's integer scaling of
      // durations would then truncate, causing playback/rhythm errors in
      // the generated GBA M4A data. Compute a clean later grid point.
      int last = lastTickPerChannel[channel];
      constexpr int g = 4;
      int releaseTick = ((last / g) + 1) * g;
      if (releaseTick <= last)
        releaseTick = last + g;
      std::vector<uchar> bytes;
      bytes.push_back(static_cast<uchar>(0x80 | channel));
      bytes.push_back(static_cast<uchar>(note));
      bytes.push_back(0);
      midifile.addEvent(track, releaseTick, bytes);
    }

    // Keep music tracks in append order so same-tick XCMD byte pairs are not
    // reordered by controller number.
    midifile.sortTrackNoteOnsBeforeOffs(kConductorTrack);
    return midifile.write(path);
  } catch (...) {
    return false;
  }
}

} // namespace ccomidi
