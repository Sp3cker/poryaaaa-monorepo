#include "VoicegroupCodeEditor.h"

namespace
{

constexpr auto hoverDelayMs = 200;

} // namespace

VoicegroupCodeEditor::VoicegroupCodeEditor(juce::CodeDocument& document, juce::CodeTokeniser* tokeniser)
    : juce::CodeEditorComponent(document, tokeniser)
{
}

VoicegroupCodeEditor::~VoicegroupCodeEditor()
{
    cancelPendingHover();
}

void VoicegroupCodeEditor::cancelPendingHover()
{
    stopTimer();
}

void VoicegroupCodeEditor::setDismissHoverCallback(DismissHoverCallback callback)
{
    dismissHoverCallback = std::move(callback);
}

void VoicegroupCodeEditor::setHoverCallback(HoverCallback callback)
{
    hoverCallback = std::move(callback);
}

void VoicegroupCodeEditor::setKeyCallback(KeyCallback callback)
{
    keyCallback = std::move(callback);
}

bool VoicegroupCodeEditor::keyPressed(const juce::KeyPress& key)
{
    if (keyCallback && keyCallback(key))
        return true;

    const auto caretPosition = getCaretPos().getPosition();
    const auto handled = juce::CodeEditorComponent::keyPressed(key);

    if (handled && getCaretPos().getPosition() != caretPosition)
        dismissHover();

    return handled;
}

void VoicegroupCodeEditor::mouseDown(const juce::MouseEvent& event)
{
    const auto caretPosition = getCaretPos().getPosition();
    juce::CodeEditorComponent::mouseDown(event);

    if (getCaretPos().getPosition() != caretPosition)
        dismissHover();
}

void VoicegroupCodeEditor::mouseDrag(const juce::MouseEvent& event)
{
    const auto caretPosition = getCaretPos().getPosition();
    juce::CodeEditorComponent::mouseDrag(event);

    if (getCaretPos().getPosition() != caretPosition)
        dismissHover();
}

void VoicegroupCodeEditor::mouseMove(const juce::MouseEvent& event)
{
    juce::CodeEditorComponent::mouseMove(event);
    dismissHover();
    pendingHoverPoint = event.getPosition();
    startTimer(hoverDelayMs);
}

void VoicegroupCodeEditor::mouseExit(const juce::MouseEvent& event)
{
    juce::CodeEditorComponent::mouseExit(event);
    dismissHover();
}

void VoicegroupCodeEditor::timerCallback()
{
    stopTimer();

    if (hoverCallback)
        hoverCallback(getPositionAt(pendingHoverPoint.x, pendingHoverPoint.y));
}

void VoicegroupCodeEditor::dismissHover()
{
    cancelPendingHover();

    if (dismissHoverCallback)
        dismissHoverCallback();
}
