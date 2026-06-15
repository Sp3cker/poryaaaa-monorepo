#include "TopToolBar.h"

#include "GruvboxTheme.h"

TopToolBar::TopToolBar()
{
    addAndMakeVisible(saveButton);
    addAndMakeVisible(saveAsButton);
    addAndMakeVisible(extractButton);
    addAndMakeVisible(extractSaveAsButton);
    addAndMakeVisible(extractCancelButton);
    for (auto* b : {&saveButton, &saveAsButton, &extractButton, &extractSaveAsButton, &extractCancelButton})
        configureButton(*b);

    saveButton.onClick = [this]
    {
        if (onSave)
            onSave();
    };
    saveAsButton.onClick = [this]
    {
        if (onSaveAs)
            onSaveAs();
    };
    extractButton.onClick = [this]
    {
        if (onEnterExtract)
            onEnterExtract();
    };
    extractSaveAsButton.onClick = [this]
    {
        if (onSaveExtractAs)
            onSaveExtractAs();
    };
    extractCancelButton.onClick = [this]
    {
        if (onCancelExtract)
            onCancelExtract();
    };
    updateButtonState();
}

void TopToolBar::paint(juce::Graphics& g)
{
    g.fillAll(GruvboxTheme::gutterBackground());
}

void TopToolBar::resized()
{
    flex.items.clear();
    flex.flexDirection = juce::FlexBox::Direction::row;
    flex.justifyContent = juce::FlexBox::JustifyContent::flexStart;
    flex.alignItems = juce::FlexBox::AlignItems::center;

    auto addButton = [this](juce::TextButton& button, float width)
    {
        if (button.isVisible())
            flex.items.add(juce::FlexItem(button).withMinWidth(width).withMinHeight(28).withMargin({0, 6, 0, 0}));
    };
    addButton(saveButton, 60);
    addButton(saveAsButton, 84);
    addButton(extractButton, 76);
    addButton(extractSaveAsButton, 96);
    addButton(extractCancelButton, 76);

    flex.performLayout(getLocalBounds().reduced(4, 2));
}

void TopToolBar::setMode(Mode modeToUse)
{
    mode = modeToUse;
    updateButtonState();
}

void TopToolBar::setDocumentLoaded(bool loaded)
{
    documentLoaded = loaded;
    updateButtonState();
}

void TopToolBar::setCanSaveExtract(bool canSave)
{
    canSaveExtract = canSave;
    updateButtonState();
}

void TopToolBar::configureButton(juce::TextButton& button)
{
    const auto fg = GruvboxTheme::foreground();
    button.setColour(juce::TextButton::buttonColourId, GruvboxTheme::gutterBackground());
    button.setColour(juce::TextButton::textColourOffId, fg);
    button.setColour(juce::TextButton::textColourOnId, fg);
}

void TopToolBar::updateButtonState()
{
    const auto extractMode = mode == Mode::extract;
    saveButton.setVisible(!extractMode);
    saveAsButton.setVisible(!extractMode);
    extractButton.setVisible(!extractMode);
    extractSaveAsButton.setVisible(extractMode);
    extractCancelButton.setVisible(extractMode);
    saveButton.setEnabled(documentLoaded);
    saveAsButton.setEnabled(documentLoaded);
    extractButton.setEnabled(documentLoaded);
    extractSaveAsButton.setEnabled(documentLoaded && canSaveExtract);
    extractCancelButton.setEnabled(documentLoaded);
    resized();
    repaint();
}
