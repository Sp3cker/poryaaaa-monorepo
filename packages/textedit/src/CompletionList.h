#pragma once

#include <juce_gui_extra/juce_gui_extra.h>

#include <functional>
#include <vector>

struct VoicegroupCompletionItem;

class CompletionList final : public juce::Component,
                             private juce::ListBoxModel
{
public:
    using AcceptCallback = std::function<void(const VoicegroupCompletionItem&)>;

    CompletionList();
    ~CompletionList() override;

    void setItems(std::vector<VoicegroupCompletionItem> newItems);
    void clear();
    void setAcceptCallback(AcceptCallback callback);
    bool acceptSelectedItem();
    void resized() override;
    void paint(juce::Graphics& g) override;

private:
    int getNumRows() override;
    void paintListBoxItem(int rowNumber, juce::Graphics& g, int width, int height, bool rowIsSelected) override;
    void listBoxItemDoubleClicked(int row, const juce::MouseEvent& event) override;
    void returnKeyPressed(int lastRowSelected) override;

    juce::ListBox list { "Completions", this };
    std::vector<VoicegroupCompletionItem> items;
    AcceptCallback acceptCallback;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CompletionList)
};
