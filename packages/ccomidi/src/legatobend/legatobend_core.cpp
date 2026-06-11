#include "legatobend/legatobend_core.h"

#include <algorithm>
#include <cassert>
#include <cmath>

namespace ccomidi::legatobend {

namespace {

constexpr auto kNoteOff = std::uint8_t{0x80};
constexpr auto kNoteOn = std::uint8_t{0x90};
constexpr auto kControlChange = std::uint8_t{0xB0};
constexpr auto kPitchBend = std::uint8_t{0xE0};
constexpr auto kCcBendRange = std::uint8_t{0x14};
constexpr auto kNeutralBend = 64;

int clamp_data_byte(int value) { return std::clamp(value, 0, 127); }

std::uint8_t message_channel(const MidiEvent &event) {
  return static_cast<std::uint8_t>(event.status & 0x0F);
}

std::uint8_t message_type(const MidiEvent &event) {
  return static_cast<std::uint8_t>(event.status & 0xF0);
}

bool is_note_off(const MidiEvent &event) {
  const std::uint8_t type = message_type(event);
  return type == kNoteOff || (type == kNoteOn && event.data2 == 0);
}

bool is_note_on(const MidiEvent &event) {
  return message_type(event) == kNoteOn && event.data2 > 0;
}

bool is_bend_range(const MidiEvent &event) {
  return message_type(event) == kControlChange && event.data1 == kCcBendRange;
}

} // namespace

void LegatoBendCore::reset() { channels_ = {}; }

void LegatoBendCore::set_bend_time_ms(double value) {
  bendTimeMs_ = std::max(0.0, value);
}

void LegatoBendCore::set_bend_curve(BendCurve curve) { bendCurve_ = curve; }

void LegatoBendCore::process(const MidiEvent &event,
                             std::vector<MidiEvent> *out) {
  if (!out)
    return;
  if (!enabled()) {
    out->push_back(event);
    return;
  }
  const std::uint8_t midiChannel = message_channel(event);
  auto &channel = channels_[midiChannel];
  if (is_bend_range(event)) {
    channel.bendRange = event.data2;
    out->push_back(event);
    return;
  }
  if (!is_note_on(event) && !is_note_off(event)) {
    out->push_back(event);
    return;
  }
  if (is_note_on(event)) {
    process_note_on(&channel, midiChannel, event, out);
  } else {
    process_note_off(&channel, midiChannel, event, out);
  }
}

void LegatoBendCore::advance(double elapsedMs, std::vector<MidiEvent> *out) {
  assert(elapsedMs >= 0.0);
  if (!out || elapsedMs <= 0.0)
    return;
  for (auto midiChannel = std::uint8_t{0}; midiChannel < channels_.size();
       ++midiChannel) {
    auto &channel = channels_[midiChannel];
    if (!channel.ramp.active)
      continue;
    auto &ramp = channel.ramp;
    ramp.elapsedMs = std::min(ramp.durationMs, ramp.elapsedMs + elapsedMs);
    double progress =
        ramp.durationMs > 0.0 ? ramp.elapsedMs / ramp.durationMs : 1.0;
    progress = apply_curve(ramp.curve, progress);
    channel.currentBend = ramp.from + ((ramp.to - ramp.from) * progress);
    const int bend = rounded_bend(channel.currentBend);
    if (bend != channel.lastEmittedBend) {
      append_pitch_bend(midiChannel, bend, out);
      channel.lastEmittedBend = bend;
    }
    if (ramp.elapsedMs >= ramp.durationMs) {
      channel.currentBend = ramp.to;
      channel.ramp.active = false;
    }
  }
}

double LegatoBendCore::bend_time_ms() const { return bendTimeMs_; }

BendCurve LegatoBendCore::bend_curve() const { return bendCurve_; }

bool LegatoBendCore::enabled() const { return bendTimeMs_ > 0.0; }

bool LegatoBendCore::has_active_ramp() const {
  return std::any_of(
      channels_.begin(), channels_.end(),
      [](const Channel &channel) { return channel.ramp.active; });
}

void LegatoBendCore::process_note_on(Channel *channel, std::uint8_t midiChannel,
                                     const MidiEvent &event,
                                     std::vector<MidiEvent> *out) {
  if (!channel->active) {
    append_reset_if_needed(channel, midiChannel, out);
    reset_phrase(channel);
    channel->active = true;
    channel->anchorHeld = true;
    channel->anchorNote = event.data1;
    out->push_back(event);
    return;
  }
  channel->targetNote = event.data1;
  start_ramp(channel, event.data1);
}

void LegatoBendCore::process_note_off(Channel *channel,
                                      std::uint8_t midiChannel,
                                      const MidiEvent &event,
                                      std::vector<MidiEvent> *out) {
  if (!channel->active) {
    out->push_back(event);
    return;
  }
  const std::uint8_t note = event.data1;
  if (note == channel->anchorNote) {
    channel->anchorHeld = false;
    if (channel->targetNote >= 0)
      return;
    finish_phrase_with_input_note_off(channel, midiChannel, event, out);
    return;
  }
  if (channel->targetNote >= 0 && note == channel->targetNote) {
    channel->targetNote = -1;
    if (channel->anchorHeld) {
      start_ramp(channel, channel->anchorNote);
    } else {
      finish_phrase_with_generated_note_off(channel, midiChannel, out);
    }
  }
}

void LegatoBendCore::start_ramp(Channel *channel, int targetNote) {
  assert(enabled());
  channel->ramp.active = true;
  channel->ramp.from = channel->currentBend;
  channel->ramp.to = target_bend_value(*channel, targetNote);
  channel->ramp.elapsedMs = 0.0;
  channel->ramp.durationMs = bendTimeMs_;
  channel->ramp.curve = bendCurve_;
}

void LegatoBendCore::finish_phrase_with_input_note_off(
    Channel *channel, std::uint8_t midiChannel, const MidiEvent &event,
    std::vector<MidiEvent> *out) {
  out->push_back(event);
  append_reset_if_needed(channel, midiChannel, out);
  reset_phrase(channel);
}

void LegatoBendCore::finish_phrase_with_generated_note_off(
    Channel *channel, std::uint8_t midiChannel, std::vector<MidiEvent> *out) {
  out->push_back(MidiEvent{static_cast<std::uint8_t>(kNoteOff | midiChannel),
                           channel->anchorNote, 0});
  reset_phrase(channel);
}

void LegatoBendCore::append_reset_if_needed(Channel *channel,
                                            std::uint8_t midiChannel,
                                            std::vector<MidiEvent> *out) {
  if (channel->lastEmittedBend == kNeutralBend)
    return;
  append_pitch_bend(midiChannel, kNeutralBend, out);
  channel->currentBend = kNeutralBend;
  channel->lastEmittedBend = kNeutralBend;
  channel->ramp.active = false;
}

void LegatoBendCore::reset_phrase(Channel *channel) {
  const int bendRange = channel->bendRange;
  const double currentBend = channel->currentBend;
  const int lastEmittedBend = channel->lastEmittedBend;
  *channel = Channel{};
  channel->bendRange = bendRange;
  channel->currentBend = currentBend;
  channel->lastEmittedBend = lastEmittedBend;
}

void LegatoBendCore::append_pitch_bend(std::uint8_t midiChannel, int value,
                                       std::vector<MidiEvent> *out) {
  out->push_back(MidiEvent{static_cast<std::uint8_t>(kPitchBend | midiChannel),
                           0,
                           static_cast<std::uint8_t>(clamp_data_byte(value))});
}

int LegatoBendCore::target_bend_value(const Channel &channel, int targetNote) {
  if (channel.bendRange == 0)
    return kNeutralBend;
  const int semitones = targetNote - static_cast<int>(channel.anchorNote);
  const auto bendUnits = static_cast<int>(
      std::lround(double(semitones * 64) / double(channel.bendRange)));
  return clamp_data_byte(kNeutralBend + bendUnits);
}

double LegatoBendCore::apply_curve(BendCurve curve, double progress) {
  if (curve == BendCurve::Linear)
    return progress;
  return progress * progress * (3.0 - (2.0 * progress));
}

int LegatoBendCore::rounded_bend(double value) {
  return clamp_data_byte(static_cast<int>(std::lround(value)));
}

} // namespace ccomidi::legatobend
