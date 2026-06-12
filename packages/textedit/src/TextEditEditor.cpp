#include "TextEditEditor.h"

#include "GruvboxTheme.h"

TextEditEditor::TextEditEditor(TextEditProcessor& processorToUse)
    : AudioProcessorEditor(processorToUse),
      textProcessor(processorToUse),
      editor(document, &tokeniser)
{
    document.replaceAllContent(textProcessor.getDocumentText());
    document.clearUndoHistory();
    document.setSavePoint();

    editor.setLineNumbersShown(true);
    editor.setTabSize(4, true);
    editor.setFont(juce::Font(juce::FontOptions(juce::Font::getDefaultMonospacedFontName(), 15.0f, juce::Font::plain)));
    editor.setColour(juce::CodeEditorComponent::backgroundColourId, GruvboxTheme::background());
    editor.setColour(juce::CodeEditorComponent::highlightColourId, GruvboxTheme::selection());
    editor.setColour(juce::CodeEditorComponent::defaultTextColourId, GruvboxTheme::foreground());
    editor.setColour(juce::CodeEditorComponent::lineNumberBackgroundId, GruvboxTheme::gutterBackground());
    editor.setColour(juce::CodeEditorComponent::lineNumberTextId, GruvboxTheme::gutterText());
    editor.setColourScheme(GruvboxTheme::codeColourScheme());

    statusLabel.setJustificationType(juce::Justification::centredLeft);
    statusLabel.setColour(juce::Label::backgroundColourId, GruvboxTheme::statusBackground());
    statusLabel.setColour(juce::Label::textColourId, GruvboxTheme::statusText());
    statusLabel.setText("LSP: starting", juce::dontSendNotification);

    addAndMakeVisible(editor);
    addAndMakeVisible(statusLabel);
    setResizable(true, true);
    setSize(900, 700);

    document.addListener(this);
    textProcessor.addDocumentChangeListener(this);
    addAndMakeVisible(toolbar);
    // toolbar.saveButton.onClick = [this]();
    if (lspClient.start())
        lspClient.syncDocument(document.getAllContent());

    startTimerHz(8);
}

TextEditEditor::~TextEditEditor()
{
    stopTimer();
    pushDocumentToProcessor();
    lspClient.stop();
    textProcessor.removeDocumentChangeListener(this);
    document.removeListener(this);
}

void TextEditEditor::resized()
{
    auto bounds = getLocalBounds();
    const int toolbarHeight = 36;
    toolbar.setBounds(bounds.removeFromTop(toolbarHeight));
    statusLabel.setBounds(bounds.removeFromBottom(24));
    editor.setBounds(bounds);
}

void TextEditEditor::parentHierarchyChanged()
{
    focusEditor();
}

void TextEditEditor::visibilityChanged()
{
    if (isVisible())
        focusEditor();
}

void TextEditEditor::codeDocumentTextInserted(const juce::String& newText, int insertIndex)
{
    juce::ignoreUnused(newText, insertIndex);
    notifyLocalEdit();

    if (newText.containsAnyOf("_, "))
        requestLspContext();
}

void TextEditEditor::codeDocumentTextDeleted(int startIndex, int endIndex)
{
    juce::ignoreUnused(startIndex, endIndex);
    notifyLocalEdit();
}

void TextEditEditor::changeListenerCallback(juce::ChangeBroadcaster* source)
{
    juce::ignoreUnused(source);
    pullDocumentFromProcessor();
}

void TextEditEditor::timerCallback()
{
    const auto statusText = lspClient.getStatusText();
    if (statusText != lastStatusText)
    {
        lastStatusText = statusText;
        statusLabel.setText(statusText, juce::dontSendNotification);
    }
}

void TextEditEditor::pushDocumentToProcessor()
{
    if (!updatingDocument)
        textProcessor.setDocumentText(document.getAllContent());
}

/* Local (typed) edits fan out to both consumers. `updatingDocument` only
 * suppresses the echo during a processor-origin pull, where
 * pullDocumentFromProcessor syncs the LSP itself. */
void TextEditEditor::notifyLocalEdit()
{
    if (updatingDocument)
        return;

    const auto text = document.getAllContent();
    textProcessor.setDocumentText(text);
    lspClient.syncDocument(text);
}

void TextEditEditor::pullDocumentFromProcessor()
{
    const auto processorText = textProcessor.getDocumentText();
    if (document.getAllContent() == processorText)
        return;

    const juce::ScopedValueSetter<bool> scopedUpdate(updatingDocument, true);
    document.replaceAllContent(processorText);

    lspClient.syncDocument(processorText);
}

void TextEditEditor::requestLspContext()
{
    const auto caret = editor.getCaretPos();
    const auto line = caret.getLineNumber();
    const auto character = caret.getIndexInLine();
    lspClient.requestCompletion(line, character);
    lspClient.requestSignatureHelp(line, character);
    lspClient.requestHover(line, character);
}

void TextEditEditor::focusEditor()
{
    if (editor.isShowing())
        editor.grabKeyboardFocus();
}

TopToolBar::TopToolBar()
{
    addAndMakeVisible(saveButton);
    // Style to match your Gruvbox theme
    // auto bg = GruvboxTheme::background();
    auto fg = GruvboxTheme::foreground();

    for (auto* b : { &saveButton })
    {
        b->setColour(juce::TextButton::buttonColourId,          GruvboxTheme::gutterBackground());
        b->setColour(juce::TextButton::textColourOffId,         fg);
        b->setColour(juce::TextButton::textColourOnId,          fg);
    }
}

void TopToolBar::paint(juce::Graphics& g)
{
    g.fillAll(GruvboxTheme::gutterBackground()); // or a slightly different shade
}

void TopToolBar::resized()
{
    flex.items.clear();
    flex.flexDirection = juce::FlexBox::Direction::row;
    flex.justifyContent = juce::FlexBox::JustifyContent::spaceBetween;
    flex.alignItems = juce::FlexBox::AlignItems::center;

    flex.items.add(juce::FlexItem(saveButton).withMinWidth(60).withMinHeight(28));

    flex.performLayout(getLocalBounds().reduced(4, 2));
}