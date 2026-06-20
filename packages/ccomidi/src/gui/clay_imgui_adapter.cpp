#include "gui/clay_imgui_adapter.h"

#include <algorithm>
#include <cfloat>
#include <cmath>

namespace ccomidi
{
namespace
{

ImFont* clay_imgui_font(const ClayImguiFonts* fonts, uint16_t fontId)
{
    if (fonts && fontId == kClayImguiPrimaryFont && fonts->primary)
        return fonts->primary;
    if (fonts && fontId == kClayImguiSecondaryFont && fonts->secondary)
        return fonts->secondary;
    return ImGui::GetFont();
}

Clay_Dimensions measure_clay_text(Clay_StringSlice text, Clay_TextElementConfig* config, void* userData)
{
    const auto* fonts = static_cast<const ClayImguiFonts*>(userData);
    ImFont* font = clay_imgui_font(fonts, config ? config->fontId : 0);
    const float fontSize = config ? static_cast<float>(config->fontSize) : ImGui::GetFontSize();
    const char* begin = text.chars;
    const char* end = text.chars ? text.chars + text.length : nullptr;
    const ImVec2 size = font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, begin, end);
    return Clay_Dimensions{size.x, size.y};
}

void handle_clay_error(Clay_ErrorData) {}

float pixel_snap(float value)
{
    return std::floor(value);
}

ImVec2 clay_imgui_pos(Clay_BoundingBox box, ImVec2 origin)
{
    return ImVec2(pixel_snap(origin.x + box.x), pixel_snap(origin.y + box.y));
}

ImU32 clay_imgui_color(Clay_Color color)
{
    const auto channel = [](float value) { return std::clamp(static_cast<int>(std::lround(value)), 0, 255); };
    return IM_COL32(channel(color.r), channel(color.g), channel(color.b), channel(color.a));
}

bool ensure_clay_context(ClayImguiState* state)
{
    if (!state)
        return false;
    if (state->context)
    {
        Clay_SetCurrentContext(state->context);
        return true;
    }
    const uint32_t memorySize = Clay_MinMemorySize();
    state->memory.resize(memorySize);
    state->arena = Clay_CreateArenaWithCapacityAndMemory(state->memory.size(), state->memory.data());
    state->context =
        Clay_Initialize(state->arena, Clay_Dimensions{1.0f, 1.0f}, Clay_ErrorHandler{handle_clay_error, nullptr});
    return state->context != nullptr;
}

} // namespace

Clay_String clay_string(const std::string& value)
{
    return Clay_String{false, static_cast<int32_t>(value.size()), value.c_str()};
}

bool clay_imgui_begin_layout(
    ClayImguiState* state, ImVec2 dimensions, ImVec2 pointer, bool pointerDown, ClayImguiFonts* fonts)
{
    if (!ensure_clay_context(state))
        return false;
    Clay_SetLayoutDimensions(Clay_Dimensions{dimensions.x, dimensions.y});
    Clay_SetPointerState(Clay_Vector2{pointer.x, pointer.y}, pointerDown);
    Clay_SetMeasureTextFunction(measure_clay_text, fonts);
    Clay_BeginLayout();
    return true;
}

void clay_imgui_render_commands(ImDrawList* drawList,
                                Clay_RenderCommandArray commands,
                                ImVec2 origin,
                                const ClayImguiFonts& fonts,
                                ClayImguiCustomRenderer customRenderer,
                                void* customUserData)
{
    const ImVec2 mouse = ImGui::GetMousePos();
    for (int32_t i = 0; i < commands.length; ++i)
    {
        Clay_RenderCommand* command = Clay_RenderCommandArray_Get(&commands, i);
        if (!command)
            continue;
        const ImVec2 min = clay_imgui_pos(command->boundingBox, origin);
        const ImVec2 max = ImVec2(min.x + command->boundingBox.width, min.y + command->boundingBox.height);
        switch (command->commandType)
        {
        case CLAY_RENDER_COMMAND_TYPE_CUSTOM:
            if (customRenderer)
                customRenderer(drawList,
                               ClayImguiCustomCommand{command->renderData.custom.customData, min, max, mouse},
                               customUserData);
            break;
        case CLAY_RENDER_COMMAND_TYPE_TEXT:
        {
            const Clay_TextRenderData& text = command->renderData.text;
            ImFont* font = clay_imgui_font(&fonts, text.fontId);
            const char* begin = text.stringContents.chars;
            const char* end = begin ? begin + text.stringContents.length : nullptr;
            drawList->AddText(
                font, static_cast<float>(text.fontSize), min, clay_imgui_color(text.textColor), begin, end);
            break;
        }
        case CLAY_RENDER_COMMAND_TYPE_RECTANGLE:
        {
            const Clay_RectangleRenderData& rect = command->renderData.rectangle;
            drawList->AddRectFilled(min, max, clay_imgui_color(rect.backgroundColor), rect.cornerRadius.topLeft);
            break;
        }
        case CLAY_RENDER_COMMAND_TYPE_SCISSOR_START:
            drawList->PushClipRect(min, max, true);
            break;
        case CLAY_RENDER_COMMAND_TYPE_SCISSOR_END:
            drawList->PopClipRect();
            break;
        case CLAY_RENDER_COMMAND_TYPE_NONE:
        case CLAY_RENDER_COMMAND_TYPE_BORDER:
        case CLAY_RENDER_COMMAND_TYPE_IMAGE:
        case CLAY_RENDER_COMMAND_TYPE_OVERLAY_COLOR_START:
        case CLAY_RENDER_COMMAND_TYPE_OVERLAY_COLOR_END:
            break;
        }
    }
}

} // namespace ccomidi
