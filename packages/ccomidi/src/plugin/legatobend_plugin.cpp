#include <clap/clap.h>
#include <clap/events.h>
#include <clap/ext/audio-ports.h>
#include <clap/ext/gui.h>
#include <clap/ext/note-ports.h>
#include <clap/ext/params.h>
#include <clap/ext/state.h>
#include <clap/plugin-features.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include <vector>

#include "gui/legatobend_editor.h"
#include "plugin/legatobend_param_ids.h"
#include "plugin/legatobend_plugin_shared.h"

namespace ccomidi::legatobend {

namespace {

constexpr std::uint32_t kStateVersion = 1;
constexpr double kStepMs = 5.0;

Plugin *from_plugin(const clap_plugin_t *plugin) {
  return plugin ? static_cast<Plugin *>(plugin->plugin_data) : nullptr;
}

double clamp_bend_time(double value) {
  return std::clamp(value, 0.0, kMaxBendTimeMs);
}

BendCurve curve_from_value(double value) {
  return std::floor(value) >= 1.0 ? BendCurve::Easing : BendCurve::Linear;
}

double value_from_curve(BendCurve curve) {
  return curve == BendCurve::Easing ? 1.0 : 0.0;
}

void append_generated(std::uint32_t time, const std::vector<MidiEvent> &events,
                      std::vector<TimedMidiEvent> *out) {
  for (const MidiEvent &event : events)
    out->push_back(TimedMidiEvent{time, event});
}

std::uint32_t clamp_event_time(double position, std::uint32_t endTime) {
  const auto rounded =
      static_cast<std::uint32_t>(std::max(0.0, std::round(position)));
  return std::min(rounded, endTime);
}

double step_samples(const Plugin *plugin) {
  const double sampleRate = plugin ? plugin->sampleRate : 44100.0;
  return std::max(1.0, sampleRate * (kStepMs / 1000.0));
}

void advance_to(Plugin *plugin, std::uint32_t fromTime, std::uint32_t toTime,
                std::vector<TimedMidiEvent> *out) {
  if (!plugin || !out || toTime <= fromTime)
    return;
  const double stepSamples = step_samples(plugin);
  if (!plugin->core.has_active_ramp()) {
    plugin->samplesUntilNextStep = stepSamples;
    return;
  }
  if (plugin->samplesUntilNextStep <= 0.0)
    plugin->samplesUntilNextStep = stepSamples;

  double position = static_cast<double>(fromTime);
  double remaining = static_cast<double>(toTime - fromTime);
  while (plugin->core.has_active_ramp() &&
         remaining + 0.000001 >= plugin->samplesUntilNextStep) {
    position += plugin->samplesUntilNextStep;
    remaining = static_cast<double>(toTime) - position;
    auto generated = std::vector<MidiEvent>{};
    plugin->core.advance(kStepMs, &generated);
    append_generated(clamp_event_time(position, toTime), generated, out);
    plugin->samplesUntilNextStep = stepSamples;
  }
  if (plugin->core.has_active_ramp()) {
    plugin->samplesUntilNextStep =
        std::max(0.000001, plugin->samplesUntilNextStep - remaining);
  } else {
    plugin->samplesUntilNextStep = stepSamples;
  }
}

bool event_to_midi(const clap_event_header_t *header, MidiEvent *midi) {
  if (!header || !midi || header->space_id != CLAP_CORE_EVENT_SPACE_ID)
    return false;
  if (header->type == CLAP_EVENT_MIDI) {
    const auto *event = reinterpret_cast<const clap_event_midi_t *>(header);
    *midi = MidiEvent{event->data[0], event->data[1], event->data[2]};
    return true;
  }
  if (header->type != CLAP_EVENT_NOTE_ON &&
      header->type != CLAP_EVENT_NOTE_OFF &&
      header->type != CLAP_EVENT_NOTE_CHOKE) {
    return false;
  }
  const auto *event = reinterpret_cast<const clap_event_note_t *>(header);
  const auto channel = static_cast<std::uint8_t>(
      std::clamp(static_cast<int>(event->channel), 0, 15));
  const auto key = static_cast<std::uint8_t>(
      std::clamp(static_cast<int>(event->key), 0, 127));
  auto velocity = static_cast<std::uint8_t>(std::clamp(
      static_cast<int>(std::lround(event->velocity * 127.0)), 0, 127));
  const bool isOn = header->type == CLAP_EVENT_NOTE_ON;
  if (isOn && velocity == 0)
    velocity = 1;
  if (header->type == CLAP_EVENT_NOTE_CHOKE)
    velocity = 0;
  midi->status = static_cast<std::uint8_t>((isOn ? 0x90 : 0x80) | channel);
  midi->data1 = key;
  midi->data2 = velocity;
  return true;
}

void push_midi_event(const TimedMidiEvent &event,
                     const clap_output_events_t *outEvents) {
  if (!outEvents)
    return;
  clap_event_midi_t out = {};
  out.header.size = sizeof(out);
  out.header.time = event.time;
  out.header.space_id = CLAP_CORE_EVENT_SPACE_ID;
  out.header.type = CLAP_EVENT_MIDI;
  out.header.flags = 0;
  out.port_index = 0;
  out.data[0] = event.midi.status;
  out.data[1] = event.midi.data1;
  out.data[2] = event.midi.data2;
  outEvents->try_push(outEvents, &out.header);
}

void zero_audio_output(const clap_process_t *process) {
  if (!process || !process->audio_outputs || process->audio_outputs_count == 0)
    return;
  const clap_audio_buffer_t &out = process->audio_outputs[0];
  for (std::uint32_t ch = 0; ch < out.channel_count; ++ch) {
    if (out.data32 && out.data32[ch])
      std::memset(out.data32[ch], 0, sizeof(float) * process->frames_count);
    if (out.data64 && out.data64[ch])
      std::memset(out.data64[ch], 0, sizeof(double) * process->frames_count);
  }
}

void apply_parameter(Plugin *plugin, clap_id paramId, double value) {
  if (!plugin)
    return;
  if (paramId == kParamBendTimeMs) {
    plugin->core.set_bend_time_ms(clamp_bend_time(value));
  } else if (paramId == kParamBendCurve) {
    plugin->core.set_bend_curve(curve_from_value(value));
  }
}

double get_parameter_value(const Plugin *plugin, clap_id paramId, bool *ok) {
  if (ok)
    *ok = false;
  if (!plugin)
    return 0.0;
  if (paramId == kParamBendTimeMs) {
    if (ok)
      *ok = true;
    return plugin->core.bend_time_ms();
  }
  if (paramId == kParamBendCurve) {
    if (ok)
      *ok = true;
    return value_from_curve(plugin->core.bend_curve());
  }
  return 0.0;
}

void emit_pending_ui_param_events(Plugin *plugin,
                                  const clap_output_events_t *out) {
  if (!plugin || !out)
    return;
  std::vector<std::pair<clap_id, double>> events;
  {
    std::lock_guard<std::mutex> lock(plugin->stateMutex);
    events.swap(plugin->pendingUiParamEvents);
  }
  for (const auto &[paramId, value] : events) {
    clap_event_param_gesture_t begin = {};
    begin.header.size = sizeof(begin);
    begin.header.time = 0;
    begin.header.space_id = CLAP_CORE_EVENT_SPACE_ID;
    begin.header.type = CLAP_EVENT_PARAM_GESTURE_BEGIN;
    begin.header.flags = 0;
    begin.param_id = paramId;
    out->try_push(out, &begin.header);

    clap_event_param_value_t valueEvent = {};
    valueEvent.header.size = sizeof(valueEvent);
    valueEvent.header.time = 0;
    valueEvent.header.space_id = CLAP_CORE_EVENT_SPACE_ID;
    valueEvent.header.type = CLAP_EVENT_PARAM_VALUE;
    valueEvent.header.flags = 0;
    valueEvent.param_id = paramId;
    valueEvent.cookie = nullptr;
    valueEvent.note_id = -1;
    valueEvent.port_index = -1;
    valueEvent.channel = -1;
    valueEvent.key = -1;
    valueEvent.value = value;
    out->try_push(out, &valueEvent.header);

    clap_event_param_gesture_t end = begin;
    end.header.type = CLAP_EVENT_PARAM_GESTURE_END;
    out->try_push(out, &end.header);
  }
}

void destroy_editor(Plugin *plugin) {
  if (!plugin || !plugin->editor)
    return;
  EditorState *editor = plugin->editor;
  plugin->editor = nullptr;
  editor_prepare_destroy(editor);
  editor_destroy(editor);
}

} // namespace

void fill_ui_snapshot(Plugin *plugin, UiSnapshot *snapshot) {
  if (!plugin || !snapshot)
    return;
  std::lock_guard<std::mutex> lock(plugin->stateMutex);
  snapshot->bendTimeMs = plugin->core.bend_time_ms();
  snapshot->bendCurve = value_from_curve(plugin->core.bend_curve());
}

void apply_ui_param_change(Plugin *plugin, clap_id paramId, double value) {
  if (!plugin)
    return;
  {
    std::lock_guard<std::mutex> lock(plugin->stateMutex);
    apply_parameter(plugin, paramId, value);
    plugin->pendingUiParamEvents.emplace_back(paramId, value);
  }
  request_host_param_sync(plugin);
}

void request_host_param_sync(Plugin *plugin) {
  if (!plugin || !plugin->host)
    return;
  const auto *hostParams = static_cast<const clap_host_params_t *>(
      plugin->host->get_extension(plugin->host, CLAP_EXT_PARAMS));
  if (hostParams && hostParams->rescan)
    hostParams->rescan(plugin->host, CLAP_PARAM_RESCAN_VALUES);
  if (hostParams && hostParams->request_flush)
    hostParams->request_flush(plugin->host);
  else if (plugin->host->request_process)
    plugin->host->request_process(plugin->host);
}

bool plugin_init(const clap_plugin_t *plugin) {
  Plugin *self = from_plugin(plugin);
  if (!self)
    return false;
  std::lock_guard<std::mutex> lock(self->stateMutex);
  self->core.set_bend_time_ms(kDefaultBendTimeMs);
  self->core.set_bend_curve(BendCurve::Linear);
  self->samplesUntilNextStep = step_samples(self);
  return true;
}

void plugin_destroy(const clap_plugin_t *plugin) {
  Plugin *self = from_plugin(plugin);
  destroy_editor(self);
  delete self;
}

bool plugin_activate(const clap_plugin_t *plugin, double sampleRate,
                     std::uint32_t minFrames, std::uint32_t maxFrames) {
  (void)minFrames;
  (void)maxFrames;
  Plugin *self = from_plugin(plugin);
  if (!self)
    return false;
  self->active = true;
  self->sampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;
  std::lock_guard<std::mutex> lock(self->stateMutex);
  self->core.reset();
  self->samplesUntilNextStep = step_samples(self);
  return true;
}

void plugin_deactivate(const clap_plugin_t *plugin) {
  Plugin *self = from_plugin(plugin);
  if (self)
    self->active = false;
}

bool plugin_start_processing(const clap_plugin_t *plugin) {
  Plugin *self = from_plugin(plugin);
  if (!self)
    return false;
  std::lock_guard<std::mutex> lock(self->stateMutex);
  self->core.reset();
  self->samplesUntilNextStep = step_samples(self);
  return true;
}

void plugin_stop_processing(const clap_plugin_t *plugin) { (void)plugin; }

void plugin_reset(const clap_plugin_t *plugin) {
  Plugin *self = from_plugin(plugin);
  if (!self)
    return;
  std::lock_guard<std::mutex> lock(self->stateMutex);
  self->core.reset();
  self->samplesUntilNextStep = step_samples(self);
}

clap_process_status plugin_process(const clap_plugin_t *plugin,
                                   const clap_process_t *process) {
  Plugin *self = from_plugin(plugin);
  if (!self || !process)
    return CLAP_PROCESS_ERROR;

  auto output = std::vector<TimedMidiEvent>{};
  const std::uint32_t eventCount =
      process->in_events ? process->in_events->size(process->in_events) : 0;
  output.reserve(eventCount + 16);
  {
    std::lock_guard<std::mutex> lock(self->stateMutex);
    std::uint32_t currentTime = 0;
    for (std::uint32_t i = 0; i < eventCount; ++i) {
      const clap_event_header_t *header =
          process->in_events->get(process->in_events, i);
      if (!header)
        continue;
      const std::uint32_t eventTime =
          std::min(header->time, process->frames_count);
      advance_to(self, currentTime, eventTime, &output);
      currentTime = eventTime;

      if (header->space_id == CLAP_CORE_EVENT_SPACE_ID &&
          header->type == CLAP_EVENT_PARAM_VALUE) {
        const auto *param =
            reinterpret_cast<const clap_event_param_value_t *>(header);
        apply_parameter(self, param->param_id, param->value);
        continue;
      }

      auto midi = MidiEvent{};
      if (event_to_midi(header, &midi)) {
        auto generated = std::vector<MidiEvent>{};
        self->core.process(midi, &generated);
        append_generated(eventTime, generated, &output);
      }
    }
    advance_to(self, currentTime, process->frames_count, &output);
  }

  for (const TimedMidiEvent &event : output)
    push_midi_event(event, process->out_events);
  emit_pending_ui_param_events(self, process->out_events);
  zero_audio_output(process);
  return CLAP_PROCESS_CONTINUE;
}

const void *plugin_get_extension(const clap_plugin_t *plugin, const char *id);
void plugin_on_main_thread(const clap_plugin_t *plugin);

const char *s_features[] = {
    CLAP_PLUGIN_FEATURE_NOTE_EFFECT,
    CLAP_PLUGIN_FEATURE_UTILITY,
    nullptr,
};

const clap_plugin_descriptor_t s_descriptor = {
    .clap_version = CLAP_VERSION,
    .id = "com.specker.ccomidi.legatobend",
    .name = "ccomidi legatobend",
    .vendor = "specker",
    .url = "",
    .manual_url = "",
    .support_url = "",
    .version = "0.1.0",
    .description = "Legato MIDI note-to-pitch-bend transformer",
    .features = s_features,
};

const clap_plugin_t s_pluginPrototype = {
    .desc = &s_descriptor,
    .plugin_data = nullptr,
    .init = plugin_init,
    .destroy = plugin_destroy,
    .activate = plugin_activate,
    .deactivate = plugin_deactivate,
    .start_processing = plugin_start_processing,
    .stop_processing = plugin_stop_processing,
    .reset = plugin_reset,
    .process = plugin_process,
    .get_extension = plugin_get_extension,
    .on_main_thread = plugin_on_main_thread,
};

bool gui_is_api_supported(const clap_plugin_t *plugin, const char *api,
                          bool isFloating) {
  (void)plugin;
  bool supported = false;
  if (isFloating)
    supported = true;
#if defined(_WIN32)
  else
    supported = api && std::strcmp(api, CLAP_WINDOW_API_WIN32) == 0;
#elif defined(__APPLE__)
  else
    supported = api && std::strcmp(api, CLAP_WINDOW_API_COCOA) == 0;
#else
  else
    supported = api && std::strcmp(api, CLAP_WINDOW_API_X11) == 0;
#endif
  return supported;
}

bool gui_get_preferred_api(const clap_plugin_t *plugin, const char **api,
                           bool *isFloating) {
  (void)plugin;
  if (!api || !isFloating)
    return false;
  *isFloating = false;
#if defined(_WIN32)
  *api = CLAP_WINDOW_API_WIN32;
#elif defined(__APPLE__)
  *api = CLAP_WINDOW_API_COCOA;
#else
  *api = CLAP_WINDOW_API_X11;
#endif
  return true;
}

bool gui_create(const clap_plugin_t *plugin, const char *api, bool isFloating) {
  (void)api;
  (void)isFloating;
  Plugin *self = from_plugin(plugin);
  if (!self || self->editor)
    return false;
  self->editor = editor_create(self);
  return self->editor != nullptr;
}

void gui_destroy(const clap_plugin_t *plugin) {
  Plugin *self = from_plugin(plugin);
  destroy_editor(self);
}

bool gui_set_scale(const clap_plugin_t *plugin, double scale) {
  (void)plugin;
  (void)scale;
  return false;
}

bool gui_get_size(const clap_plugin_t *plugin, std::uint32_t *width,
                  std::uint32_t *height) {
  Plugin *self = from_plugin(plugin);
  if (!self || !self->editor)
    return false;
  editor_get_size(self->editor, width, height);
  return true;
}

bool gui_can_resize(const clap_plugin_t *plugin) {
  Plugin *self = from_plugin(plugin);
  return self && self->editor && editor_can_resize(self->editor);
}

bool gui_get_resize_hints(const clap_plugin_t *plugin,
                          clap_gui_resize_hints_t *hints) {
  (void)plugin;
  if (!hints)
    return false;
  hints->can_resize_horizontally = true;
  hints->can_resize_vertically = true;
  hints->preserve_aspect_ratio = false;
  hints->aspect_ratio_width = 0;
  hints->aspect_ratio_height = 0;
  return true;
}

bool gui_adjust_size(const clap_plugin_t *plugin, std::uint32_t *width,
                     std::uint32_t *height) {
  (void)plugin;
  (void)width;
  (void)height;
  return true;
}

bool gui_set_size(const clap_plugin_t *plugin, std::uint32_t width,
                  std::uint32_t height) {
  Plugin *self = from_plugin(plugin);
  return self && self->editor && editor_set_size(self->editor, width, height);
}

bool gui_set_parent(const clap_plugin_t *plugin, const clap_window_t *window) {
  Plugin *self = from_plugin(plugin);
  if (!self || !self->editor || !window)
    return false;
  std::uintptr_t nativeParent = 0;
#if defined(_WIN32)
  nativeParent = reinterpret_cast<std::uintptr_t>(window->win32);
#elif defined(__APPLE__)
  nativeParent = reinterpret_cast<std::uintptr_t>(window->cocoa);
#else
  nativeParent = static_cast<std::uintptr_t>(window->x11);
#endif
  return editor_set_parent(self->editor, nativeParent);
}

bool gui_set_transient(const clap_plugin_t *plugin,
                       const clap_window_t *window) {
  (void)plugin;
  (void)window;
  return true;
}

void gui_suggest_title(const clap_plugin_t *plugin, const char *title) {
  (void)plugin;
  (void)title;
}

bool gui_show(const clap_plugin_t *plugin) {
  Plugin *self = from_plugin(plugin);
  if (!self || !self->editor)
    return false;
  const bool shown = editor_show(self->editor);
  if (shown)
    editor_start_internal_timer(self->editor);
  return shown;
}

bool gui_hide(const clap_plugin_t *plugin) {
  Plugin *self = from_plugin(plugin);
  if (!self || !self->editor)
    return false;
  return editor_hide(self->editor);
}

const clap_plugin_gui_t s_gui = {
    .is_api_supported = gui_is_api_supported,
    .get_preferred_api = gui_get_preferred_api,
    .create = gui_create,
    .destroy = gui_destroy,
    .set_scale = gui_set_scale,
    .get_size = gui_get_size,
    .can_resize = gui_can_resize,
    .get_resize_hints = gui_get_resize_hints,
    .adjust_size = gui_adjust_size,
    .set_size = gui_set_size,
    .set_parent = gui_set_parent,
    .set_transient = gui_set_transient,
    .suggest_title = gui_suggest_title,
    .show = gui_show,
    .hide = gui_hide,
};

std::uint32_t audio_ports_count(const clap_plugin_t *plugin, bool isInput) {
  (void)plugin;
  return isInput ? 0u : 1u;
}

bool audio_ports_get(const clap_plugin_t *plugin, std::uint32_t index,
                     bool isInput, clap_audio_port_info_t *info) {
  (void)plugin;
  if (isInput || index != 0 || !info)
    return false;
  std::memset(info, 0, sizeof(*info));
  info->id = 0;
  std::snprintf(info->name, sizeof(info->name), "%s", "Audio Output");
  info->flags = CLAP_AUDIO_PORT_IS_MAIN;
  info->channel_count = 2;
  info->port_type = CLAP_PORT_STEREO;
  info->in_place_pair = CLAP_INVALID_ID;
  return true;
}

const clap_plugin_audio_ports_t s_audioPorts = {
    .count = audio_ports_count,
    .get = audio_ports_get,
};

std::uint32_t note_ports_count(const clap_plugin_t *plugin, bool isInput) {
  (void)plugin;
  (void)isInput;
  return 1u;
}

bool note_ports_get(const clap_plugin_t *plugin, std::uint32_t index,
                    bool isInput, clap_note_port_info_t *info) {
  (void)plugin;
  if (index != 0 || !info)
    return false;
  std::memset(info, 0, sizeof(*info));
  info->id = 0;
  info->supported_dialects = CLAP_NOTE_DIALECT_MIDI;
  info->preferred_dialect = CLAP_NOTE_DIALECT_MIDI;
  std::snprintf(info->name, sizeof(info->name), "%s",
                isInput ? "MIDI Input" : "MIDI Output");
  return true;
}

const clap_plugin_note_ports_t s_notePorts = {
    .count = note_ports_count,
    .get = note_ports_get,
};

std::uint32_t params_count(const clap_plugin_t *plugin) {
  (void)plugin;
  return kParamCount;
}

bool params_get_info(const clap_plugin_t *plugin, std::uint32_t paramIndex,
                     clap_param_info_t *info) {
  (void)plugin;
  if (!info || paramIndex >= kParamCount)
    return false;
  std::memset(info, 0, sizeof(*info));
  info->id = paramIndex == 0 ? kParamBendTimeMs : kParamBendCurve;
  info->flags = CLAP_PARAM_IS_AUTOMATABLE | CLAP_PARAM_REQUIRES_PROCESS;
  if (info->id == kParamBendTimeMs) {
    std::snprintf(info->name, sizeof(info->name), "Bend Time");
    std::snprintf(info->module, sizeof(info->module), "Legato Bend");
    info->min_value = 0.0;
    info->max_value = kMaxBendTimeMs;
    info->default_value = kDefaultBendTimeMs;
  } else {
    std::snprintf(info->name, sizeof(info->name), "Curve");
    std::snprintf(info->module, sizeof(info->module), "Legato Bend");
    info->flags |= CLAP_PARAM_IS_STEPPED | CLAP_PARAM_IS_ENUM;
    info->min_value = 0.0;
    info->max_value = 1.0;
    info->default_value = 0.0;
  }
  return true;
}

bool params_get_value(const clap_plugin_t *plugin, clap_id paramId,
                      double *value) {
  Plugin *self = from_plugin(plugin);
  if (!self || !value)
    return false;
  std::lock_guard<std::mutex> lock(self->stateMutex);
  bool ok = false;
  *value = get_parameter_value(self, paramId, &ok);
  return ok;
}

bool params_value_to_text(const clap_plugin_t *plugin, clap_id paramId,
                          double value, char *display, std::uint32_t size) {
  (void)plugin;
  if (!display || size == 0)
    return false;
  if (paramId == kParamBendTimeMs) {
    std::snprintf(display, size, "%.1f ms", clamp_bend_time(value));
    return true;
  }
  if (paramId == kParamBendCurve) {
    std::snprintf(display, size, "%s",
                  curve_from_value(value) == BendCurve::Easing ? "Easing"
                                                               : "Linear");
    return true;
  }
  return false;
}

bool params_text_to_value(const clap_plugin_t *plugin, clap_id paramId,
                          const char *display, double *value) {
  (void)plugin;
  if (!display || !value)
    return false;
  if (paramId == kParamBendCurve) {
    if (std::strcmp(display, "Easing") == 0) {
      *value = 1.0;
      return true;
    }
    if (std::strcmp(display, "Linear") == 0) {
      *value = 0.0;
      return true;
    }
  }
  char *end = nullptr;
  const double parsed = std::strtod(display, &end);
  if (end == display)
    return false;
  if (paramId == kParamBendTimeMs) {
    *value = clamp_bend_time(parsed);
    return true;
  }
  if (paramId == kParamBendCurve) {
    *value = value_from_curve(curve_from_value(parsed));
    return true;
  }
  return false;
}

void params_flush(const clap_plugin_t *plugin, const clap_input_events_t *in,
                  const clap_output_events_t *out) {
  Plugin *self = from_plugin(plugin);
  if (!self)
    return;
  if (in) {
    std::lock_guard<std::mutex> lock(self->stateMutex);
    const std::uint32_t count = in->size(in);
    for (std::uint32_t i = 0; i < count; ++i) {
      const clap_event_header_t *header = in->get(in, i);
      if (!header || header->space_id != CLAP_CORE_EVENT_SPACE_ID ||
          header->type != CLAP_EVENT_PARAM_VALUE)
        continue;
      const auto *event =
          reinterpret_cast<const clap_event_param_value_t *>(header);
      apply_parameter(self, event->param_id, event->value);
    }
  }
  emit_pending_ui_param_events(self, out);
}

const clap_plugin_params_t s_params = {
    .count = params_count,
    .get_info = params_get_info,
    .get_value = params_get_value,
    .value_to_text = params_value_to_text,
    .text_to_value = params_text_to_value,
    .flush = params_flush,
};

bool state_save(const clap_plugin_t *plugin, const clap_ostream_t *stream) {
  Plugin *self = from_plugin(plugin);
  if (!self || !stream)
    return false;
  std::lock_guard<std::mutex> lock(self->stateMutex);
  const std::uint32_t version = kStateVersion;
  if (stream->write(stream, &version, sizeof(version)) != sizeof(version))
    return false;
  const double bendTime = self->core.bend_time_ms();
  if (stream->write(stream, &bendTime, sizeof(bendTime)) != sizeof(bendTime))
    return false;
  const double curve = value_from_curve(self->core.bend_curve());
  return stream->write(stream, &curve, sizeof(curve)) == sizeof(curve);
}

bool state_load(const clap_plugin_t *plugin, const clap_istream_t *stream) {
  Plugin *self = from_plugin(plugin);
  if (!self || !stream)
    return false;
  std::uint32_t version = 0;
  if (stream->read(stream, &version, sizeof(version)) != sizeof(version))
    return false;
  if (version != kStateVersion)
    return false;
  double bendTime = kDefaultBendTimeMs;
  if (stream->read(stream, &bendTime, sizeof(bendTime)) != sizeof(bendTime))
    return false;
  double curve = 0.0;
  if (stream->read(stream, &curve, sizeof(curve)) != sizeof(curve))
    return false;
  {
    std::lock_guard<std::mutex> lock(self->stateMutex);
    self->core.set_bend_time_ms(bendTime);
    self->core.set_bend_curve(curve_from_value(curve));
    self->core.reset();
  }
  request_host_param_sync(self);
  return true;
}

const clap_plugin_state_t s_state = {
    .save = state_save,
    .load = state_load,
};

const void *plugin_get_extension(const clap_plugin_t *plugin, const char *id) {
  (void)plugin;
  if (std::strcmp(id, CLAP_EXT_AUDIO_PORTS) == 0)
    return &s_audioPorts;
  if (std::strcmp(id, CLAP_EXT_GUI) == 0)
    return &s_gui;
  if (std::strcmp(id, CLAP_EXT_NOTE_PORTS) == 0)
    return &s_notePorts;
  if (std::strcmp(id, CLAP_EXT_PARAMS) == 0)
    return &s_params;
  if (std::strcmp(id, CLAP_EXT_STATE) == 0)
    return &s_state;
  return nullptr;
}

void plugin_on_main_thread(const clap_plugin_t *plugin) { (void)plugin; }

std::uint32_t factory_get_plugin_count(const clap_plugin_factory_t *factory) {
  (void)factory;
  return 1;
}

const clap_plugin_descriptor_t *
factory_get_plugin_descriptor(const clap_plugin_factory_t *factory,
                              std::uint32_t index) {
  (void)factory;
  return index == 0 ? &s_descriptor : nullptr;
}

const clap_plugin_t *factory_create_plugin(const clap_plugin_factory_t *factory,
                                           const clap_host_t *host,
                                           const char *pluginId) {
  (void)factory;
  if (!host || !pluginId || std::strcmp(pluginId, s_descriptor.id) != 0)
    return nullptr;
  if (!clap_version_is_compatible(host->clap_version))
    return nullptr;
  auto *self = new (std::nothrow) Plugin();
  if (!self)
    return nullptr;
  self->host = host;
  self->clapPlugin = s_pluginPrototype;
  self->clapPlugin.plugin_data = self;
  return &self->clapPlugin;
}

const clap_plugin_factory_t s_factory = {
    .get_plugin_count = factory_get_plugin_count,
    .get_plugin_descriptor = factory_get_plugin_descriptor,
    .create_plugin = factory_create_plugin,
};

bool entry_init(const char *pluginPath) {
  (void)pluginPath;
  return true;
}

void entry_deinit(void) {}

const void *entry_get_factory(const char *factoryId) {
  if (std::strcmp(factoryId, CLAP_PLUGIN_FACTORY_ID) == 0)
    return &s_factory;
  return nullptr;
}

} // namespace ccomidi::legatobend

extern "C" const clap_plugin_entry clap_entry = {
    .clap_version = CLAP_VERSION,
    .init = ccomidi::legatobend::entry_init,
    .deinit = ccomidi::legatobend::entry_deinit,
    .get_factory = ccomidi::legatobend::entry_get_factory,
};
