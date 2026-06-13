#pragma once

#include <juce_gui_extra/juce_gui_extra.h>

class CompletionList final : public juce::Component,
                             private juce::ListBoxModel
{
public:
    CompletionList();
    ~CompletionList() override;

    void resized() override;
    void paint(juce::Graphics& g) override;

private:
    int getNumRows() override;
    void paintListBoxItem(int rowNumber, juce::Graphics& g, int width, int height, bool rowIsSelected) override;

    juce::ListBox list { "Completions", this };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CompletionList)
};
