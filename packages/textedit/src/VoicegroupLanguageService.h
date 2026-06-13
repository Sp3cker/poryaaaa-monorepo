#pragma once

#include <juce_gui_extra/juce_gui_extra.h>

#include <functional>
#include <vector>

#include "VoicegroupLanguageBridge.h"

class EmbeddedLanguageService final
{
public:
    using StatusCallback = std::function<void()>;
    using CompletionCallback = std::function<void(std::vector<VoicegroupCompletionItem>)>;
    using HoverCallback = std::function<void(juce::String)>;

    void setStatusCallback(StatusCallback callback);
    void setCompletionCallback(CompletionCallback callback);
    void setHoverCallback(HoverCallback callback);
    bool setProjectRoot(const juce::File& root);
    bool canRequestContext() const;
    void syncDocument(const juce::String& text);
    void requestCompletion(int line, int character);
    void requestHover(int line, int character);
    void requestSignatureHelp(int line, int character);
    juce::String getStatusText() const;

private:
    void notifyStatusChanged();

    VoicegroupLanguageBridge bridge;
    StatusCallback statusCallback;
    CompletionCallback completionCallback;
    HoverCallback hoverCallback;
    const juce::String documentUri = "file:///textedit/voicegroup.inc";
};
