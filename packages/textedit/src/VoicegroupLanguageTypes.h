#pragma once

#include <juce_gui_extra/juce_gui_extra.h>

struct VoicegroupCompletionItem
{
    juce::String label;
    juce::String detail;
    juce::String insertText;
    int replacementStartLine = 0;
    int replacementStartCharacter = 0;
    int replacementEndLine = 0;
    int replacementEndCharacter = 0;
};

enum class VoicegroupTabActionKind
{
    insertIndent,
    selectRange,
    moveCaret
};

struct VoicegroupTabAction
{
    VoicegroupTabActionKind kind = VoicegroupTabActionKind::insertIndent;
    int startLine = 0;
    int startCharacter = 0;
    int endLine = 0;
    int endCharacter = 0;
};
