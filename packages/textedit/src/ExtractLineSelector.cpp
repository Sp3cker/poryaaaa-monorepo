#include "ExtractLineSelector.h"

#include "GruvboxTheme.h"

namespace
{

bool isExtractableVoiceLine(const juce::String& line)
{
    const auto trimmed = line.trimStart();
    return trimmed.startsWith("voice_") && !trimmed.startsWith("voice_group");
}

juce::String pluralizedLines(int count)
{
    return juce::String(count) + (count == 1 ? " line selected" : " lines selected");
}

} // namespace

void ExtractLineSelector::begin(juce::CodeEditorComponent& editorToUse, juce::CodeDocument& documentToUse)
{
    editor = &editorToUse;
    document = &documentToUse;
    selectedLines.clear();
    collectExtractableLines();
    setVisible(true);
    startTimerHz(30);
    repaint();
}

void ExtractLineSelector::end()
{
    stopTimer();
    editor = nullptr;
    document = nullptr;
    extractableLines.clear();
    selectedLines.clear();
    setVisible(false);
}

void ExtractLineSelector::setSelectionChangedCallback(SelectionChangedCallback callback)
{
    selectionChangedCallback = std::move(callback);
}

juce::String ExtractLineSelector::getSelectedVoiceLinesText() const
{
    juce::String text;
    if (document == nullptr)
        return text;

    for (const auto line : selectedLines)
        if (line >= 0 && line < document->getNumLines())
            text += document->getLine(line) + "\n";

    return text;
}

juce::String ExtractLineSelector::getStatusText() const
{
    return "Extract: " + pluralizedLines(getSelectedLineCount());
}

void ExtractLineSelector::paint(juce::Graphics& g)
{
    g.fillAll(GruvboxTheme::gutterBackground().withAlpha(0.92f));

    for (auto index = size_t{0}; index < extractableLines.size(); ++index)
    {
        const auto line = extractableLines[index];
        const auto checkboxBounds = checkboxBoundsForLine(line);
        if (checkboxBounds.isEmpty() || !checkboxBounds.intersects(getLocalBounds()))
            continue;

        const auto selected = selectedLines.count(line) > 0;
        auto labelBounds = checkboxBounds.withX(0).withWidth(checkboxBounds.getX()).reduced(4, 0);
        g.setColour(GruvboxTheme::gutterText());
        g.setFont(juce::Font(juce::FontOptions(float(juce::jmax(10, checkboxBounds.getHeight() - 4)))));
        g.drawFittedText(juce::String(int(index)), labelBounds, juce::Justification::centredRight, 1);
        g.setColour(selected ? GruvboxTheme::selection() : GruvboxTheme::background());
        g.fillRect(checkboxBounds);
        g.setColour(selected ? GruvboxTheme::foreground() : GruvboxTheme::gutterText());
        g.drawRect(checkboxBounds);
        if (selected)
        {
            const auto tickBounds = checkboxBounds.reduced(4);
            juce::Path tick;
            tick.startNewSubPath(float(tickBounds.getX()), float(tickBounds.getCentreY()));
            tick.lineTo(float(tickBounds.getCentreX() - 1), float(tickBounds.getBottom()));
            tick.lineTo(float(tickBounds.getRight()), float(tickBounds.getY()));
            g.strokePath(tick, juce::PathStrokeType(2.0f));
        }
    }
}

void ExtractLineSelector::mouseDown(const juce::MouseEvent& event)
{
    for (const auto line : extractableLines)
    {
        if (checkboxBoundsForLine(line).contains(event.getPosition()))
        {
            toggleLine(line);
            return;
        }
    }
}

void ExtractLineSelector::timerCallback()
{
    repaint();
}

void ExtractLineSelector::collectExtractableLines()
{
    extractableLines.clear();
    if (document == nullptr)
        return;

    for (auto line = 0; line < document->getNumLines(); ++line)
        if (isExtractableVoiceLine(document->getLine(line)))
            extractableLines.push_back(line);
}

void ExtractLineSelector::toggleLine(int line)
{
    if (selectedLines.count(line) > 0)
        selectedLines.erase(line);
    else
        selectedLines.insert(line);

    if (selectionChangedCallback)
        selectionChangedCallback();

    repaint();
}

juce::Rectangle<int> ExtractLineSelector::checkboxBoundsForLine(int line) const
{
    if (editor == nullptr || document == nullptr)
        return {};

    const auto lineBounds = editor->getCharacterBounds(juce::CodeDocument::Position(*document, line, 0));
    if (lineBounds.isEmpty())
        return {};

    const auto size = juce::jlimit(8, juce::jmax(8, getWidth() / 3), juce::jmax(8, lineBounds.getHeight() - 4));
    return {getWidth() - size - 4, lineBounds.getY() + (lineBounds.getHeight() - size) / 2, size, size};
}
