#pragma once

#include "imgui.h"
#include "plugin/voicegroup_bridge.h"

namespace ccomidi
{

int draw_drum_pad_grid(const VoiceSlot& voice, int activeNote, ImFont* sampleFont, ImFont* keyFont);

} // namespace ccomidi
