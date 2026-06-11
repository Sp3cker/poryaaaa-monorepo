#include "gui/legatobend_editor.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

#include "imgui.h"

#include "gui/editor_shell.h"
#include "plugin/legatobend_param_ids.h"

namespace ccomidi::legatobend {

struct EditorState {
  Plugin *plugin = nullptr;
  EditorShell *shell = nullptr;
};

namespace {

constexpr std::uint32_t kDefaultWidth = 520;
constexpr std::uint32_t kDefaultHeight = 220;

void draw_frame(void *userData, std::uint32_t width, std::uint32_t height) {
  auto *editor = static_cast<EditorState *>(userData);
  Plugin *plugin = editor ? editor->plugin : nullptr;
  if (!plugin)
    return;

  UiSnapshot snapshot = {};
  fill_ui_snapshot(plugin, &snapshot);

  ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
  ImGui::SetNextWindowSize(
      ImVec2(static_cast<float>(width), static_cast<float>(height)));
  const ImGuiWindowFlags flags =
      ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
      ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize;

  ImGui::Begin("##ccomidi_legatobend", nullptr, flags);
  ImFont *boldFont = editor_shell_bold_font(editor->shell);
  if (boldFont)
    ImGui::PushFont(boldFont, 0.0f);
  ImGui::TextUnformatted("ccomidi legatobend");
  if (boldFont)
    ImGui::PopFont();
  ImGui::Separator();

  float bendTime = static_cast<float>(std::clamp(
      snapshot.bendTimeMs, 0.0, static_cast<double>(kMaxBendTimeMs)));
  ImGui::SetNextItemWidth(-1.0f);
  if (ImGui::SliderFloat("Bend Time", &bendTime, 0.0f,
                         static_cast<float>(kMaxBendTimeMs), "%.1f ms",
                         ImGuiSliderFlags_AlwaysClamp)) {
    apply_ui_param_change(plugin, kParamBendTimeMs, bendTime);
  }

  int curve = std::floor(snapshot.bendCurve) >= 1.0 ? 1 : 0;
  const char *items[] = {"Linear", "Easing"};
  ImGui::SetNextItemWidth(-1.0f);
  if (ImGui::Combo("Curve", &curve, items, 2))
    apply_ui_param_change(plugin, kParamBendCurve, static_cast<double>(curve));

  ImGui::End();
}

} // namespace

EditorState *editor_create(Plugin *plugin) {
  auto *editor = new EditorState();
  editor->plugin = plugin;

  EditorShellConfig config = {};
  config.title = "ccomidi legatobend";
  config.className = "ccomidi-legatobend";
  config.defaultWidth = kDefaultWidth;
  config.defaultHeight = kDefaultHeight;
  config.minWidth = 420;
  config.minHeight = 180;

  EditorShellCallbacks callbacks = {};
  callbacks.userData = editor;
  callbacks.drawFrame = &draw_frame;

  editor->shell =
      editor_shell_create(plugin ? plugin->host : nullptr, config, callbacks);
  if (!editor->shell) {
    delete editor;
    return nullptr;
  }
  return editor;
}

void editor_prepare_destroy(EditorState *editor) {
  if (!editor)
    return;
  editor->plugin = nullptr;
  editor_shell_prepare_destroy(editor->shell);
}

void editor_destroy(EditorState *editor) {
  if (!editor)
    return;
  editor_shell_destroy(editor->shell);
  delete editor;
}

bool editor_show(EditorState *editor) {
  return editor && editor_shell_show(editor->shell);
}

bool editor_hide(EditorState *editor) {
  return editor && editor_shell_hide(editor->shell);
}

bool editor_set_parent(EditorState *editor, std::uintptr_t nativeParent) {
  return editor && editor_shell_set_parent(editor->shell, nativeParent);
}

bool editor_set_size(EditorState *editor, std::uint32_t width,
                     std::uint32_t height) {
  return editor && editor_shell_set_size(editor->shell, width, height);
}

void editor_get_size(EditorState *editor, std::uint32_t *width,
                     std::uint32_t *height) {
  if (!editor) {
    if (width)
      *width = kDefaultWidth;
    if (height)
      *height = kDefaultHeight;
    return;
  }
  editor_shell_get_size(editor->shell, width, height);
}

bool editor_can_resize(EditorState *editor) {
  return editor && editor_shell_can_resize(editor->shell);
}

bool editor_was_closed(EditorState *editor) {
  return editor && editor_shell_was_closed(editor->shell);
}

void editor_start_internal_timer(EditorState *editor) {
  if (editor)
    editor_shell_start_timer(editor->shell);
}

void editor_stop_internal_timer(EditorState *editor) {
  if (editor)
    editor_shell_stop_timer(editor->shell);
}

} // namespace ccomidi::legatobend
