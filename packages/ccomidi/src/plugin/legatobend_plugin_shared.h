#ifndef CCOMIDI_LEGATOBEND_PLUGIN_SHARED_H
#define CCOMIDI_LEGATOBEND_PLUGIN_SHARED_H

#include <mutex>
#include <utility>
#include <vector>

#include <clap/clap.h>

#include "legatobend/legatobend_core.h"

namespace ccomidi::legatobend {

struct EditorState;

struct UiSnapshot {
  double bendTimeMs = 80.0;
  double bendCurve = 0.0;
};

struct TimedMidiEvent {
  std::uint32_t time = 0;
  MidiEvent midi = {};
};

struct Plugin {
  clap_plugin_t clapPlugin = {};
  const clap_host_t *host = nullptr;
  std::mutex stateMutex = {};
  LegatoBendCore core = {};
  bool active = false;
  double sampleRate = 44100.0;
  double samplesUntilNextStep = 0.0;
  EditorState *editor = nullptr;
  clap_id guiTimerId = CLAP_INVALID_ID;
  std::vector<std::pair<clap_id, double>> pendingUiParamEvents = {};
};

void fill_ui_snapshot(Plugin *plugin, UiSnapshot *snapshot);
void apply_ui_param_change(Plugin *plugin, clap_id paramId, double value);
void request_host_param_sync(Plugin *plugin);

} // namespace ccomidi::legatobend

#endif
