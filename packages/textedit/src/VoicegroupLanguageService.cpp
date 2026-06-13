#include "VoicegroupLanguageService.h"

void EmbeddedLanguageService::setStatusCallback(StatusCallback callback)
{
    statusCallback = std::move(callback);
}

void EmbeddedLanguageService::setCompletionCallback(CompletionCallback callback)
{
    completionCallback = std::move(callback);
}

void EmbeddedLanguageService::setHoverCallback(HoverCallback callback)
{
    hoverCallback = std::move(callback);
}

bool EmbeddedLanguageService::setProjectRoot(const juce::File& root)
{
    const auto result = bridge.setProjectRoot(root);
    notifyStatusChanged();
    return result;
}

bool EmbeddedLanguageService::canRequestContext() const
{
    return bridge.isAvailable();
}

void EmbeddedLanguageService::syncDocument(const juce::String& text)
{
    bridge.syncDocument(documentUri, text);
    notifyStatusChanged();
}

void EmbeddedLanguageService::requestCompletion(int line, int character)
{
    if (!completionCallback)
        return;

    auto completions = std::vector<VoicegroupCompletionItem> {};
    for (const auto& item : bridge.completions(line, character))
        completions.push_back({ item.label, item.detail });

    completionCallback(std::move(completions));
    notifyStatusChanged();
}

void EmbeddedLanguageService::requestHover(int line, int character)
{
    if (!hoverCallback)
        return;

    const auto hoverText = bridge.hover(line, character);
    hoverCallback(hoverText.value_or(juce::String {}));
    notifyStatusChanged();
}

void EmbeddedLanguageService::requestSignatureHelp(int line, int character)
{
    juce::ignoreUnused(line, character);
}

juce::String EmbeddedLanguageService::getStatusText() const
{
    return bridge.getStatusText();
}

void EmbeddedLanguageService::notifyStatusChanged()
{
    if (statusCallback)
        statusCallback();
}
