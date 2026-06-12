#pragma once

#include <juce_gui_extra/juce_gui_extra.h>

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

class TextEditEditor final : public juce::AudioProcessorEditor,
                             private juce::CodeDocument::Listener,
                             private juce::ChangeListener,
                             private juce::Timer
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
    void timerCallback() override;

    void pushDocumentToProcessor();
    void pullDocumentFromProcessor();
    void notifyLocalEdit();
    void requestLspContext();
    void focusEditor();

    TextEditProcessor& textProcessor;
    juce::CodeDocument document;
    VoicegroupTokeniser tokeniser;
    juce::CodeEditorComponent editor;
    juce::Label statusLabel;
    VoicegroupLspClient lspClient;
    juce::String lastStatusText;
    bool updatingDocument = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TextEditEditor)
};
