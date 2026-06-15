#pragma once

#include <juce_gui_extra/juce_gui_extra.h>

#include <functional>

class VoicegroupCodeEditor final : public juce::CodeEditorComponent, private juce::Timer
{
public:
    using HoverCallback = std::function<void(juce::CodeDocument::Position)>;
    using DismissHoverCallback = std::function<void()>;
    using KeyCallback = std::function<bool(const juce::KeyPress&)>;

    VoicegroupCodeEditor(juce::CodeDocument& document, juce::CodeTokeniser* tokeniser);
    ~VoicegroupCodeEditor() override;

    void cancelPendingHover();
    void setDismissHoverCallback(DismissHoverCallback callback);
    void setHoverCallback(HoverCallback callback);
    void setKeyCallback(KeyCallback callback);

private:
    bool keyPressed(const juce::KeyPress& key) override;
    void mouseDown(const juce::MouseEvent& event) override;
    void mouseDrag(const juce::MouseEvent& event) override;
    void mouseMove(const juce::MouseEvent& event) override;
    void mouseExit(const juce::MouseEvent& event) override;
    void timerCallback() override;
    void dismissHover();

    DismissHoverCallback dismissHoverCallback;
    HoverCallback hoverCallback;
    KeyCallback keyCallback;
    juce::Point<int> pendingHoverPoint;
};
