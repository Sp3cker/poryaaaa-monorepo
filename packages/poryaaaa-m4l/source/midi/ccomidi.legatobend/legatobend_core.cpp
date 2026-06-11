#include "legatobend_core.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>

namespace ccomidi_legatobend {

namespace {

constexpr auto kNoteOff = std::uint8_t{0x80};
constexpr auto kNoteOn = std::uint8_t{0x90};
constexpr auto kControlChange = std::uint8_t{0xB0};
constexpr auto kPitchBend = std::uint8_t{0xE0};
constexpr auto kCcBendRange = std::uint8_t{0x14};
constexpr auto kNeutralBend = 64;

auto clamp_data_byte(int value) -> int
{
    return std::clamp(value, 0, 127);
}

auto message_channel(MidiMessage const& message) -> std::uint8_t
{
    return std::uint8_t(message.bytes[0] & 0x0F);
}

auto message_type(MidiMessage const& message) -> std::uint8_t
{
    return std::uint8_t(message.bytes[0] & 0xF0);
}

auto is_note_off(MidiMessage const& message) -> bool
{
    auto type = message_type(message);
    return message.length == 3 && (type == kNoteOff || (type == kNoteOn && message.bytes[2] == 0));
}

auto is_note_on(MidiMessage const& message) -> bool
{
    return message.length == 3 && message_type(message) == kNoteOn && message.bytes[2] > 0;
}

auto is_bend_range(MidiMessage const& message) -> bool
{
    return message.length == 3 && message_type(message) == kControlChange
           && message.bytes[1] == kCcBendRange;
}

}  // namespace

void LegatoBendCore::set_bend_time_ms(long value)
{
    bend_time_ms_ = std::max(0L, value);
}

void LegatoBendCore::set_bend_curve(BendCurve curve)
{
    bend_curve_ = curve;
}

void LegatoBendCore::reset()
{
    channels_ = {};
}

auto LegatoBendCore::enabled() const -> bool
{
    return bend_time_ms_ > 0;
}

auto LegatoBendCore::has_active_ramp() const -> bool
{
    return std::any_of(channels_.begin(), channels_.end(), [](Channel const& channel) {
        return channel.ramp.active;
    });
}

void LegatoBendCore::process(MidiMessage const& message, std::vector<std::uint8_t>& out)
{
    if (message.length == 0) return;
    if (!enabled()) {
        append_message(message, out);
        return;
    }
    if (is_bend_range(message)) {
        auto midi_channel = message_channel(message);
        channels_[midi_channel].bend_range = message.bytes[2];
        append_message(message, out);
        return;
    }
    if (!is_note_on(message) && !is_note_off(message)) {
        append_message(message, out);
        return;
    }
    auto midi_channel = message_channel(message);
    auto& channel = channels_[midi_channel];
    if (is_note_on(message)) {
        process_note_on(channel, midi_channel, message, out);
    } else {
        process_note_off(channel, midi_channel, message, out);
    }
}

void LegatoBendCore::advance(double elapsed_ms, std::vector<std::uint8_t>& out)
{
    assert(elapsed_ms >= 0.0);
    if (elapsed_ms <= 0.0) return;
    for (auto midi_channel = std::uint8_t{0}; midi_channel < channels_.size(); ++midi_channel) {
        auto& channel = channels_[midi_channel];
        if (!channel.ramp.active) continue;
        auto& ramp = channel.ramp;
        ramp.elapsed_ms = std::min(ramp.duration_ms, ramp.elapsed_ms + elapsed_ms);
        auto t = ramp.duration_ms > 0.0 ? ramp.elapsed_ms / ramp.duration_ms : 1.0;
        t = apply_curve(ramp.curve, t);
        channel.current_bend = ramp.from + ((ramp.to - ramp.from) * t);
        auto bend = rounded_bend(channel.current_bend);
        if (bend != channel.last_emitted_bend) {
            append_pitch_bend(midi_channel, bend, out);
            channel.last_emitted_bend = bend;
        }
        if (ramp.elapsed_ms >= ramp.duration_ms) {
            channel.current_bend = ramp.to;
            channel.ramp.active = false;
        }
    }
}

void LegatoBendCore::process_note_on(Channel& channel, std::uint8_t midi_channel,
                                     MidiMessage const& message, std::vector<std::uint8_t>& out)
{
    auto note = message.bytes[1];
    if (!channel.active) {
        append_reset_if_needed(channel, midi_channel, out);
        reset_phrase(channel);
        channel.active = true;
        channel.anchor_held = true;
        channel.anchor_note = note;
        append_message(message, out);
        return;
    }
    channel.target_note = note;
    start_ramp(channel, midi_channel, note);
}

void LegatoBendCore::process_note_off(Channel& channel, std::uint8_t midi_channel,
                                      MidiMessage const& message, std::vector<std::uint8_t>& out)
{
    if (!channel.active) {
        append_message(message, out);
        return;
    }
    auto note = message.bytes[1];
    if (note == channel.anchor_note) {
        channel.anchor_held = false;
        if (channel.target_note >= 0) return;
        finish_phrase_with_input_note_off(channel, midi_channel, message, out);
        return;
    }
    if (channel.target_note >= 0 && note == channel.target_note) {
        channel.target_note = -1;
        if (channel.anchor_held) {
            start_ramp(channel, midi_channel, channel.anchor_note);
        } else {
            finish_phrase_with_generated_note_off(channel, midi_channel, out);
        }
    }
}

void LegatoBendCore::start_ramp(Channel& channel, std::uint8_t /*midi_channel*/, int target_note)
{
    assert(enabled());
    channel.ramp.active = true;
    channel.ramp.from = channel.current_bend;
    channel.ramp.to = target_bend_value(channel, target_note);
    channel.ramp.elapsed_ms = 0.0;
    channel.ramp.duration_ms = double(bend_time_ms_);
    channel.ramp.curve = bend_curve_;
}

void LegatoBendCore::finish_phrase_with_input_note_off(Channel& channel,
                                                       std::uint8_t midi_channel,
                                                       MidiMessage const& message,
                                                       std::vector<std::uint8_t>& out)
{
    append_message(message, out);
    append_reset_if_needed(channel, midi_channel, out);
    reset_phrase(channel);
}

void LegatoBendCore::finish_phrase_with_generated_note_off(Channel& channel,
                                                           std::uint8_t midi_channel,
                                                           std::vector<std::uint8_t>& out)
{
    append_note_off(midi_channel, channel.anchor_note, out);
    reset_phrase(channel);
}

void LegatoBendCore::append_reset_if_needed(Channel& channel, std::uint8_t midi_channel,
                                            std::vector<std::uint8_t>& out)
{
    if (channel.last_emitted_bend == kNeutralBend) return;
    append_pitch_bend(midi_channel, kNeutralBend, out);
    channel.current_bend = kNeutralBend;
    channel.last_emitted_bend = kNeutralBend;
    channel.ramp.active = false;
}

void LegatoBendCore::reset_phrase(Channel& channel)
{
    auto bend_range = channel.bend_range;
    auto current_bend = channel.current_bend;
    auto last_emitted_bend = channel.last_emitted_bend;
    channel = Channel{};
    channel.bend_range = bend_range;
    channel.current_bend = current_bend;
    channel.last_emitted_bend = last_emitted_bend;
}

void LegatoBendCore::append_message(MidiMessage const& message, std::vector<std::uint8_t>& out)
{
    for (auto i = std::uint8_t{0}; i < message.length; ++i) {
        out.push_back(message.bytes[i]);
    }
}

void LegatoBendCore::append_note_off(std::uint8_t midi_channel, std::uint8_t note,
                                     std::vector<std::uint8_t>& out)
{
    out.push_back(std::uint8_t(kNoteOff | midi_channel));
    out.push_back(note);
    out.push_back(0);
}

void LegatoBendCore::append_pitch_bend(std::uint8_t midi_channel, int value,
                                       std::vector<std::uint8_t>& out)
{
    out.push_back(std::uint8_t(kPitchBend | midi_channel));
    out.push_back(0);
    out.push_back(std::uint8_t(clamp_data_byte(value)));
}

auto LegatoBendCore::target_bend_value(Channel const& channel, int target_note) -> int
{
    if (channel.bend_range == 0) return kNeutralBend;
    auto semitones = target_note - int(channel.anchor_note);
    auto bend_units = std::lround(double(semitones * 64) / double(channel.bend_range));
    return clamp_data_byte(kNeutralBend + int(bend_units));
}

auto LegatoBendCore::apply_curve(BendCurve curve, double progress) -> double
{
    if (curve == BendCurve::Linear) return progress;
    return progress * progress * (3.0 - (2.0 * progress));
}

auto LegatoBendCore::rounded_bend(double value) -> int
{
    return clamp_data_byte(int(std::lround(value)));
}

}  // namespace ccomidi_legatobend
