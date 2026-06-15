#pragma once

#include <juce_gui_extra/juce_gui_extra.h>

#include <functional>

class TopToolBar final : public juce::Component
{
public:
    enum class Mode
    {
        normal,
        extract
    };

    TopToolBar();

    void resized() override;
    void paint(juce::Graphics& g) override;

    void setMode(Mode mode);
    void setDocumentLoaded(bool loaded);
    void setCanSaveExtract(bool canSave);

    std::function<void()> onSave;
    std::function<void()> onSaveAs;
    std::function<void()> onEnterExtract;
    std::function<void()> onSaveExtractAs;
    std::function<void()> onCancelExtract;

private:
    void configureButton(juce::TextButton& button);
    void updateButtonState();

    juce::TextButton saveButton{"Save"};
    juce::TextButton saveAsButton{"Save As"};
    juce::TextButton extractButton{"Extract"};
    juce::TextButton extractSaveAsButton{"Save As..."};
    juce::TextButton extractCancelButton{"Cancel"};
    juce::FlexBox flex;
    Mode mode = Mode::normal;
    bool documentLoaded = true;
    bool canSaveExtract = false;
};
