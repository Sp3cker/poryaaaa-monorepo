#pragma once

#include <juce_gui_extra/juce_gui_extra.h>

#include <memory>

#include "CompletionList.h"
#include "ExtractLineSelector.h"
#include "HoverCard.h"
#include "TextEditFileStore.h"
#include "TextEditProcessor.h"
#include "TopToolBar.h"
#include "VoicegroupCodeEditor.h"
#include "VoicegroupLanguageService.h"
#include "VoicegroupTokeniser.h"

class TextInteractionState final
{
public:
    void setCompletionActive(bool active)
    {
        completionActive = active;
    }
    void enterExtractMode()
    {
        mode = Mode::extract;
    }
    void exitExtractMode()
    {
        mode = Mode::normal;
    }
    bool canUseLanguageFeatures() const
    {
        return mode == Mode::normal;
    }
    bool canRequestHover() const
    {
        return !completionActive && canUseLanguageFeatures();
    }
    bool canShowHover() const
    {
        return !completionActive && canUseLanguageFeatures();
    }
    bool isExtractActive() const
    {
        return mode == Mode::extract;
    }

private:
    enum class Mode
    {
        normal,
        extract
    };

    bool completionActive = false;
    Mode mode = Mode::normal;
};

class TextEditEditor final : public juce::AudioProcessorEditor, private juce::CodeDocument::Listener
{
public:
    explicit TextEditEditor(TextEditProcessor& processor);
    ~TextEditEditor() override;

    void resized() override;
    void parentHierarchyChanged() override;
    void visibilityChanged() override;
    TopToolBar toolbar;

private:
    void codeDocumentTextInserted(const juce::String& newText, int insertIndex) override;
    void codeDocumentTextDeleted(int startIndex, int endIndex) override;

    void refreshLanguageServiceStatus();
    void syncLocalEdit();
    void requestLanguageContext();
    void requestLanguageContext(juce::CodeDocument::Position position);
    void requestHover(juce::CodeDocument::Position position);
    void showCompletions(std::vector<VoicegroupCompletionItem> items);
    void showHover(juce::String text);
    void clearCompletions();
    void dismissHover();
    bool handleEditorKeyPressed(const juce::KeyPress& key);
    void acceptCompletion(VoicegroupCompletionItem completion);
    void replaceEditorText(juce::Range<int> range, const juce::String& replacement, int caretPositionAfter);
    bool requestAndApplyTabAction();
    void applyTabAction(const VoicegroupTabAction& action);
    void loadInitialVoicegroup();
    void saveVoicegroup();
    void saveVoicegroupAs();
    void enterExtractMode();
    void exitExtractMode();
    void saveExtractedVoicegroupAs();
    void updateExtractStatus();
    void positionExtractCheckboxStrip();
    void showFileStoreError(const TextEditFileStoreError& error);
    void showIoError(const juce::String& title, const juce::String& message);
    void positionCompletionListAtCaret();
    void positionHoverCardAt(juce::CodeDocument::Position position);
    void focusEditor();

    TextEditFileStore fileStore;
    juce::CodeDocument document;
    VoicegroupTokeniser tokeniser;
    VoicegroupCodeEditor editor;
    CompletionList completionList;
    HoverCard hoverCard;
    ExtractLineSelector extractLineSelector;
    juce::Label statusLabel;
    EmbeddedLanguageService languageService;
    juce::ScopedMessageBox ioErrorBox;
    std::unique_ptr<juce::FileChooser> saveAsChooser;
    juce::String lastStatusText;
    juce::CodeDocument::Position lastHoverPosition{document, 0};
    TextInteractionState interactionState;
    bool isApplyingProgrammaticEdit = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TextEditEditor)
};
