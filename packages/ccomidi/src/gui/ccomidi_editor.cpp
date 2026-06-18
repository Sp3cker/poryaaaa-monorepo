#include "gui/ccomidi_editor.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <limits>
#include <string>

#include "imgui.h"

#include "gui/editor_shell.h"
#include "plugin/param_ids.h"

namespace ccomidi
{

enum class EditorPanel
{
    Controls,
    Drumset,
};

struct EditorState
{
    Plugin* plugin = nullptr;
    EditorShell* shell = nullptr;
    std::string windowTitle = {};
    EditorPanel activePanel = EditorPanel::Controls;
};

namespace
{

constexpr std::uint32_t kDefaultWidth = 980;
constexpr std::uint32_t kDefaultHeight = 620;
constexpr float kPadLabelFontSize = 16.0f;
constexpr double kIdleTimerHz = 30.0;

ImU32 output_channel_color(std::uint8_t outputChannel)
{
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
    return kChannelColors[std::clamp<int>(outputChannel, 0, kMaxSelectableOutputChannelIndex)];
}

std::string note_label(int note)
{
    static const char* kNames[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "Bb", "B"};
    note = std::clamp(note, 0, 127);
    return std::string(kNames[note % 12]) + std::to_string(note / 12 - 2);
}

std::string pad_name_label(const std::string& name)
{
    static const std::string kPrefix = "DirectSoundWaveData_";
    if (name.rfind(kPrefix, 0) == 0)
        return name.substr(kPrefix.size());
    return name;
}

ImU32 pad_name_color(const std::string& name)
{
    if (name.find("Kick") != std::string::npos)
        return IM_COL32(0xFF, 0x5A, 0x8C, 0xFF);
    if (name.find("Snare") != std::string::npos)
        return IM_COL32(0x30, 0xD7, 0xFF, 0xFF);
    if (name.find("Rim") != std::string::npos)
        return IM_COL32(0xB7, 0xE2, 0x5D, 0xFF);
    if (name.find("Ride") != std::string::npos)
        return IM_COL32(0x33, 0xE6, 0xE0, 0xFF);
    if (name.find("Perc") != std::string::npos)
        return IM_COL32(0xFF, 0x8A, 0x5A, 0xFF);
    return IM_COL32(0xD8, 0xBF, 0x7A, 0xFF);
}

float pixel_snap(float value)
{
    return std::floor(value);
}

ImVec2 pixel_snap(ImVec2 value)
{
    return ImVec2(pixel_snap(value.x), pixel_snap(value.y));
}

ImVec2 text_size(ImFont* font, float fontSize, const std::string& text)
{
    if (font)
        return font->CalcTextSizeA(fontSize, std::numeric_limits<float>::max(), 0.0f, text.c_str());
    return ImGui::CalcTextSize(text.c_str());
}

void add_pad_text(ImDrawList* drawList, ImFont* font, float fontSize, ImVec2 pos, ImU32 color, const std::string& text)
{
    if (font)
        drawList->AddText(font, fontSize, pixel_snap(pos), color, text.c_str());
    else
        drawList->AddText(pixel_snap(pos), color, text.c_str());
}

void queue_pad_note(Plugin* plugin, int note, bool on)
{
    if (!plugin || note < 0 || note > 127)
        return;
    {
        std::lock_guard<std::mutex> lock(plugin->stateMutex);
        if (on)
            plugin->core.queue_note_on(static_cast<std::uint8_t>(note));
        else
            plugin->core.queue_note_off(static_cast<std::uint8_t>(note));
    }
    request_host_param_sync(plugin);
}

void draw_panel_tab(EditorState* editor, EditorPanel panel, const char* label)
{
    const bool selected = editor->activePanel == panel;
    if (selected)
    {
        ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_TabActive));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImGui::GetStyleColorVec4(ImGuiCol_TabHovered));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImGui::GetStyleColorVec4(ImGuiCol_TabActive));
    }
    if (ImGui::Button(label))
        editor->activePanel = panel;
    if (selected)
        ImGui::PopStyleColor(3);
}

