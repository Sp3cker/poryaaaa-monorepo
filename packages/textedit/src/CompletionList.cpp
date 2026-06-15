#include "CompletionList.h"

#include "GruvboxTheme.h"
#include "VoicegroupLanguageService.h"

CompletionList::CompletionList()
{
    addAndMakeVisible(list);
    list.setModel(this);
    list.setRowHeight(22);
    list.setMultipleSelectionEnabled(false);
    list.setColour(juce::ListBox::backgroundColourId, GruvboxTheme::background());
    list.setColour(juce::ListBox::outlineColourId, GruvboxTheme::gutterBackground());
    list.setOutlineThickness(1);
    list.selectRow(0);
}

CompletionList::~CompletionList()
{
    list.setModel(nullptr);
}

void CompletionList::setItems(std::vector<VoicegroupCompletionItem> newItems)
{
    items = std::move(newItems);
    list.updateContent();
    list.selectRow(items.empty() ? -1 : 0);
    setVisible(!items.empty());
    repaint();
}

void CompletionList::setAcceptCallback(AcceptCallback callback)
{
    acceptCallback = std::move(callback);
}

void CompletionList::clear()
{
    setItems({});
}

void CompletionList::selectNext()
{
    if (items.empty())
        return;

    list.selectRow(juce::jmin(list.getSelectedRow() + 1, static_cast<int>(items.size()) - 1));
}

void CompletionList::selectPrevious()
{
    if (items.empty())
        return;

    list.selectRow(juce::jmax(list.getSelectedRow() - 1, 0));
}

void CompletionList::acceptSelectedItem()
{
    const auto selected = getSelectedItem();
    if (selected.has_value() && acceptCallback)
        acceptCallback(*selected);
}

std::optional<VoicegroupCompletionItem> CompletionList::getSelectedItem() const
{
    const auto selectedRow = list.getSelectedRow();
    if (selectedRow < 0 || selectedRow >= static_cast<int>(items.size()))
        return std::nullopt;

    return items[static_cast<size_t>(selectedRow)];
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
    return static_cast<int>(items.size());
}

void CompletionList::paintListBoxItem(int rowNumber, juce::Graphics& g, int width, int height, bool rowIsSelected)
{
    if (rowNumber < 0 || rowNumber >= static_cast<int>(items.size()))
        return;

    const auto& item = items[static_cast<size_t>(rowNumber)];
    g.fillAll(rowIsSelected ? GruvboxTheme::selection() : GruvboxTheme::background());

    auto bounds = juce::Rectangle<int>(0, 0, width, height).reduced(8, 2);
    g.setColour(GruvboxTheme::foreground());
    g.setFont(juce::Font(juce::FontOptions(13.0f)));
    g.drawFittedText(item.label, bounds.removeFromLeft(180), juce::Justification::centredLeft, 1);

    if (bounds.getWidth() > 0)
    {
        g.setColour(GruvboxTheme::gutterText());
        g.drawFittedText(item.detail, bounds, juce::Justification::centredLeft, 1);
    }
}

void CompletionList::listBoxItemDoubleClicked(int row, const juce::MouseEvent& event)
{
    juce::ignoreUnused(event);

    if (row >= 0 && row < static_cast<int>(items.size()) && acceptCallback)
        acceptCallback(items[static_cast<size_t>(row)]);
}
