#pragma once

#include <juce_gui_extra/juce_gui_extra.h>

class HoverCard final : public juce::Component
{
public:
    HoverCard();

    void setText(const juce::String& text);
    void clear();
    void resized() override;
    void paint(juce::Graphics& g) override;

private:
    juce::TextEditor text;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(HoverCard)
};