void draw_drumset_panel(Plugin* plugin, const VoiceSlot& voice, ImFont* padLabelFont)
{
    const int columns = 8;
    const ImGuiStyle& style = ImGui::GetStyle();
    const float gap = style.ItemSpacing.x;
    const float availableWidth = ImGui::GetContentRegionAvail().x;
    const float padWidth = std::max(1.0f, (availableWidth - gap * static_cast<float>(columns - 1)) / columns);
    const ImVec2 padSize(padWidth, 58.0f);
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    for (std::size_t i = 0; i < voice.drumset.size(); ++i)
    {
        if (i > 0 && i % columns != 0)
            ImGui::SameLine();
        const DrumPad& pad = voice.drumset[i];
        ImGui::PushID(static_cast<int>(i));
        ImGui::InvisibleButton("##pad", padSize);
        if (ImGui::IsItemActivated())
            queue_pad_note(plugin, pad.note, true);
        if (ImGui::IsItemDeactivated())
            queue_pad_note(plugin, pad.note, false);
        const bool active = ImGui::IsItemActive();
        const bool hovered = ImGui::IsItemHovered();
        const ImVec2 min = ImGui::GetItemRectMin();
        const ImVec2 max = ImGui::GetItemRectMax();
        const ImU32 bg = active    ? IM_COL32(0xEA, 0x38, 0xA8, 0xFF)
                         : hovered ? IM_COL32(0x3B, 0x3B, 0x3B, 0xFF)
                                   : IM_COL32(0x2A, 0x2A, 0x2A, 0xFF);
        drawList->AddRectFilled(min, max, bg, 6.0f);
        drawList->PushClipRect(min, max, true);
        const std::string keyLabel = note_label(pad.note);
        const std::string sampleLabel = pad_name_label(pad.name);
        const ImVec2 keySize = text_size(padLabelFont, kPadLabelFontSize, keyLabel);
        const float keyX = min.x + std::max(0.0f, (padSize.x - keySize.x) * 0.5f);
        add_pad_text(drawList,
                     padLabelFont,
                     kPadLabelFontSize,
                     ImVec2(keyX, min.y + 8.0f),
                     IM_COL32(0xF1, 0xF1, 0xF1, 0xFF),
                     keyLabel);
        add_pad_text(drawList,
                     padLabelFont,
                     kPadLabelFontSize,
                     ImVec2(min.x + 10.0f, min.y + 34.0f),
                     pad_name_color(pad.name),
                     sampleLabel);
        drawList->PopClipRect();
        ImGui::PopID();
    }
}

