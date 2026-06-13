#include "CompletionList.h"

#include "GruvboxTheme.h"

CompletionList::CompletionList()
{
    addAndMakeVisible(list);
    list.setModel(this);
    list.setRowHeight(22);
    list.setMultipleSelectionEnabled(false);
    list.setColour(juce::ListBox::backgroundColourId, GruvboxTheme::background());
    list.setColour(juce::ListBox::outlineColourId, GruvboxTheme::gutterBackground());
    list.setOutlineThickness(1);
}

CompletionList::~CompletionList()
{
    list.setModel(nullptr);
}

void CompletionList::resized()
{
    list.setBounds(getLocalBounds().reduced(1));
}

void CompletionList::paint(juce::Graphics& g)
{
    g.fillAll(GruvboxTheme::background());
    g.setColour(GruvboxTheme::gutterBackground());
    g.drawRect(getLocalBounds());
}

int CompletionList::getNumRows()
{
    return 0;
}

void CompletionList::paintListBoxItem(int rowNumber, juce::Graphics& g, int width, int height, bool rowIsSelected)
{
    juce::ignoreUnused(rowNumber, g, width, height, rowIsSelected);
}
