#include "VoicegroupLanguageBridgeTests.h"

#include "VoicegroupLanguageBridge.h"

#include <algorithm>
#include <iostream>

bool runVoicegroupLanguageBridgeTests() {
  if (!juce::File(TEXTEDIT_VOICEGROUP_BRIDGE_PATH).existsAsFile()) {
    std::cerr << "voicegroup bridge integration skipped: missing "
              << TEXTEDIT_VOICEGROUP_BRIDGE_PATH << "\n";
    return true;
  }

  auto bridge = VoicegroupLanguageBridge{};
  if (!bridge.isAvailable()) {
    std::cerr << "voicegroup bridge unavailable: " << bridge.getStatusText()
              << "\n";
    return false;
  }

  if (!bridge.syncDocument("file:///textedit/voicegroup.inc", "\tvoice_dir")) {
    std::cerr << "voicegroup bridge sync failed: " << bridge.getStatusText()
              << "\n";
    return false;
  }

  const auto completions = bridge.completions(0, 10);
  if (std::none_of(
          completions.begin(), completions.end(),
          [](const auto &item) { return item.label == "voice_directsound"; })) {
    std::cerr
        << "voicegroup bridge completion failed: missing voice_directsound\n";
    return false;
  }

  if (!bridge.syncDocument("file:///textedit/voicegroup.inc",
                           "\tvoice_directsound 60, 0, "
                           "DirectSoundWaveData_piano, 255, 0, 255, 127")) {
    std::cerr << "voicegroup bridge hover sync failed: "
              << bridge.getStatusText() << "\n";
    return false;
  }

  const auto hover = bridge.hover(0, 20);
  if (!hover.has_value() || !hover->contains("base_midi_key")) {
    std::cerr << "voicegroup bridge hover failed\n";
    return false;
  }

  return true;
}
