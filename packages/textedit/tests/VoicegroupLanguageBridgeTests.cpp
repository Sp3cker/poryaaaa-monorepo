#include "VoicegroupLanguageBridgeTests.h"

#include "VoicegroupLanguageBridge.h"

#include <algorithm>
#include <iostream>

bool runVoicegroupLanguageBridgeTests()
{
    if (!juce::File(TEXTEDIT_VOICEGROUP_BRIDGE_PATH).existsAsFile())
    {
        std::cerr << "voicegroup bridge integration skipped: missing " << TEXTEDIT_VOICEGROUP_BRIDGE_PATH << "\n";
        return true;
    }

    auto bridge = VoicegroupLanguageBridge{};
    if (!bridge.isAvailable())
    {
        std::cerr << "voicegroup bridge unavailable: " << bridge.getStatusText() << "\n";
        return false;
    }

    if (!bridge.syncDocument("file:///textedit/voicegroup.inc", "\tvoice_dir"))
    {
        std::cerr << "voicegroup bridge sync failed: " << bridge.getStatusText() << "\n";
        return false;
    }

    const auto completions = bridge.completions(0, 10);
    const auto completion = std::find_if(
        completions.begin(), completions.end(), [](const auto& item) { return item.label == "voice_directsound"; });
    if (completion == completions.end())
    {
        std::cerr << "voicegroup bridge completion failed: missing voice_directsound\n";
        return false;
    }

    if (completion->insertText != "voice_directsound 60, 0, DirectSoundWaveData_, 255, 0, 255, 127" ||
        completion->replacementStartLine != 0 || completion->replacementStartCharacter != 1 ||
        completion->replacementEndLine != 0 || completion->replacementEndCharacter != 10)
    {
        std::cerr << "voicegroup bridge completion failed: bad edit payload\n";
        return false;
    }

    if (!bridge.syncDocument("file:///textedit/voicegroup.inc",
                             "\tvoice_directsound 60, 0, "
                             "DirectSoundWaveData_piano, 255, 0, 255, 127"))
    {
        std::cerr << "voicegroup bridge hover sync failed: " << bridge.getStatusText() << "\n";
        return false;
    }

    const auto hover = bridge.hover(0, 20);
    if (!hover.has_value() || !hover->contains("base_midi_key"))
    {
        std::cerr << "voicegroup bridge hover failed\n";
        return false;
    }

    if (!bridge.syncDocument("file:///textedit/voicegroup.inc", "\tvoice_square_1 60, 0, 0, 2, 0, 0, 15, 0"))
    {
        std::cerr << "voicegroup bridge tab sync failed: " << bridge.getStatusText() << "\n";
        return false;
    }

    const auto tabAction = bridge.tabAction(0, 20, 0, 21);
    if (!tabAction.has_value() || tabAction->kind != VoicegroupTabActionKind::selectRange ||
        tabAction->startLine != 0 || tabAction->startCharacter != 23 || tabAction->endLine != 0 ||
        tabAction->endCharacter != 24)
    {
        std::cerr << "voicegroup bridge tab action failed\n";
        return false;
    }

    return true;
}
