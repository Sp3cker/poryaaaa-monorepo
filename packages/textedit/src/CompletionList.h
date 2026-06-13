#pragma once

#include <juce_gui_extra/juce_gui_extra.h>

#include <vector>

struct VoicegroupCompletionItem;

class CompletionList final : public juce::Component,
                             private juce::ListBoxModel
{
public:
    CompletionList();
    ~CompletionList() override;

    void setItems(std::vector<VoicegroupCompletionItem> newItems);
    void clear();
    void resized() override;
    void paint(juce::Graphics& g) override;

private:
    int getNumRows() override;
    void paintListBoxItem(int rowNumber, juce::Graphics& g, int width, int height, bool rowIsSelected) override;

    juce::ListBox list { "Completions", this };
    std::vector<VoicegroupCompletionItem> items;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CompletionList)
};
