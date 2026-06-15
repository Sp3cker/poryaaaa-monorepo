#pragma once

#include <juce_gui_extra/juce_gui_extra.h>

#include <functional>
#include <memory>

#include "CompletionList.h"
#include "ExtractLineSelector.h"
#include "HoverCard.h"
#include "TextEditFileStore.h"
#include "TextEditProcessor.h"
#include "VoicegroupLanguageService.h"
#include "VoicegroupTokeniser.h"

class TopToolBar final : public juce::Component
{
public:
    enum class Mode
    {
        normal,
        extract
    };

    TopToolBar();

    void resized() override;
    void paint(juce::Graphics& g) override;

    void setMode(Mode mode);
    void setDocumentLoaded(bool loaded);
    void setCanSaveExtract(bool canSave);

    std::function<void()> onSave;
    std::function<void()> onSaveAs;
    std::function<void()> onEnterExtract;
    std::function<void()> onSaveExtractAs;
    std::function<void()> onCancelExtract;

private:
    void configureButton(juce::TextButton& button);
    void updateButtonState();

    juce::TextButton saveButton{"Save"};
    juce::TextButton saveAsButton{"Save As"};
    juce::TextButton extractButton{"Extract"};
    juce::TextButton extractSaveAsButton{"Save As..."};
    juce::TextButton extractCancelButton{"Cancel"};
    juce::FlexBox flex;
    Mode mode = Mode::normal;
    bool documentLoaded = true;
    bool canSaveExtract = false;
};

class VoicegroupCodeEditor final : public juce::CodeEditorComponent, private juce::Timer
{
public:
    using HoverCallback = std::function<void(juce::CodeDocument::Position)>;
    using DismissHoverCallback = std::function<void()>;
    using KeyCallback = std::function<bool(const juce::KeyPress&)>;

    VoicegroupCodeEditor(juce::CodeDocument& document, juce::CodeTokeniser* tokeniser);
    ~VoicegroupCodeEditor() override;

    void cancelPendingHover();
    void setDismissHoverCallback(DismissHoverCallback callback);
    void setHoverCallback(HoverCallback callback);
    void setKeyCallback(KeyCallback callback);

private:
    bool keyPressed(const juce::KeyPress& key) override;
    void mouseDown(const juce::MouseEvent& event) override;
    void mouseDrag(const juce::MouseEvent& event) override;
    void mouseMove(const juce::MouseEvent& event) override;
    void mouseExit(const juce::MouseEvent& event) override;
    void timerCallback() override;
    void dismissHover();

    DismissHoverCallback dismissHoverCallback;
    HoverCallback hoverCallback;
    KeyCallback keyCallback;
    juce::Point<int> pendingHoverPoint;
};

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
