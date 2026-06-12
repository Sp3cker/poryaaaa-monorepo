#pragma once

#include <juce_gui_extra/juce_gui_extra.h>

class VoicegroupTokeniser final : public juce::CodeTokeniser
{
public:
    enum TokenType
    {
        tokenType_error = 0,
        tokenType_comment,
        tokenType_keyword,
        tokenType_macro,
        tokenType_identifier,
        tokenType_number,
        tokenType_string,
        tokenType_operator,
        tokenType_punctuation
    };

    int readNextToken(juce::CodeDocument::Iterator& source) override;
    juce::CodeEditorComponent::ColourScheme getDefaultColourScheme() override;
};
