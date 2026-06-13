#pragma once

#include <juce_gui_extra/juce_gui_extra.h>

#include <functional>
#include <vector>

#include "VoicegroupLanguageBridge.h"

struct VoicegroupCompletionItem
{
    juce::String label;
    juce::String detail;
};

class VoicegroupLanguageService
{
public:
    using StatusCallback = std::function<void()>;
    using CompletionCallback = std::function<void(std::vector<VoicegroupCompletionItem>)>;
    using HoverCallback = std::function<void(juce::String)>;

    virtual ~VoicegroupLanguageService() = default;

    virtual void setStatusCallback(StatusCallback callback) = 0;
    virtual void setCompletionCallback(CompletionCallback callback) = 0;
    virtual void setHoverCallback(HoverCallback callback) = 0;
    virtual bool setProjectRoot(const juce::File& root) = 0;
    virtual bool canRequestContext() const = 0;
    virtual void syncDocument(const juce::String& text) = 0;
    virtual void requestCompletion(int line, int character) = 0;
    virtual void requestHover(int line, int character) = 0;
    virtual void requestSignatureHelp(int line, int character) = 0;
    virtual juce::String getStatusText() const = 0;
};

class EmbeddedLanguageService final : public VoicegroupLanguageService
{
public:
    void setStatusCallback(StatusCallback callback) override;
    void setCompletionCallback(CompletionCallback callback) override;
    void setHoverCallback(HoverCallback callback) override;
    bool setProjectRoot(const juce::File& root) override;
    bool canRequestContext() const override;
    void syncDocument(const juce::String& text) override;
    void requestCompletion(int line, int character) override;
    void requestHover(int line, int character) override;
    void requestSignatureHelp(int line, int character) override;
    juce::String getStatusText() const override;

private:
    void notifyStatusChanged();

    VoicegroupLanguageBridge bridge;
    StatusCallback statusCallback;
    CompletionCallback completionCallback;
    HoverCallback hoverCallback;
    const juce::String documentUri = "file:///textedit/voicegroup.inc";
};
