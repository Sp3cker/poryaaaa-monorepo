#pragma once

#include <juce_gui_extra/juce_gui_extra.h>

#include <functional>

#include "CompletionList.h"
#include "HoverCard.h"
#include "TextEditProcessor.h"
#include "VoicegroupLspClient.h"
#include "VoicegroupTokeniser.h"
class TopToolBar final: public juce::Component
{
    public: TopToolBar();

    void resized() override;
    void paint (juce::Graphics& g) override;

    juce::TextButton saveButton {"Save"};

    private:
        juce::FlexBox flex;
};

class VoicegroupCodeEditor final : public juce::CodeEditorComponent,
                                   private juce::Timer
{
public:
    using HoverCallback = std::function<void(juce::CodeDocument::Position)>;

    VoicegroupCodeEditor(juce::CodeDocument& document, juce::CodeTokeniser* tokeniser);
    ~VoicegroupCodeEditor() override;

    void cancelPendingHover();
    void setHoverCallback(HoverCallback callback);

private:
    void mouseMove(const juce::MouseEvent& event) override;
    void mouseExit(const juce::MouseEvent& event) override;
    void timerCallback() override;

    HoverCallback hoverCallback;
    juce::Point<int> pendingHoverPoint;
};

class TextEditEditor final : public juce::AudioProcessorEditor,
                             private juce::CodeDocument::Listener,
                             private juce::ChangeListener,
                             private juce::AsyncUpdater
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
    void changeListenerCallback(juce::ChangeBroadcaster* source) override;
    void handleAsyncUpdate() override;

    void refreshLspStatus();
    void pushDocumentToProcessor();
    void pullDocumentFromProcessor();
    void notifyLocalEdit();
    void requestLspContext();
    void requestLspHover(juce::CodeDocument::Position position);
    void setLspReady(bool ready);
    void allowEditingWithoutLsp();
    void focusEditor();

    TextEditProcessor& textProcessor;
    juce::CodeDocument document;
    VoicegroupTokeniser tokeniser;
    VoicegroupCodeEditor editor;
    CompletionList completionList;
    HoverCard hoverCard;
    juce::Label statusLabel;
    VoicegroupLspClient lspClient;
    juce::String lastStatusText;
    bool lspReady = false;
    bool updatingDocument = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TextEditEditor)
};
