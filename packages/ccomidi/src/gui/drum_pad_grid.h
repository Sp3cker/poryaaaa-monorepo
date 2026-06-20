#pragma once

#include "gui/clay_imgui_adapter.h"
#include "plugin/voicegroup_bridge.h"

namespace ccomidi
{

int draw_drum_pad_grid(
    ClayImguiState* clay, const VoiceSlot& voice, int activeNote, ImFont* sampleFont, ImFont* keyFont);

} // namespace ccomidi
