#pragma once

#include <juce_gui_extra/juce_gui_extra.h>

#include <functional>
#include <set>
#include <vector>

class ExtractLineSelector final : public juce::Component, private juce::Timer
{
public:
    using SelectionChangedCallback = std::function<void()>;

    void begin(juce::CodeEditorComponent& editor, juce::CodeDocument& document);
    void end();
    void setSelectionChangedCallback(SelectionChangedCallback callback);

    bool isActive() const
    {
        return editor != nullptr && document != nullptr;
    }
    bool hasSelection() const
    {
        return !selectedLines.empty();
    }
    int getSelectedLineCount() const
    {
        return int(selectedLines.size());
    }
    juce::String getSelectedVoiceLinesText() const;
    juce::String getStatusText() const;

    void paint(juce::Graphics& g) override;
    void mouseDown(const juce::MouseEvent& event) override;

private:
    void timerCallback() override;
    void collectExtractableLines();
    void toggleLine(int line);
    juce::Rectangle<int> checkboxBoundsForLine(int line) const;

    juce::CodeEditorComponent* editor = nullptr;
    juce::CodeDocument* document = nullptr;
    std::vector<int> extractableLines;
    std::set<int> selectedLines;
    SelectionChangedCallback selectionChangedCallback;
};
