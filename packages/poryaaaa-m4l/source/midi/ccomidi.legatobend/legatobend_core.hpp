#pragma once

#include "legatobend_parser.hpp"

#include <array>
#include <cstdint>
#include <vector>

namespace ccomidi_legatobend {

enum class BendCurve {
    Linear,
    Easing,
};

class LegatoBendCore {
public:
    void set_bend_time_ms(long value);
    void set_bend_curve(BendCurve curve);
    void reset();
    void process(MidiMessage const& message, std::vector<std::uint8_t>& out);
    void advance(double elapsed_ms, std::vector<std::uint8_t>& out);

    [[nodiscard]] auto enabled() const -> bool;
    [[nodiscard]] auto has_active_ramp() const -> bool;

private:
    struct Ramp {
        bool active = false;
        double from = 64.0;
        double to = 64.0;
        double elapsed_ms = 0.0;
        double duration_ms = 80.0;
        BendCurve curve = BendCurve::Linear;
    };

    struct Channel {
        bool active = false;
        bool anchor_held = false;
        std::uint8_t anchor_note = 0;
        int target_note = -1;
        int bend_range = 2;
        double current_bend = 64.0;
        int last_emitted_bend = 64;
        Ramp ramp;
    };

    void process_note_on(Channel& channel, std::uint8_t midi_channel,
                         MidiMessage const& message, std::vector<std::uint8_t>& out);
    void process_note_off(Channel& channel, std::uint8_t midi_channel,
                          MidiMessage const& message, std::vector<std::uint8_t>& out);
    void start_ramp(Channel& channel, std::uint8_t midi_channel, int target_note);
    void finish_phrase_with_input_note_off(Channel& channel, std::uint8_t midi_channel,
                                           MidiMessage const& message,
                                           std::vector<std::uint8_t>& out);
    void finish_phrase_with_generated_note_off(Channel& channel, std::uint8_t midi_channel,
                                               std::vector<std::uint8_t>& out);
    void append_reset_if_needed(Channel& channel, std::uint8_t midi_channel,
                                std::vector<std::uint8_t>& out);
    static void reset_phrase(Channel& channel);

    static void append_message(MidiMessage const& message, std::vector<std::uint8_t>& out);
    static void append_note_off(std::uint8_t midi_channel, std::uint8_t note,
                                std::vector<std::uint8_t>& out);
    static void append_pitch_bend(std::uint8_t midi_channel, int value,
                                  std::vector<std::uint8_t>& out);
    static auto target_bend_value(Channel const& channel, int target_note) -> int;
    static auto apply_curve(BendCurve curve, double progress) -> double;
    static auto rounded_bend(double value) -> int;

    std::array<Channel, 16> channels_ = {};
    long bend_time_ms_ = 80;
    BendCurve bend_curve_ = BendCurve::Linear;
};

}  // namespace ccomidi_legatobend
