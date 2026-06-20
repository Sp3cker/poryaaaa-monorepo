#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "clay.h"
#include "imgui.h"

namespace ccomidi
{

inline constexpr uint16_t kClayImguiPrimaryFont = 1;
inline constexpr uint16_t kClayImguiSecondaryFont = 2;

struct ClayImguiState
{
    std::vector<char> memory = {};
    Clay_Arena arena = {};
    Clay_Context* context = nullptr;
};

struct ClayImguiFonts
{
    ImFont* primary = nullptr;
    ImFont* secondary = nullptr;
};

struct ClayImguiCustomCommand
{
    void* customData = nullptr;
    ImVec2 min = {};
    ImVec2 max = {};
    ImVec2 mouse = {};
};

using ClayImguiCustomRenderer = void (*)(ImDrawList* drawList, const ClayImguiCustomCommand& command, void* userData);

Clay_String clay_string(const std::string& value);
bool clay_imgui_begin_layout(
    ClayImguiState* state, ImVec2 dimensions, ImVec2 pointer, bool pointerDown, ClayImguiFonts* fonts);
void clay_imgui_render_commands(ImDrawList* drawList,
                                Clay_RenderCommandArray commands,
                                ImVec2 origin,
                                const ClayImguiFonts& fonts,
                                ClayImguiCustomRenderer customRenderer,
                                void* customUserData);

} // namespace ccomidi
