#pragma once

#include <juce_gui_extra/juce_gui_extra.h>

#include <optional>
#include <vector>

struct VoicegroupBridgeCompletionItem
{
    juce::String label;
    juce::String detail;
};

class VoicegroupLanguageBridge final
{
public:
    VoicegroupLanguageBridge();
    ~VoicegroupLanguageBridge();

    bool isAvailable() const;
    bool setProjectRoot(const juce::File& root);
    bool syncDocument(const juce::String& uri, const juce::String& text);
    std::vector<VoicegroupBridgeCompletionItem> completions(int line, int character);
    std::optional<juce::String> hover(int line, int character);
    juce::String getStatusText() const;

private:
    using ServiceHandle = void*;
    using CompletionCallback = void (*)(const char* label, const char* detail, void* userData);
    using HoverCallback = void (*)(const char* text, void* userData);

    using CreateFn = ServiceHandle (*)(const char* projectRoot);
    using DestroyFn = void (*)(ServiceHandle service);
    using SetProjectRootFn = int (*)(ServiceHandle service, const char* projectRoot);
    using SyncDocumentFn = int (*)(ServiceHandle service, const char* uri, const char* text);
    using CompleteFn = int (*)(ServiceHandle service, int line, int character, CompletionCallback callback, void* userData);
    using HoverFn = int (*)(ServiceHandle service, int line, int character, HoverCallback callback, void* userData);

    bool loadBridge();
    bool loadFunctions();
    void closeBridge();
    void setStatus(juce::String status);

    static void collectCompletion(const char* label, const char* detail, void* userData);
    static void collectHover(const char* text, void* userData);

    juce::DynamicLibrary library;
    ServiceHandle service = nullptr;
    CreateFn create = nullptr;
    DestroyFn destroy = nullptr;
    SetProjectRootFn setRoot = nullptr;
    SyncDocumentFn sync = nullptr;
    CompleteFn complete = nullptr;
    HoverFn hoverText = nullptr;
    juce::String statusText = "Language service: bridge not loaded";
};
