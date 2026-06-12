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

    if (lspClient.start())
    {
        lspClient.openDocument(document.getAllContent());
        lspDocumentOpened = true;
    }

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
    pushDocumentToProcessor();
    notifyLspTextChanged();

    if (newText.containsAnyOf("_, "))
        requestLspContext();
}

void TextEditEditor::codeDocumentTextDeleted(int startIndex, int endIndex)
{
    juce::ignoreUnused(startIndex, endIndex);
    pushDocumentToProcessor();
    notifyLspTextChanged();
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

void TextEditEditor::pullDocumentFromProcessor()
{
    const auto processorText = textProcessor.getDocumentText();
    if (document.getAllContent() == processorText)
        return;

    const juce::ScopedValueSetter<bool> scopedUpdate(updatingDocument, true);
    document.replaceAllContent(processorText);
    notifyLspTextChanged();
}

void TextEditEditor::notifyLspTextChanged()
{
    if (updatingDocument)
        return;

    if (!lspDocumentOpened)
        return;

    lspClient.changeDocument(document.getAllContent());
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
