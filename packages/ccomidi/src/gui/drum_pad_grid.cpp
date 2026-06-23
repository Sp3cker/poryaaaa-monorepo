#include "gui/drum_pad_grid.h"

#include <algorithm>
#include <string>
#include <vector>

#include "typographic_scale.h"

namespace ccomidi
{
namespace
{

namespace text = poryaaaa::gui::text;

struct PadView
{
    std::string sample = {};
    std::string key = {};
    ImU32 sampleColor = 0;
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

ImU32 sample_color(const std::string& name)
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

void draw_pad(ImDrawList* drawList, const PadView& pad, ImVec2 min, ImVec2 max, ImFont* sampleFont, ImFont* keyFont)
{
    const ImVec2 mouse = ImGui::GetMousePos();
    const bool hovered = mouse.x >= min.x && mouse.x < max.x && mouse.y >= min.y && mouse.y < max.y;

    drawList->AddRectFilled(min, max, pad_background_color(pad, hovered), 6.0f);

    const ImVec2 keyMin(min.x, max.y - text::Lg - text::Sm);
    drawList->AddRectFilled(keyMin, max, IM_COL32(0x00, 0x00, 0x00, 0x28));
    drawList->AddLine(keyMin, ImVec2(max.x, keyMin.y), IM_COL32(0xFF, 0xFF, 0xFF, 0x18));

    if (!sampleFont)
        sampleFont = ImGui::GetFont();
    if (!keyFont)
        keyFont = ImGui::GetFont();

    const ImVec2 samplePos(min.x + text::Sm, min.y + text::Sm);
    const ImVec4 sampleClip(samplePos.x, samplePos.y, max.x - text::Sm, keyMin.y);
    drawList->AddText(sampleFont,
                      text::Xl,
                      samplePos,
                      pad.sampleColor,
                      pad.sample.c_str(),
                      pad.sample.c_str() + pad.sample.size(),
                      0.0f,
                      &sampleClip);

    const ImVec2 keyPos(keyMin.x + text::Sm, keyMin.y);
    const ImVec4 keyClip(keyPos.x, keyPos.y, max.x, max.y);
    drawList->AddText(keyFont,
                      text::Lg,
                      keyPos,
                      IM_COL32(0xF1, 0xF1, 0xF1, 0xFF),
                      pad.key.c_str(),
                      pad.key.c_str() + pad.key.size(),
                      0.0f,
                      &keyClip);
}

} // namespace

int draw_drum_pad_grid(const VoiceSlot& voice, int activeNote, ImFont* sampleFont, ImFont* keyFont)
{
    const int columns = 8;
    const float gap = ImGui::GetStyle().ItemSpacing.x;
    const float availableWidth = ImGui::GetContentRegionAvail().x;
    const float padWidth = std::max(0.0f, (availableWidth - gap * static_cast<float>(columns - 1)) / columns);
    const float padHeight = text::Lg + text::Xl * 2.0f + text::Lg + text::Sm;
    const int rows = static_cast<int>((voice.drumset.size() + columns - 1) / columns);
    if (rows <= 0)
        return activeNote;
    const float totalHeight = padHeight * static_cast<float>(rows) + gap * static_cast<float>(rows - 1);
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImGui::InvisibleButton("##drumset", ImVec2(availableWidth, totalHeight));
    const ImVec2 origin = ImGui::GetItemRectMin();
    std::vector<PadView> pads;
    pads.reserve(voice.drumset.size());
    static const std::string kSamplePrefix = "DirectSoundWaveData_";
    for (const DrumPad& pad : voice.drumset)
    {
        const bool hasPrefix = pad.name.rfind(kSamplePrefix, 0) == 0;
        const std::size_t sampleOffset = hasPrefix ? kSamplePrefix.size() : 0;
        pads.push_back(PadView{pad.name.substr(sampleOffset),
                               note_label(pad.note),
                               sample_color(pad.name),
                               pad.note,
                               activeNote == pad.note,
                               is_accidental(pad.note)});
    }
    const ImVec2 mouse = ImGui::GetMousePos();
    int hoveredNote = -1;
    for (int row = 0; row < rows; ++row)
    {
        const float y = origin.y + static_cast<float>(row) * (padHeight + gap);
        for (int column = 0; column < columns; ++column)
        {
            const std::size_t padIndex = static_cast<std::size_t>(row * columns + column);
            if (padIndex >= pads.size())
                continue;

            const float x = origin.x + static_cast<float>(column) * (padWidth + gap);
            const ImVec2 min(x, y);
            const ImVec2 max(x + padWidth, y + padHeight);
            if (mouse.x >= min.x && mouse.x < max.x && mouse.y >= min.y && mouse.y < max.y)
                hoveredNote = pads[padIndex].note;
            draw_pad(drawList, pads[padIndex], min, max, sampleFont, keyFont);
        }
    }
    return ImGui::IsMouseDown(ImGuiMouseButton_Left) ? hoveredNote : -1;
}

} // namespace ccomidi
