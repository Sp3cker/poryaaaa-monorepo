#include "gui/ccomidi_editor.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <string>

#include "imgui.h"

#include "gui/editor_shell.h"
#include "plugin/param_ids.h"

namespace ccomidi {

struct EditorState {
  Plugin *plugin = nullptr;
  EditorShell *shell = nullptr;
  std::string windowTitle = {};
};

namespace {

constexpr std::uint32_t kDefaultWidth = 980;
constexpr std::uint32_t kDefaultHeight = 620;

ImU32 output_channel_color(std::uint8_t outputChannel) {
  static constexpr ImU32 kChannelColors[] = {
      IM_COL32(0x61, 0xAF, 0xEF, 0xFF),
      IM_COL32(0xFF, 0x95, 0x80, 0xFF),
      IM_COL32(0x8A, 0xFF, 0x80, 0xFF),
      IM_COL32(0xFF, 0xFF, 0x80, 0xFF),
      IM_COL32(0x95, 0x80, 0xFF, 0xFF),
      IM_COL32(0xFF, 0x80, 0xBF, 0xFF),
      IM_COL32(0x80, 0xFF, 0xEA, 0xFF),
      IM_COL32(0xF8, 0xF8, 0xF2, 0xFF),
      IM_COL32(0x79, 0x70, 0xA9, 0xFF),
      IM_COL32(0xFF, 0xBF, 0xB3, 0xFF),
      IM_COL32(0xB9, 0xFF, 0xB3, 0xFF),
      IM_COL32(0xFF, 0xFF, 0xB3, 0xFF),
      IM_COL32(0xBF, 0xB3, 0xFF, 0xFF),
      IM_COL32(0xFF, 0xB3, 0xD9, 0xFF),
      IM_COL32(0xB3, 0xFF, 0xF2, 0xFF),
      IM_COL32(0xFF, 0xFF, 0xFF, 0xFF),
  };
  return kChannelColors[std::clamp<int>(outputChannel, 0, 15)];
}

int field_count_for_type(CommandType type) {
  switch (type) {
  case CommandType::None:
    return 0;
  case CommandType::MemAcc0C:
  case CommandType::MemAcc10:
  case CommandType::Xcmd0D:
    return 4;
  default:
    return 1;
  }
}

const char *field_label(CommandType type, int field) {
  if (type == CommandType::MemAcc0C || type == CommandType::MemAcc10) {
    static const char *labels[] = {"Op", "Arg1", "Arg2", "Data"};
    return labels[field];
  }
  if (type == CommandType::Xcmd0D) {
    static const char *labels[] = {"B0", "B1", "B2", "B3"};
    return labels[field];
  }

  switch (field) {
  case 0:
    return "Value";
  case 1:
    return "B";
  case 2:
    return "C";
  case 3:
    return "D";
  default:
    return "";
  }
}

std::uint8_t floor_to_u8(double value, std::uint8_t minValue,
                         std::uint8_t maxValue) {
  const double floored = std::floor(value);
  if (floored <= static_cast<double>(minValue))
    return minValue;
  if (floored >= static_cast<double>(maxValue))
    return maxValue;
  return static_cast<std::uint8_t>(floored);
}

bool is_table_command_type(CommandType type) {
  return type == CommandType::None || !is_fixed_command_type(type);
}

void draw_parameter_controls(Plugin *plugin, std::size_t row, CommandType type,
                             const UiRowSnapshot &rowSnapshot) {
  const int activeFields = field_count_for_type(type);
  if (activeFields == 0) {
    ImGui::AlignTextToFramePadding();
    ImGui::TextDisabled("-");
    return;
  }

  if (activeFields == 1) {
    int value = static_cast<int>(rowSnapshot.values[0]);
    const std::string label =
        std::string(field_label(type, 0)) + "##v" + std::to_string(row) + "_0";
    ImGui::SetNextItemWidth(-1.0f);
    if (ImGui::SliderInt(label.c_str(), &value, 0, 127, "%d",
                         ImGuiSliderFlags_AlwaysClamp)) {
      apply_ui_param_change(
          plugin,
          row_param_id(static_cast<std::uint32_t>(row), RowParamSlot::Value0),
          value);
    }
    return;
  }

  const float fullWidth = ImGui::GetContentRegionAvail().x;
  const float spacing = ImGui::GetStyle().ItemSpacing.x;
  const float controlWidth =
      (fullWidth - (spacing * static_cast<float>(activeFields - 1))) /
      static_cast<float>(activeFields);

  for (int field = 0; field < activeFields; ++field) {
    if (field > 0)
      ImGui::SameLine();

    ImGui::PushItemWidth(controlWidth);
    int value = static_cast<int>(rowSnapshot.values[field]);
    const std::string label = std::string(field_label(type, field)) + "##v" +
                              std::to_string(row) + "_" + std::to_string(field);
    if (ImGui::SliderInt(label.c_str(), &value, 0, 127, "%d",
                         ImGuiSliderFlags_AlwaysClamp)) {
      apply_ui_param_change(
          plugin,
          row_param_id(static_cast<std::uint32_t>(row),
                       static_cast<RowParamSlot>(
                           static_cast<int>(RowParamSlot::Value0) + field)),
          value);
    }
    ImGui::PopItemWidth();
  }
}

void draw_frame(void *userData, std::uint32_t width, std::uint32_t height) {
  auto *editor = static_cast<EditorState *>(userData);
  Plugin *plugin = editor ? editor->plugin : nullptr;
  if (!plugin)
    return;

  UiSnapshot snapshot = {};
  fill_ui_snapshot(plugin, &snapshot);
  reload_voicegroup_if_changed(plugin);

  ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
  ImGui::SetNextWindowSize(
      ImVec2(static_cast<float>(width), static_cast<float>(height)));
  const ImGuiWindowFlags flags =
      ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
      ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize;

  ImGui::Begin("##ccomidi", nullptr, flags);

  const std::uint8_t outputChannelIndex =
      floor_to_u8(snapshot.outputChannel, 0, 15);
  int outputChannel = static_cast<int>(outputChannelIndex) + 1;
  outputChannel = std::clamp(outputChannel, 1, 16);
  const std::string windowTitle =
      "ccomidi - Chn " + std::to_string(outputChannel);
  if (editor->windowTitle != windowTitle) {
    editor->windowTitle = windowTitle;
    editor_shell_set_title(editor->shell, editor->windowTitle.c_str());
  }
  ImFont *boldFont = editor_shell_bold_font(editor->shell);

  if (boldFont)
    ImGui::PushFont(boldFont, 0.0f);
  ImGui::TextUnformatted("ccomidi");
  if (boldFont)
    ImGui::PopFont();
  ImGui::Separator();

  bool programEnabled = std::floor(snapshot.programEnabled) >= 1.0;
  int program = static_cast<int>(std::floor(snapshot.program));
  program = std::clamp(program, 0, 127);
  const bool hasVoices = !plugin->voiceLoad.slots.empty();
  const char *reloadVoicesLabel = "Reload Voices";
  const float reloadVoicesWidth =
      ImGui::CalcTextSize(reloadVoicesLabel).x +
      ImGui::GetStyle().FramePadding.x * 2.0f;
  if (ImGui::BeginTable("program_row", 3,
                        ImGuiTableFlags_SizingStretchProp,
                        ImVec2(ImGui::GetContentRegionAvail().x, 0.0f))) {
    ImGui::TableSetupColumn("##emit_program_col",
                            ImGuiTableColumnFlags_WidthFixed,
                            ImGui::GetFrameHeight());
    ImGui::TableSetupColumn("##program_col",
                            ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableSetupColumn("##reload_voices_col",
                            ImGuiTableColumnFlags_WidthFixed,
                            reloadVoicesWidth);
    ImGui::TableNextRow();

    ImGui::TableSetColumnIndex(0);
    if (ImGui::Checkbox("##emit_program", &programEnabled))
      apply_ui_param_change(plugin, kParamProgramEnabled,
                            programEnabled ? 1.0 : 0.0);

    ImGui::TableSetColumnIndex(1);
    ImGui::BeginDisabled(!programEnabled);
    if (hasVoices) {
      const VoiceSlot *current = nullptr;
      for (const VoiceSlot &slot : plugin->voiceLoad.slots) {
        if (slot.program == program) {
          current = &slot;
          break;
        }
      }
      char preview[288];
      if (current)
        std::snprintf(preview, sizeof(preview), "%03d  %s", current->program,
                      current->name.c_str());
      else
        std::snprintf(preview, sizeof(preview), "%03d  (empty slot)", program);
      ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
      if (boldFont)
        ImGui::PushFont(boldFont, 0.0f);
      const bool comboOpen = ImGui::BeginCombo("##program", preview);
      if (boldFont)
        ImGui::PopFont();
      if (comboOpen) {
        for (const VoiceSlot &slot : plugin->voiceLoad.slots) {
          char label[288];
          std::snprintf(label, sizeof(label), "%03d  %s", slot.program,
                        slot.name.c_str());
          const bool selected = slot.program == program;
          if (ImGui::Selectable(label, selected))
            apply_ui_param_change(plugin, kParamProgram,
                                  static_cast<double>(slot.program));
          if (selected)
            ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
      }
    } else {
      ImGui::AlignTextToFramePadding();
      ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "Voicegroup: %s",
                         plugin->voiceLoad.error.c_str());
      if (!plugin->voiceLoad.statePath.empty())
        ImGui::TextDisabled("state: %s", plugin->voiceLoad.statePath.c_str());
    }
    ImGui::EndDisabled();

    ImGui::TableSetColumnIndex(2);
    if (ImGui::Button(reloadVoicesLabel)) {
      std::lock_guard<std::mutex> lock(plugin->stateMutex);
      plugin->voiceLoad = voicegroup_bridge_load_state();
    }
    ImGui::EndTable();
  }

  ImGui::Spacing();

  if (boldFont)
    ImGui::PushFont(boldFont, 0.0f);
  ImGui::AlignTextToFramePadding();
  ImGui::TextUnformatted("Channel");
  if (boldFont)
    ImGui::PopFont();
  ImGui::SameLine();
  ImGui::SetNextItemWidth(-1.0f);
  ImVec4 channelColor =
      ImGui::ColorConvertU32ToFloat4(output_channel_color(outputChannelIndex));
  channelColor.w = 0.5f;
  ImGui::PushStyleColor(ImGuiCol_FrameBg, channelColor);
  ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, channelColor);
  ImGui::PushStyleColor(ImGuiCol_FrameBgActive, channelColor);
  if (ImGui::SliderInt("##output_channel", &outputChannel, 1, 16))
    apply_ui_param_change(plugin, kParamOutputChannel,
                          static_cast<double>(outputChannel - 1));
  ImGui::PopStyleColor(3);

  ImGui::Spacing();

  if (ImGui::BeginTable("fixed_rows", 3,
                        ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
    ImGui::TableSetupColumn("Command", ImGuiTableColumnFlags_WidthFixed,
                            120.0f);
    ImGui::TableSetupColumn("On", ImGuiTableColumnFlags_WidthFixed, 40.0f);
    ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableHeadersRow();

    for (std::size_t row = 0; row < kFixedCommandRowCount; ++row) {
      const CommandType type = fixed_command_type_for_row(row);

      ImGui::TableNextRow();
      ImGui::TableSetColumnIndex(0);
      ImGui::AlignTextToFramePadding();
      ImGui::TextUnformatted(command_type_name(type));

      ImGui::TableSetColumnIndex(1);
      bool enabled = std::floor(snapshot.rows[row].enabled) >= 1.0;
      if (ImGui::Checkbox(("##fixed_enabled" + std::to_string(row)).c_str(),
                          &enabled))
        apply_ui_param_change(plugin,
                              row_param_id(static_cast<std::uint32_t>(row),
                                           RowParamSlot::Enabled),
                              enabled ? 1.0 : 0.0);

      ImGui::TableSetColumnIndex(2);
      draw_parameter_controls(plugin, row, type, snapshot.rows[row]);
    }

    ImGui::EndTable();
  }

  ImGui::Spacing();
  ImGui::TextUnformatted("Additional Commands");
  if (ImGui::BeginTable("rows", 3,
                        ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                            ImGuiTableFlags_ScrollY)) {
    ImGui::TableSetupColumn("On", ImGuiTableColumnFlags_WidthFixed, 40.0f);
    ImGui::TableSetupColumn("Command", ImGuiTableColumnFlags_WidthStretch,
                            1.4f);
    ImGui::TableSetupColumn("Parameters", ImGuiTableColumnFlags_WidthStretch,
                            2.2f);
    ImGui::TableHeadersRow();

    for (std::size_t row = kFixedCommandRowCount; row < kMaxCommandRows;
         ++row) {
      const CommandType type = static_cast<CommandType>(
          static_cast<int>(std::floor(snapshot.rows[row].type)));

      ImGui::TableNextRow();
      ImGui::TableSetColumnIndex(0);
      bool enabled = std::floor(snapshot.rows[row].enabled) >= 1.0;
      if (ImGui::Checkbox(("##enabled" + std::to_string(row)).c_str(),
                          &enabled))
        apply_ui_param_change(plugin,
                              row_param_id(static_cast<std::uint32_t>(row),
                                           RowParamSlot::Enabled),
                              enabled ? 1.0 : 0.0);

      ImGui::TableSetColumnIndex(1);
      int typeIndex = static_cast<int>(type);
      if (ImGui::BeginCombo(("##type" + std::to_string(row)).c_str(),
                            command_type_name(type))) {
        for (int candidate = static_cast<int>(CommandType::None);
             candidate <= static_cast<int>(CommandType::Xcmd0D);
             ++candidate) {
          const auto candidateType = static_cast<CommandType>(candidate);
          if (!is_table_command_type(candidateType))
            continue;
          const bool selected = candidate == typeIndex;
          if (ImGui::Selectable(command_type_name(candidateType), selected)) {
            apply_ui_param_change(plugin,
                                  row_param_id(static_cast<std::uint32_t>(row),
                                               RowParamSlot::Type),
                                  static_cast<double>(candidate));
          }
          if (selected)
            ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
      }

      ImGui::TableSetColumnIndex(2);
      draw_parameter_controls(plugin, row, type, snapshot.rows[row]);
    }

    ImGui::EndTable();
  }

  ImGui::End();
}

} // namespace

EditorState *editor_create(Plugin *plugin) {
  auto *editor = new EditorState();
  editor->plugin = plugin;

  EditorShellConfig config = {};
  editor->windowTitle = "ccomidi - Chn 1";
  config.title = editor->windowTitle.c_str();
  config.className = "ccomidi";




  EditorShellCallbacks callbacks = {};
  callbacks.userData = editor;
  callbacks.drawFrame = &draw_frame;

  editor->shell = editor_shell_create(plugin ? plugin->host : nullptr, config,
                                      callbacks);
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

} // namespace ccomidi
