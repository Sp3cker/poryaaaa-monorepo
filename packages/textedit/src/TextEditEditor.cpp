#include "TextEditEditor.h"

#include "GruvboxTheme.h"

namespace {

constexpr auto hoverDelayMs = 200;
constexpr auto popupGap = 4;

juce::Rectangle<int> popupBoundsFor(const juce::Rectangle<int>& anchor,
                                    const juce::Rectangle<int>& availableBounds,
                                    const juce::Rectangle<int>& currentBounds)
{
    auto bounds = currentBounds.withPosition(anchor.getX(), anchor.getBottom() + popupGap);

    if (bounds.getRight() > availableBounds.getRight())
        bounds.setX(juce::jmax(availableBounds.getX(), availableBounds.getRight() - bounds.getWidth()));

    if (bounds.getBottom() > availableBounds.getBottom())
        bounds.setY(juce::jmax(availableBounds.getY(), anchor.getY() - popupGap - bounds.getHeight()));

    return bounds;
}

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

void VoicegroupCodeEditor::setHoverCallback(HoverCallback callback)
{
    hoverCallback = std::move(callback);
}

void VoicegroupCodeEditor::mouseMove(const juce::MouseEvent& event)
{
    juce::CodeEditorComponent::mouseMove(event);
    pendingHoverPoint = event.getPosition();
    startTimer(hoverDelayMs);
}

void VoicegroupCodeEditor::mouseExit(const juce::MouseEvent& event)
{
    juce::CodeEditorComponent::mouseExit(event);
    cancelPendingHover();
}

void VoicegroupCodeEditor::timerCallback()
{
    stopTimer();

    if (hoverCallback)
        hoverCallback(getPositionAt(pendingHoverPoint.x, pendingHoverPoint.y));
}

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
    statusLabel.setText("Language service: starting", juce::dontSendNotification);

    addAndMakeVisible(editor);
    addChildComponent(completionList);
    completionList.setSize(320, 180);
    addChildComponent(hoverCard);
    hoverCard.setSize(360, 120);
    addAndMakeVisible(statusLabel);
    setResizable(true, true);
    setSize(900, 700);

    document.addListener(this);
    textProcessor.addDocumentChangeListener(this);
    editor.setHoverCallback([this](auto position) { requestHover(position); });
    addAndMakeVisible(toolbar);
    // toolbar.saveButton.onClick = [this]();
    languageService.setStatusCallback([this] { refreshLanguageServiceStatus(); });
    languageService.setCompletionCallback([this](auto items) { showCompletions(std::move(items)); });
    languageService.setHoverCallback([this](auto text) { showHover(std::move(text)); });
    languageService.syncDocument(document.getAllContent());

    refreshLanguageServiceStatus();
}

TextEditEditor::~TextEditEditor()
{
    pushDocumentToProcessor();
    editor.setHoverCallback({});
    editor.cancelPendingHover();
    languageService.setStatusCallback({});
    languageService.setCompletionCallback({});
    languageService.setHoverCallback({});
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
        requestLanguageContext();
}

void TextEditEditor::codeDocumentTextDeleted(int startIndex, int endIndex)
{
    juce::ignoreUnused(startIndex, endIndex);
    notifyLocalEdit();
    completionList.clear();
}

void TextEditEditor::changeListenerCallback(juce::ChangeBroadcaster* source)
{
    juce::ignoreUnused(source);
    pullDocumentFromProcessor();
}

void TextEditEditor::refreshLanguageServiceStatus()
{
    const auto statusText = languageService.getStatusText();
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
 * pullDocumentFromProcessor syncs the language service itself. */
void TextEditEditor::notifyLocalEdit()
{
    if (updatingDocument)
        return;

    const auto text = document.getAllContent();
    textProcessor.setDocumentText(text);
    languageService.syncDocument(text);
    hoverCard.clear();
}

void TextEditEditor::pullDocumentFromProcessor()
{
    const auto processorText = textProcessor.getDocumentText();
    if (document.getAllContent() == processorText)
        return;

    const juce::ScopedValueSetter<bool> scopedUpdate(updatingDocument, true);
    document.replaceAllContent(processorText);

    languageService.syncDocument(processorText);
}

void TextEditEditor::requestLanguageContext()
{
    if (!languageService.canRequestContext())
        return;

    const auto caret = editor.getCaretPos();
    const auto line = caret.getLineNumber();
    const auto character = caret.getIndexInLine();
    languageService.requestCompletion(line, character);
    languageService.requestSignatureHelp(line, character);
}

void TextEditEditor::requestHover(juce::CodeDocument::Position position)
{
    if (!languageService.canRequestContext())
        return;

    lastHoverPosition = position;
    languageService.requestHover(position.getLineNumber(), position.getIndexInLine());
}

void TextEditEditor::showCompletions(std::vector<VoicegroupCompletionItem> items)
{
    completionList.setItems(std::move(items));

    if (completionList.isVisible())
        positionCompletionListAtCaret();
}

void TextEditEditor::showHover(juce::String text)
{
    hoverCard.setText(std::move(text));

    if (hoverCard.isVisible())
        positionHoverCardAt(lastHoverPosition);
}

void TextEditEditor::positionCompletionListAtCaret()
{
    const auto caretBounds = editor.getCharacterBounds(editor.getCaretPos());
    const auto anchor = caretBounds.withPosition(getLocalPoint(&editor, caretBounds.getTopLeft()));
    const auto availableBounds = getLocalBounds().withBottom(statusLabel.getY());
    completionList.setBounds(popupBoundsFor(anchor, availableBounds, completionList.getBounds()));
    completionList.toFront(false);
}

void TextEditEditor::positionHoverCardAt(juce::CodeDocument::Position position)
{
    const auto characterBounds = editor.getCharacterBounds(position);
    const auto anchor = characterBounds.withPosition(getLocalPoint(&editor, characterBounds.getTopLeft()));
    const auto availableBounds = getLocalBounds().withBottom(statusLabel.getY());
    hoverCard.setBounds(popupBoundsFor(anchor, availableBounds, hoverCard.getBounds()));
    hoverCard.toFront(false);
}

void TextEditEditor::focusEditor()
{
    if (editor.isEnabled() && editor.isShowing())
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