int field_count_for_type(CommandType type)
{
    switch (type)
    {
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

const char* field_label(CommandType type, int field)
{
    if (type == CommandType::MemAcc0C || type == CommandType::MemAcc10)
    {
        static const char* labels[] = {"Op", "Arg1", "Arg2", "Data"};
        return labels[field];
    }
    if (type == CommandType::Xcmd0D)
    {
        static const char* labels[] = {"B0", "B1", "B2", "B3"};
        return labels[field];
    }

    switch (field)
    {
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

std::uint8_t floor_to_u8(double value, std::uint8_t minValue, std::uint8_t maxValue)
{
    const double floored = std::floor(value);
    if (floored <= static_cast<double>(minValue))
        return minValue;
    if (floored >= static_cast<double>(maxValue))
        return maxValue;
    return static_cast<std::uint8_t>(floored);
}

bool is_table_command_type(CommandType type)
{
    return type == CommandType::None || !is_fixed_command_type(type);
}

void draw_parameter_controls(Plugin* plugin, std::size_t row, CommandType type, const UiRowSnapshot& rowSnapshot)
{
    const int activeFields = field_count_for_type(type);
    if (activeFields == 0)
    {
        ImGui::AlignTextToFramePadding();
        ImGui::TextDisabled("-");
        return;
    }

    if (activeFields == 1)
    {
        int value = static_cast<int>(rowSnapshot.values[0]);
        const std::string label = std::string(field_label(type, 0)) + "##v" + std::to_string(row) + "_0";
        ImGui::SetNextItemWidth(-1.0f);
        if (ImGui::SliderInt(label.c_str(), &value, 0, 127, "%d", ImGuiSliderFlags_AlwaysClamp))
        {
            apply_ui_param_change(plugin, row_param_id(static_cast<std::uint32_t>(row), RowParamSlot::Value0), value);
        }
        return;
    }

    const float fullWidth = ImGui::GetContentRegionAvail().x;
    const float spacing = ImGui::GetStyle().ItemSpacing.x;
    const float controlWidth =
        (fullWidth - (spacing * static_cast<float>(activeFields - 1))) / static_cast<float>(activeFields);

    for (int field = 0; field < activeFields; ++field)
    {
        if (field > 0)
            ImGui::SameLine();

        ImGui::PushItemWidth(controlWidth);
        int value = static_cast<int>(rowSnapshot.values[field]);
        const std::string label =
            std::string(field_label(type, field)) + "##v" + std::to_string(row) + "_" + std::to_string(field);
        if (ImGui::SliderInt(label.c_str(), &value, 0, 127, "%d", ImGuiSliderFlags_AlwaysClamp))
        {
            apply_ui_param_change(
                plugin,
                row_param_id(static_cast<std::uint32_t>(row),
                             static_cast<RowParamSlot>(static_cast<int>(RowParamSlot::Value0) + field)),
                value);
        }
        ImGui::PopItemWidth();
    }
}

void draw_frame(void* userData, std::uint32_t width, std::uint32_t height)
{
    auto* editor = static_cast<EditorState*>(userData);
    Plugin* plugin = editor ? editor->plugin : nullptr;
    if (!plugin)
        return;

    UiSnapshot snapshot = {};
    fill_ui_snapshot(plugin, &snapshot); // huh do we need this?
    reload_voicegroup_if_changed(plugin);

    ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
    ImGui::SetNextWindowSize(ImVec2(static_cast<float>(width), static_cast<float>(height)));
    const ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize;

    ImGui::Begin("##ccomidi", nullptr, flags);

    const std::uint8_t outputChannelIndex = floor_to_u8(snapshot.outputChannel, 0, kMaxSelectableOutputChannelIndex);
    int outputChannel = static_cast<int>(outputChannelIndex) + 1;
    outputChannel = std::clamp(outputChannel, 1, static_cast<int>(kSelectableOutputChannelCount));
    bool programEnabled = std::floor(snapshot.programEnabled) >= 1.0;
    int program = static_cast<int>(std::floor(snapshot.program));
    program = std::clamp(program, 0, 127);
    const bool hasVoices = !plugin->voiceLoad.slots.empty();
    const VoiceSlot* currentVoice = nullptr;
    for (const VoiceSlot& slot : plugin->voiceLoad.slots)
    {
        if (slot.program == program)
        {
            currentVoice = &slot;
            break;
        }
    }
    const bool selectedVoiceIsDrumset = currentVoice && !currentVoice->drumset.empty();
    if (!selectedVoiceIsDrumset)
        editor->activePanel = EditorPanel::Controls;
    const std::string windowTitle = "ccomidi - Chn " + std::to_string(outputChannel);
    if (editor->windowTitle != windowTitle)
    {
        editor->windowTitle = windowTitle;
        editor_shell_set_title(editor->shell, editor->windowTitle.c_str());
    }
    ImFont* boldFont = editor_shell_bold_font(editor->shell);
    ImFont* padLabelFont = editor_shell_pixel_font(editor->shell);

    if (boldFont)
        ImGui::PushFont(boldFont, 0.0f);
    ImGui::TextUnformatted("ccomidi");
    if (boldFont)
        ImGui::PopFont();
    if (selectedVoiceIsDrumset)
    {
        ImGui::SameLine(0.0f, 18.0f);
        draw_panel_tab(editor, EditorPanel::Controls, "Controls");
        ImGui::SameLine(0.0f, 0.0f);
        draw_panel_tab(editor, EditorPanel::Drumset, "Drumset");
    }
    ImGui::Separator();

    const char* reloadVoicesLabel = "Reload Voices";
    const float reloadVoicesWidth = ImGui::CalcTextSize(reloadVoicesLabel).x + ImGui::GetStyle().FramePadding.x * 2.0f;
    if (ImGui::BeginTable(
            "program_row", 3, ImGuiTableFlags_SizingStretchProp, ImVec2(ImGui::GetContentRegionAvail().x, 0.0f)))
    {
        ImGui::TableSetupColumn("##emit_program_col", ImGuiTableColumnFlags_WidthFixed, ImGui::GetFrameHeight());
        ImGui::TableSetupColumn("##program_col", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("##reload_voices_col", ImGuiTableColumnFlags_WidthFixed, reloadVoicesWidth);
        ImGui::TableNextRow();

        ImGui::TableSetColumnIndex(0);
        if (ImGui::Checkbox("##emit_program", &programEnabled))
            apply_ui_param_change(plugin, kParamProgramEnabled, programEnabled ? 1.0 : 0.0);

        ImGui::TableSetColumnIndex(1);
        ImGui::BeginDisabled(!programEnabled);
        if (hasVoices)
        {
            char preview[288];
            if (currentVoice)
                std::snprintf(preview, sizeof(preview), "%03d  %s", currentVoice->program, currentVoice->name.c_str());
            else
                std::snprintf(preview, sizeof(preview), "%03d  (empty slot)", program);
            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
            if (boldFont)
                ImGui::PushFont(boldFont, 0.0f);
            const bool comboOpen = ImGui::BeginCombo("##program", preview);
            if (boldFont)
                ImGui::PopFont();
            if (comboOpen)
            {
                for (const VoiceSlot& slot : plugin->voiceLoad.slots)
                {
                    char label[288];
                    std::snprintf(label, sizeof(label), "%03d  %s", slot.program, slot.name.c_str());
                    const bool selected = slot.program == program;
                    if (ImGui::Selectable(label, selected))
                        apply_ui_param_change(plugin, kParamProgram, static_cast<double>(slot.program));
                    if (selected)
                        ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }
        }
        else
        {
            ImGui::AlignTextToFramePadding();
            ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "Voicegroup: %s", plugin->voiceLoad.error.c_str());
            if (!plugin->voiceLoad.statePath.empty())
                ImGui::TextDisabled("state: %s", plugin->voiceLoad.statePath.c_str());
        }
        ImGui::EndDisabled();

        ImGui::TableSetColumnIndex(2);
        if (ImGui::Button(reloadVoicesLabel))
        {
            std::lock_guard<std::mutex> lock(plugin->stateMutex);
            plugin->voiceLoad = voicegroup_bridge_load_state();
        }
        ImGui::EndTable();
    }

    ImGui::Spacing();

    if (editor->activePanel == EditorPanel::Drumset && currentVoice)
    {
        draw_drumset_panel(plugin, *currentVoice, padLabelFont);
        ImGui::End();
        return;
    }

    if (boldFont)
        ImGui::PushFont(boldFont, 0.0f);
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted("Channel");
    if (boldFont)
        ImGui::PopFont();
    ImGui::SameLine();
    ImGui::SetNextItemWidth(-1.0f);
    ImVec4 channelColor = ImGui::ColorConvertU32ToFloat4(output_channel_color(outputChannelIndex));
    channelColor.w = 0.5f;
    ImGui::PushStyleColor(ImGuiCol_FrameBg, channelColor);
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, channelColor);
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive, channelColor);
    if (ImGui::SliderInt("##output_channel", &outputChannel, 1, static_cast<int>(kSelectableOutputChannelCount)))
        apply_ui_param_change(plugin, kParamOutputChannel, static_cast<double>(outputChannel - 1));
    ImGui::PopStyleColor(3);

    ImGui::Spacing();

    if (ImGui::BeginTable("fixed_rows", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg))
    {
        ImGui::TableSetupColumn("Command", ImGuiTableColumnFlags_WidthFixed, 120.0f);
        ImGui::TableSetupColumn("On", ImGuiTableColumnFlags_WidthFixed, 40.0f);
        ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableHeadersRow();

        for (std::size_t row = 0; row < kFixedCommandRowCount; ++row)
        {
            const CommandType type = fixed_command_type_for_row(row);

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted(command_type_name(type));

            ImGui::TableSetColumnIndex(1);
            bool enabled = std::floor(snapshot.rows[row].enabled) >= 1.0;
            if (ImGui::Checkbox(("##fixed_enabled" + std::to_string(row)).c_str(), &enabled))
                apply_ui_param_change(
                    plugin, row_param_id(static_cast<std::uint32_t>(row), RowParamSlot::Enabled), enabled ? 1.0 : 0.0);

            ImGui::TableSetColumnIndex(2);
            draw_parameter_controls(plugin, row, type, snapshot.rows[row]);
        }

        ImGui::EndTable();
    }

    ImGui::Spacing();
    ImGui::TextUnformatted("Additional Commands");
    if (ImGui::BeginTable("rows", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY))
    {
        ImGui::TableSetupColumn("On", ImGuiTableColumnFlags_WidthFixed, 40.0f);
        ImGui::TableSetupColumn("Command", ImGuiTableColumnFlags_WidthStretch, 1.4f);
        ImGui::TableSetupColumn("Parameters", ImGuiTableColumnFlags_WidthStretch, 2.2f);
        ImGui::TableHeadersRow();

        for (std::size_t row = kFixedCommandRowCount; row < kMaxCommandRows; ++row)
        {
            const CommandType type = static_cast<CommandType>(static_cast<int>(std::floor(snapshot.rows[row].type)));

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            bool enabled = std::floor(snapshot.rows[row].enabled) >= 1.0;
            if (ImGui::Checkbox(("##enabled" + std::to_string(row)).c_str(), &enabled))
                apply_ui_param_change(
                    plugin, row_param_id(static_cast<std::uint32_t>(row), RowParamSlot::Enabled), enabled ? 1.0 : 0.0);

            ImGui::TableSetColumnIndex(1);
            int typeIndex = static_cast<int>(type);
            if (ImGui::BeginCombo(("##type" + std::to_string(row)).c_str(), command_type_name(type)))
            {
                for (int candidate = static_cast<int>(CommandType::None);
                     candidate <= static_cast<int>(CommandType::Xcmd0D);
                     ++candidate)
                {
                    const auto candidateType = static_cast<CommandType>(candidate);
                    if (!is_table_command_type(candidateType))
                        continue;
                    const bool selected = candidate == typeIndex;
                    if (ImGui::Selectable(command_type_name(candidateType), selected))
                    {
                        apply_ui_param_change(plugin,
                                              row_param_id(static_cast<std::uint32_t>(row), RowParamSlot::Type),
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

EditorState* editor_create(Plugin* plugin)
{
    auto* editor = new EditorState();
    editor->plugin = plugin;

    EditorShellConfig config = {};
    editor->windowTitle = "ccomidi - Chn 1";
    config.title = editor->windowTitle.c_str();
    config.className = "ccomidi";

    EditorShellCallbacks callbacks = {};
    callbacks.userData = editor;
    callbacks.drawFrame = &draw_frame;

    editor->shell = editor_shell_create(plugin ? plugin->host : nullptr, config, callbacks);
    if (!editor->shell)
    {
        delete editor;
        return nullptr;
    }
    return editor;
}

void editor_prepare_destroy(EditorState* editor)
{
    if (!editor)
        return;
    editor->plugin = nullptr;
    editor_shell_prepare_destroy(editor->shell);
}

void editor_destroy(EditorState* editor)
{
    if (!editor)
        return;
    editor_shell_destroy(editor->shell);
    delete editor;
}

bool editor_show(EditorState* editor)
{
    return editor && editor_shell_show(editor->shell);
}

bool editor_hide(EditorState* editor)
{
    return editor && editor_shell_hide(editor->shell);
}

bool editor_set_parent(EditorState* editor, std::uintptr_t nativeParent)
{
    return editor && editor_shell_set_parent(editor->shell, nativeParent);
}

bool editor_set_size(EditorState* editor, std::uint32_t width, std::uint32_t height)
{
    return editor && editor_shell_set_size(editor->shell, width, height);
}

void editor_get_size(EditorState* editor, std::uint32_t* width, std::uint32_t* height)
{
    if (!editor)
    {
        if (width)
            *width = kDefaultWidth;
        if (height)
            *height = kDefaultHeight;
        return;
    }
    editor_shell_get_size(editor->shell, width, height);
}

bool editor_can_resize(EditorState* editor)
{
    return editor && editor_shell_can_resize(editor->shell);
}

bool editor_was_closed(EditorState* editor)
{
    return editor && editor_shell_was_closed(editor->shell);
}

void editor_start_internal_timer(EditorState* editor)
{
    if (editor)
        editor_shell_start_timer(editor->shell, kIdleTimerHz);
}

void editor_stop_internal_timer(EditorState* editor)
{
    if (editor)
        editor_shell_stop_timer(editor->shell);
}

} // namespace ccomidi
