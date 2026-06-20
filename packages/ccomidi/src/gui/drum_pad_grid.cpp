#include "gui/drum_pad_grid.h"

#include <algorithm>
#include <cfloat>
#include <string>
#include <vector>

#include "clay.h"
#include "imgui.h"

#include "typographic_scale.h"

namespace ccomidi
{
namespace
{

namespace text = poryaaaa::gui::text;

struct PadView
{
    Clay_String sample = {};
    std::string key = {};
    Clay_Color sampleColor = {};
    int note = -1;
    bool active = false;
    bool accidental = false;
};

std::string note_label(int note)
{
    static const char* kNames[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "Bb", "B"};
    note = std::clamp(note, 0, 127);
    return std::string(kNames[note % 12]) + std::to_string(note / 12 - 2);
}

Clay_Color sample_color(const std::string& name)
{
    if (name.find("Kick") != std::string::npos)
        return {0xFF, 0x5A, 0x8C, 0xFF};
    if (name.find("Snare") != std::string::npos)
        return {0x30, 0xD7, 0xFF, 0xFF};
    if (name.find("Rim") != std::string::npos)
        return {0xB7, 0xE2, 0x5D, 0xFF};
    if (name.find("Ride") != std::string::npos)
        return {0x33, 0xE6, 0xE0, 0xFF};
    if (name.find("Perc") != std::string::npos)
        return {0xFF, 0x8A, 0x5A, 0xFF};
    return {0xD8, 0xBF, 0x7A, 0xFF};
}

ImU32 pad_background_color(const PadView& data, bool hovered)
{
    return data.active       ? (data.accidental ? IM_COL32(0x52, 0x52, 0x52, 0xFF) : IM_COL32(0x70, 0x70, 0x70, 0xFF))
           : hovered         ? (data.accidental ? IM_COL32(0x32, 0x32, 0x32, 0xFF) : IM_COL32(0x4A, 0x4A, 0x4A, 0xFF))
           : data.accidental ? IM_COL32(0x22, 0x22, 0x22, 0xFF)
                             : IM_COL32(0x3A, 0x3A, 0x3A, 0xFF);
}

bool is_accidental(int note)
{
    const int pitchClass = note % 12;
    return pitchClass == 1 || pitchClass == 3 || pitchClass == 6 || pitchClass == 8 || pitchClass == 10;
}

void render_pad(ImDrawList* drawList, const ClayImguiCustomCommand& command, void* userData)
{
    const auto* pad = static_cast<const PadView*>(command.customData);
    if (!pad)
        return;
    auto* hoveredNote = static_cast<int*>(userData);
    const bool hovered = command.mouse.x >= command.min.x && command.mouse.x < command.max.x &&
                         command.mouse.y >= command.min.y && command.mouse.y < command.max.y;
    if (hovered && hoveredNote)
        *hoveredNote = pad->note;
    drawList->AddRectFilled(command.min, command.max, pad_background_color(*pad, hovered), 6.0f);
    const ImVec2 keyMin(command.min.x, command.max.y - text::Lg - text::Sm);
    drawList->AddRectFilled(keyMin, command.max, IM_COL32(0x00, 0x00, 0x00, 0x28));
    drawList->AddLine(keyMin, ImVec2(command.max.x, keyMin.y), IM_COL32(0xFF, 0xFF, 0xFF, 0x18));
}

void draw_empty_pad(uint32_t padIndex, float padHeight)
{
    CLAY(CLAY_IDI("drum_pad_empty", padIndex),
         {
             .layout =
                 {
                     .sizing = {CLAY_SIZING_GROW(0, FLT_MAX), CLAY_SIZING_FIXED(padHeight)},
                 },
         })
    {
    }
}

void draw_pad(uint32_t padIndex, const PadView& view, float padHeight)
{
    CLAY(CLAY_IDI("drum_pad", padIndex),
         {
             .layout =
                 {
                     .sizing = {CLAY_SIZING_GROW(0, FLT_MAX), CLAY_SIZING_FIXED(padHeight)},
                     .layoutDirection = CLAY_TOP_TO_BOTTOM,
                 },
             .custom = {.customData = const_cast<PadView*>(&view)},
         })
    {
        CLAY(CLAY_IDI("drum_pad_sample", padIndex),
             {
                 .layout =
                     {
                         .sizing = {CLAY_SIZING_GROW(0, FLT_MAX), CLAY_SIZING_GROW(0, FLT_MAX)},
                         .padding = {static_cast<uint16_t>(text::Sm),
                                     static_cast<uint16_t>(text::Sm),
                                     static_cast<uint16_t>(text::Sm),
                                     0},
                     },
                 .clip = {.horizontal = true, .vertical = true},
             })
        {
            CLAY_TEXT(view.sample,
                      CLAY_TEXT_CONFIG({.textColor = view.sampleColor,
                                        .fontId = kClayImguiPrimaryFont,
                                        .fontSize = static_cast<uint16_t>(text::Xl),
                                        .lineHeight = static_cast<uint16_t>(text::Xl)}));
        }
        CLAY(CLAY_IDI("drum_pad_key", padIndex),
             {
                 .layout =
                     {
                         .sizing = {CLAY_SIZING_GROW(0, FLT_MAX), CLAY_SIZING_FIXED(text::Lg + text::Sm)},
                         .padding = {static_cast<uint16_t>(text::Sm), 0, 0, 0},
                     },
                 .clip = {.horizontal = true, .vertical = true},
             })
        {
            CLAY_TEXT(clay_string(view.key),
                      CLAY_TEXT_CONFIG({.textColor = {0xF1, 0xF1, 0xF1, 0xFF},
                                        .fontId = kClayImguiSecondaryFont,
                                        .fontSize = static_cast<uint16_t>(text::Lg),
                                        .lineHeight = static_cast<uint16_t>(text::Lg),
                                        .wrapMode = CLAY_TEXT_WRAP_NONE}));
        }
    }
}

} // namespace

int draw_drum_pad_grid(
    ClayImguiState* clay, const VoiceSlot& voice, int activeNote, ImFont* sampleFont, ImFont* keyFont)
{
    const int columns = 8;
    const float gap = ImGui::GetStyle().ItemSpacing.x;
    const float availableWidth = ImGui::GetContentRegionAvail().x;
    const float padHeight = text::Lg + text::Xl * 2.0f + text::Lg + text::Sm;
    const int rows = static_cast<int>((voice.drumset.size() + columns - 1) / columns);
    if (rows <= 0)
        return activeNote;
    const float totalHeight = padHeight * static_cast<float>(rows) + gap * static_cast<float>(rows - 1);
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImGui::InvisibleButton("##clay_drumset", ImVec2(availableWidth, totalHeight));
    const ImVec2 origin = ImGui::GetItemRectMin();
    std::vector<PadView> pads;
    pads.reserve(voice.drumset.size());
    static const std::string kSamplePrefix = "DirectSoundWaveData_";
    for (const DrumPad& pad : voice.drumset)
    {
        const bool hasPrefix = pad.name.rfind(kSamplePrefix, 0) == 0;
        const std::size_t sampleOffset = hasPrefix ? kSamplePrefix.size() : 0;
        pads.push_back(PadView{
            Clay_String{false, static_cast<int32_t>(pad.name.size() - sampleOffset), pad.name.c_str() + sampleOffset},
            note_label(pad.note),
            sample_color(pad.name),
            pad.note,
            activeNote == pad.note,
            is_accidental(pad.note)});
    }
    const ImVec2 mouse = ImGui::GetMousePos();
    ClayImguiFonts fonts{sampleFont, keyFont};
    if (!clay_imgui_begin_layout(clay,
                                 ImVec2(availableWidth, totalHeight),
                                 ImVec2(mouse.x - origin.x, mouse.y - origin.y),
                                 ImGui::IsMouseDown(ImGuiMouseButton_Left),
                                 &fonts))
        return activeNote;
    CLAY(CLAY_ID("drumset"),
         {
             .layout =
                 {
                     .sizing = {CLAY_SIZING_FIXED(availableWidth), CLAY_SIZING_FIXED(totalHeight)},
                     .childGap = static_cast<uint16_t>(gap),
                     .layoutDirection = CLAY_TOP_TO_BOTTOM,
                 },
         })
    {
        for (int row = 0; row < rows; ++row)
        {
            CLAY(CLAY_IDI("drum_row", row),
                 {
                     .layout =
                         {
                             .sizing = {CLAY_SIZING_GROW(0, FLT_MAX), CLAY_SIZING_FIXED(padHeight)},
                             .childGap = static_cast<uint16_t>(gap),
                             .layoutDirection = CLAY_LEFT_TO_RIGHT,
                         },
                 })
            {
                for (int column = 0; column < columns; ++column)
                {
                    const std::size_t padIndex = static_cast<std::size_t>(row * columns + column);
                    if (padIndex >= pads.size())
                        draw_empty_pad(static_cast<uint32_t>(padIndex), padHeight);
                    else
                        draw_pad(static_cast<uint32_t>(padIndex), pads[padIndex], padHeight);
                }
            }
        }
    }
    int hoveredNote = -1;
    clay_imgui_render_commands(drawList, Clay_EndLayout(0.0f), origin, fonts, render_pad, &hoveredNote);
    return ImGui::IsMouseDown(ImGuiMouseButton_Left) ? hoveredNote : -1;
}

} // namespace ccomidi
