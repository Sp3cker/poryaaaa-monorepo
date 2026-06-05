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

bool is_channel_voice_status(std::uint8_t status) {
  const std::uint8_t kind = status & 0xF0;
  return kind >= 0x80 && kind <= 0xE0;
}

bool is_single_data_byte_status(std::uint8_t status) {
  const std::uint8_t kind = status & 0xF0;
  return kind == 0xC0 || kind == 0xD0;
}

bool is_note_on(const MidiRecord &record) {
  return (record.status & 0xF0) == 0x90 && record.data2 > 0;
}

bool is_note_status(std::uint8_t status) {
  const std::uint8_t kind = status & 0xF0;
  return kind == 0x80 || kind == 0x90;
}

bool is_xcmd_cc(const MidiRecord &record) {
  if ((record.status & 0xF0) != 0xB0)
    return false;
  return record.data1 == 0x1D || record.data1 == 0x1E ||
         record.data1 == 0x1F;
}

bool uses_coarse_grid(const MidiRecord &record) {
  const std::uint8_t kind = record.status & 0xF0;
  return kind == 0xB0 || kind == 0xC0 || kind == 0xE0;
}

bool uses_latest_value_per_cell(const MidiRecord &record) {
  const std::uint8_t kind = record.status & 0xF0;
  if (kind == 0xB0)
    return !is_xcmd_cc(record);
  return kind == 0xC0 || kind == 0xE0;
}

double anchor_beats_for_snapshot(const RecorderCore::Snapshot &snapshot) {
  for (const MidiRecord &record : snapshot.midi) {
    if (is_note_on(record))
      return std::floor(record.beats);
  }
  return 0.0;
}

double ticks_for_beats(double beats, double anchorBeats, std::uint16_t ppq) {
  const double relativeBeats = beats - anchorBeats;
  if (relativeBeats <= 0.0)
    return 0.0;
  return relativeBeats * static_cast<double>(ppq);
}

int floored_tick_for_beats(double beats, double anchorBeats, std::uint16_t ppq) {
  return static_cast<int>(std::floor(ticks_for_beats(beats, anchorBeats, ppq) +
                                     1.0e-9));
}

int nearest_tick_for_beats(double beats, double anchorBeats, std::uint16_t ppq) {
  return static_cast<int>(std::floor(ticks_for_beats(beats, anchorBeats, ppq) +
                                     0.5));
}

int coarse_grid_tick(int tick) {
  constexpr int kCoarseGridTicks = 4;
  return tick - tick % kCoarseGridTicks;
}

PendingMidiEvent make_pending_event(const MidiRecord &record, int tick,
                                    std::uint64_t order) {
  PendingMidiEvent event;
  event.tick = tick;
  event.order = order;
  event.record = record;
  event.coalesces = uses_latest_value_per_cell(record);
  event.coalesceKind = record.status & 0xF0;
  if (event.coalesceKind == 0xB0)
    event.coalesceData1 = record.data1;
  return event;
}

void append_pending_event(std::vector<PendingMidiEvent> &events,
                          const PendingMidiEvent &event) {
  if (event.coalesces) {
    for (PendingMidiEvent &existing : events) {
      if (!existing.coalesces)
        continue;
      if (existing.tick == event.tick &&
          existing.coalesceKind == event.coalesceKind &&
          existing.coalesceData1 == event.coalesceData1) {
        existing = event;
        return;
      }
    }
  }
  events.push_back(event);
}

} // namespace

bool write_smf1(const std::string &path,
                const RecorderCore::Snapshot &snapshot,
                const SmfWriteOptions &options) {
  if (options.ppq == 0 || options.tempoBpm <= 0.0)
    return false;

  const std::uint16_t ppq = options.ppq;
  const double anchorBeats = anchor_beats_for_snapshot(snapshot);

  try {
    constexpr int kConductorTrack = 0;

    int channelTrack[16];
    for (int i = 0; i < 16; ++i)
      channelTrack[i] = -1;
    int nextTrack = 1;
    for (const MidiRecord &m : snapshot.midi) {
      if (!is_channel_voice_status(m.status))
        continue;
      const std::uint8_t channel = m.status & 0x0F;
      if (channelTrack[channel] < 0)
        channelTrack[channel] = nextTrack++;
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
    for (const MidiRecord &m : snapshot.midi) {
      if (!is_channel_voice_status(m.status))
        continue;
      const std::uint8_t channel = m.status & 0x0F;
      if (channelTrack[channel] < 0)
        continue;

      int tick = is_note_status(m.status)
          ? nearest_tick_for_beats(m.beats, anchorBeats, ppq)
          : floored_tick_for_beats(m.beats, anchorBeats, ppq);
      if (uses_coarse_grid(m) && !is_note_status(m.status))
        tick = coarse_grid_tick(tick);
      append_pending_event(pendingEvents[channel],
                           make_pending_event(m, tick, eventOrder++));
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
        if (!is_single_data_byte_status(m.status))
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
      const int releaseTick = lastTickPerChannel[channel] + 1;
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
