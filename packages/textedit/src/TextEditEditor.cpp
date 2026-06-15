#include "TextEditEditor.h"

#include "GruvboxTheme.h"

namespace
{

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

juce::String extractedVoicegroupText(const juce::File& file, const juce::String& voiceLines)
{
    return "voice_group " + file.getFileNameWithoutExtension() + "\n" + voiceLines;
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

TextEditEditor::TextEditEditor(TextEditProcessor& processorToUse)
    : AudioProcessorEditor(processorToUse), editor(document, &tokeniser)
{
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
    addChildComponent(extractLineSelector);
    addAndMakeVisible(statusLabel);
    setResizable(true, true);
    setSize(900, 700);

    editor.setHoverCallback([this](auto position) { requestHover(position); });
    editor.setDismissHoverCallback([this] { dismissHover(); });
    editor.setKeyCallback([this](const auto& key) { return handleEditorKeyPressed(key); });
    extractLineSelector.setSelectionChangedCallback([this] { updateExtractStatus(); });
    addAndMakeVisible(toolbar);
    toolbar.onSave = [this] { saveVoicegroup(); };
    toolbar.onSaveAs = [this] { saveVoicegroupAs(); };
    toolbar.onEnterExtract = [this] { enterExtractMode(); };
    toolbar.onSaveExtractAs = [this] { saveExtractedVoicegroupAs(); };
    toolbar.onCancelExtract = [this] { exitExtractMode(); };
    languageService.setStatusCallback([this] { refreshLanguageServiceStatus(); });
    languageService.setCompletionCallback([this](auto items) { showCompletions(std::move(items)); });
    languageService.setHoverCallback([this](auto text) { showHover(std::move(text)); });
    completionList.setAcceptCallback([this](auto item) { acceptCompletion(std::move(item)); });
    fileStore.setErrorListener([this](const auto& error) { showFileStoreError(error); });
    loadInitialVoicegroup();
    document.addListener(this);

    refreshLanguageServiceStatus();
}

TextEditEditor::~TextEditEditor()
{
    editor.setHoverCallback({});
    editor.setDismissHoverCallback({});
    editor.setKeyCallback({});
    editor.cancelPendingHover();
    extractLineSelector.setSelectionChangedCallback({});
    extractLineSelector.end();
    toolbar.onSave = {};
    toolbar.onSaveAs = {};
    toolbar.onEnterExtract = {};
    toolbar.onSaveExtractAs = {};
    toolbar.onCancelExtract = {};
    languageService.setStatusCallback({});
    languageService.setCompletionCallback({});
    languageService.setHoverCallback({});
    completionList.setAcceptCallback({});
    fileStore.setErrorListener({});
    document.removeListener(this);
}

void TextEditEditor::resized()
{
    auto bounds = getLocalBounds();
    const int toolbarHeight = 36;
    toolbar.setBounds(bounds.removeFromTop(toolbarHeight));
    statusLabel.setBounds(bounds.removeFromBottom(24));
    editor.setBounds(bounds);
    positionExtractCheckboxStrip();
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
    syncLocalEdit();

    if (!isApplyingCompletion)
        requestLanguageContext();
}

void TextEditEditor::codeDocumentTextDeleted(int startIndex, int endIndex)
{
    juce::ignoreUnused(startIndex, endIndex);
    syncLocalEdit();

    if (!isApplyingCompletion)
        requestLanguageContext();
}

void TextEditEditor::refreshLanguageServiceStatus()
{
    if (interactionState.isExtractActive())
        return;

    const auto statusText = languageService.getStatusText();
    if (statusText != lastStatusText)
    {
        lastStatusText = statusText;
        statusLabel.setText(statusText, juce::dontSendNotification);
    }
}

void TextEditEditor::syncLocalEdit()
{
    const auto text = document.getAllContent();
    languageService.syncDocument(text);
    dismissHover();
}

void TextEditEditor::requestLanguageContext()
{
    requestLanguageContext(editor.getCaretPos());
}

void TextEditEditor::requestLanguageContext(juce::CodeDocument::Position position)
{
    if (!interactionState.canUseLanguageFeatures() || !languageService.canRequestContext())
    {
        clearCompletions();
        return;
    }

    const auto line = position.getLineNumber();
    const auto character = position.getIndexInLine();
    languageService.requestCompletion(line, character);
    languageService.requestSignatureHelp(line, character);
}

void TextEditEditor::requestHover(juce::CodeDocument::Position position)
{
    if (!languageService.canRequestContext() || !interactionState.canRequestHover())
        return;

    lastHoverPosition = position;
    languageService.requestHover(position.getLineNumber(), position.getIndexInLine());
}

void TextEditEditor::showCompletions(std::vector<VoicegroupCompletionItem> items)
{
    if (!interactionState.canUseLanguageFeatures())
    {
        clearCompletions();
        return;
    }

    interactionState.setCompletionActive(!items.empty());
    completionList.setItems(std::move(items));

    if (completionList.isVisible())
    {
        dismissHover();
        positionCompletionListAtCaret();
    }
}

void TextEditEditor::showHover(juce::String text)
{
    if (!interactionState.canShowHover())
        return;

    hoverCard.setText(std::move(text));

    if (hoverCard.isVisible())
        positionHoverCardAt(lastHoverPosition);
}

void TextEditEditor::clearCompletions()
{
    interactionState.setCompletionActive(false);
    completionList.clear();
}

void TextEditEditor::dismissHover()
{
    hoverCard.clear();
}

bool TextEditEditor::handleEditorKeyPressed(const juce::KeyPress& key)
{
    if (interactionState.isExtractActive())
    {
        if (key == juce::KeyPress::escapeKey)
        {
            exitExtractMode();
            return true;
        }

        return false;
    }

    if (completionList.isVisible())
    {
        if (key == juce::KeyPress::upKey)
        {
            completionList.selectPrevious();
            return true;
        }

        if (key == juce::KeyPress::downKey)
        {
            completionList.selectNext();
            return true;
        }

        if (key == juce::KeyPress::returnKey)
        {
            completionList.acceptSelectedItem();
            return true;
        }

        if (key == juce::KeyPress::tabKey || key.getTextCharacter() == '\t')
        {
            completionList.acceptSelectedItem();
            return true;
        }

        if (key == juce::KeyPress::escapeKey)
        {
            clearCompletions();
            return true;
        }
    }

    if (key == juce::KeyPress::tabKey || key.getTextCharacter() == '\t')
        return requestAndApplyTabAction();

    return false;
}

void TextEditEditor::acceptCompletion(VoicegroupCompletionItem completion)
{
    clearCompletions();
    editor.selectRegion(
        juce::CodeDocument::Position(document, completion.replacementStartLine, completion.replacementStartCharacter),
        juce::CodeDocument::Position(document, completion.replacementEndLine, completion.replacementEndCharacter));
    isApplyingCompletion = true;
    editor.insertTextAtCaret(completion.insertText);
    isApplyingCompletion = false;

    if (const auto action = languageService.requestTabAction(completion.replacementStartLine,
                                                             completion.replacementStartCharacter,
                                                             completion.replacementStartLine,
                                                             completion.replacementStartCharacter))
    {
        applyTabAction(*action);
    }
}

bool TextEditEditor::requestAndApplyTabAction()
{
    clearCompletions();

    if (!languageService.canRequestContext())
        return false;

    const auto selection = editor.getHighlightedRegion();
    const auto start = juce::CodeDocument::Position(document, selection.getStart());
    const auto end = juce::CodeDocument::Position(document, selection.getEnd());
    const auto action = languageService.requestTabAction(
        start.getLineNumber(), start.getIndexInLine(), end.getLineNumber(), end.getIndexInLine());
    if (!action.has_value() || action->kind == VoicegroupTabActionKind::insertIndent)
        return false;

    applyTabAction(*action);
    return true;
}

void TextEditEditor::applyTabAction(const VoicegroupTabAction& action)
{
    clearCompletions();
    dismissHover();

    if (action.kind == VoicegroupTabActionKind::selectRange)
    {
        editor.selectRegion(juce::CodeDocument::Position(document, action.startLine, action.startCharacter),
                            juce::CodeDocument::Position(document, action.endLine, action.endCharacter));
        requestLanguageContext(juce::CodeDocument::Position(document, action.endLine, action.endCharacter));
        return;
    }

    if (action.kind == VoicegroupTabActionKind::moveCaret)
        editor.moveCaretTo(juce::CodeDocument::Position(document, action.startLine, action.startCharacter), false);
}

void TextEditEditor::loadInitialVoicegroup()
{
    TextEditVoicegroupDocument voicegroupDocument;
    if (!fileStore.loadCurrentVoicegroup(voicegroupDocument))
    {
        editor.setEnabled(false);
        toolbar.setDocumentLoaded(false);
        return;
    }

    document.replaceAllContent(voicegroupDocument.text);
    document.clearUndoHistory();
    document.setSavePoint();
    languageService.setProjectRoot(voicegroupDocument.projectRoot);
    languageService.syncDocument(voicegroupDocument.text);
}

void TextEditEditor::saveVoicegroup()
{
    if (!fileStore.saveCurrentVoicegroup(document.getAllContent()))
        return;

    document.setSavePoint();
}

void TextEditEditor::saveVoicegroupAs()
{
    auto initialFile = fileStore.getCurrentVoicegroupFile();
    if (initialFile == juce::File())
        initialFile = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getChildFile("voicegroup.inc");

    saveAsChooser = std::make_unique<juce::FileChooser>("Save voicegroup as...", initialFile, "*.inc");
    const auto flags = juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles |
                       juce::FileBrowserComponent::warnAboutOverwriting;
    const juce::Component::SafePointer<TextEditEditor> safeThis(this);
    saveAsChooser->launchAsync(
        flags,
        [safeThis](const juce::FileChooser& chooser)
        {
            auto* textEditEditor = safeThis.getComponent();
            if (textEditEditor == nullptr)
                return;

            const auto chosenFile = chooser.getResult();
            if (chosenFile != juce::File() &&
                textEditEditor->fileStore.saveVoicegroupAs(chosenFile, textEditEditor->document.getAllContent()))
                textEditEditor->document.setSavePoint();

            textEditEditor->saveAsChooser.reset();
        });
}

void TextEditEditor::enterExtractMode()
{
    clearCompletions();
    dismissHover();
    interactionState.enterExtractMode();
    editor.setReadOnly(true);
    toolbar.setMode(TopToolBar::Mode::extract);
    extractLineSelector.begin(editor, document);
    updateExtractStatus();
    positionExtractCheckboxStrip();
    extractLineSelector.toFront(false);
}

void TextEditEditor::exitExtractMode()
{
    interactionState.exitExtractMode();
    extractLineSelector.end();
    editor.setReadOnly(false);
    toolbar.setMode(TopToolBar::Mode::normal);
    lastStatusText = {};
    refreshLanguageServiceStatus();
    focusEditor();
}

void TextEditEditor::saveExtractedVoicegroupAs()
{
    if (!extractLineSelector.hasSelection())
        return;

    auto initialFile = fileStore.getCurrentVoicegroupFile();
    if (initialFile == juce::File())
    {
        initialFile =
            juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getChildFile("voicegroup_extract.inc");
    }
    else
    {
        initialFile = initialFile.getSiblingFile(initialFile.getFileNameWithoutExtension() + "_extract.inc");
    }

    saveAsChooser = std::make_unique<juce::FileChooser>("Save extracted voicegroup as...", initialFile, "*.inc");
    const auto flags = juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles |
                       juce::FileBrowserComponent::warnAboutOverwriting;
    const juce::Component::SafePointer<TextEditEditor> safeThis(this);
    saveAsChooser->launchAsync(
        flags,
        [safeThis](const juce::FileChooser& chooser)
        {
            auto* textEditEditor = safeThis.getComponent();
            if (textEditEditor == nullptr)
                return;

            const auto chosenFile = chooser.getResult();
            if (chosenFile != juce::File() &&
                textEditEditor->fileStore.writeVoicegroupFile(
                    chosenFile,
                    extractedVoicegroupText(chosenFile,
                                            textEditEditor->extractLineSelector.getSelectedVoiceLinesText())))
                textEditEditor->exitExtractMode();

            textEditEditor->saveAsChooser.reset();
        });
}

void TextEditEditor::updateExtractStatus()
{
    toolbar.setCanSaveExtract(extractLineSelector.hasSelection());
    statusLabel.setText(extractLineSelector.getStatusText(), juce::dontSendNotification);
}

void TextEditEditor::positionExtractCheckboxStrip()
{
    const auto firstCharacterBounds = editor.getCharacterBounds(juce::CodeDocument::Position(document, 0, 0));
    const auto width = juce::jlimit(48, 96, firstCharacterBounds.getX());
    extractLineSelector.setBounds(editor.getX(), editor.getY(), width, editor.getHeight());
}

void TextEditEditor::showFileStoreError(const TextEditFileStoreError& error)
{
    const auto title = error.operation == TextEditFileStoreOperation::loadVoicegroup
                           ? juce::String("Could not load voicegroup")
                           : juce::String("Could not save voicegroup");
    showIoError(title, error.message);
}

void TextEditEditor::showIoError(const juce::String& title, const juce::String& message)
{
    const auto options =
        juce::MessageBoxOptions::makeOptionsOk(juce::MessageBoxIconType::WarningIcon, title, message, "OK")
            .withAssociatedComponent(&editor)
            .withParentComponent(this);
    ioErrorBox = juce::AlertWindow::showScopedAsync(options, nullptr);
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
    g.fillAll(GruvboxTheme::gutterBackground()); // or a slightly different shade
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
