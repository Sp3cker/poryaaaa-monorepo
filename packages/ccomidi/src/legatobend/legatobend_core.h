#ifndef CCOMIDI_LEGATOBEND_CORE_H
#define CCOMIDI_LEGATOBEND_CORE_H

#include <array>
#include <cstdint>
#include <vector>

namespace ccomidi::legatobend {

enum class BendCurve {
  Linear = 0,
  Easing = 1,
};

struct MidiEvent {
  std::uint8_t status = 0;
  std::uint8_t data1 = 0;
  std::uint8_t data2 = 0;
};

class LegatoBendCore {
public:
  void reset();
  void set_bend_time_ms(double value);
  void set_bend_curve(BendCurve curve);
  void process(const MidiEvent &event, std::vector<MidiEvent> *out);
  void advance(double elapsedMs, std::vector<MidiEvent> *out);

  double bend_time_ms() const;
  BendCurve bend_curve() const;
  bool enabled() const;
  bool has_active_ramp() const;

private:
  struct Ramp {
    bool active = false;
    double from = 64.0;
    double to = 64.0;
    double elapsedMs = 0.0;
    double durationMs = 80.0;
    BendCurve curve = BendCurve::Linear;
  };

  struct Channel {
    bool active = false;
    bool anchorHeld = false;
    std::uint8_t anchorNote = 0;
    int targetNote = -1;
    int bendRange = 2;
    double currentBend = 64.0;
    int lastEmittedBend = 64;
    Ramp ramp = {};
  };

  void process_note_on(Channel *channel, std::uint8_t midiChannel,
                       const MidiEvent &event, std::vector<MidiEvent> *out);
  void process_note_off(Channel *channel, std::uint8_t midiChannel,
                        const MidiEvent &event, std::vector<MidiEvent> *out);
  void start_ramp(Channel *channel, int targetNote);
  void finish_phrase_with_input_note_off(Channel *channel,
                                         std::uint8_t midiChannel,
                                         const MidiEvent &event,
                                         std::vector<MidiEvent> *out);
  void finish_phrase_with_generated_note_off(Channel *channel,
                                             std::uint8_t midiChannel,
                                             std::vector<MidiEvent> *out);
  void append_reset_if_needed(Channel *channel, std::uint8_t midiChannel,
                              std::vector<MidiEvent> *out);
  static void reset_phrase(Channel *channel);
  static void append_pitch_bend(std::uint8_t midiChannel, int value,
                                std::vector<MidiEvent> *out);
  static int target_bend_value(const Channel &channel, int targetNote);
  static double apply_curve(BendCurve curve, double progress);
  static int rounded_bend(double value);

  std::array<Channel, 16> channels_ = {};
  double bendTimeMs_ = 80.0;
  BendCurve bendCurve_ = BendCurve::Linear;
};

} // namespace ccomidi::legatobend

#endif
