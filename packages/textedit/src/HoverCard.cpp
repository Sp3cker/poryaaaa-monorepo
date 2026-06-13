#include "HoverCard.h"

#include "GruvboxTheme.h"

HoverCard::HoverCard()
{
    addAndMakeVisible(text);
    text.setReadOnly(true);
    text.setMultiLine(true);
    text.setScrollbarsShown(true);
    text.setCaretVisible(false);
    text.setPopupMenuEnabled(true);
    text.setColour(juce::TextEditor::backgroundColourId, GruvboxTheme::background());
    text.setColour(juce::TextEditor::textColourId, GruvboxTheme::foreground());
    text.setColour(juce::TextEditor::highlightColourId, GruvboxTheme::selection());
    text.setColour(juce::TextEditor::highlightedTextColourId, GruvboxTheme::foreground());
    text.setColour(juce::TextEditor::outlineColourId, GruvboxTheme::gutterBackground());
    text.setColour(juce::TextEditor::focusedOutlineColourId, GruvboxTheme::gutterBackground());
    text.setText({}, juce::dontSendNotification);
}

void HoverCard::setText(const juce::String& newText)
{
    text.setText(newText, juce::dontSendNotification);
}

void HoverCard::resized()
{
    text.setBounds(getLocalBounds().reduced(1));
}

void HoverCard::paint(juce::Graphics& g)
{
    g.fillAll(GruvboxTheme::background());
    g.setColour(GruvboxTheme::gutterBackground());
    g.drawRect(getLocalBounds());
}
