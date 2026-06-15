#include "VoicegroupLanguageBridge.h"

#include <cstdlib>

namespace
{

constexpr auto bridgeTabActionInsertIndent = 0;
constexpr auto bridgeTabActionSelectRange = 1;
constexpr auto bridgeTabActionMoveCaret = 2;

juce::String bridgePath()
{
    // Highest priority: explicit override via environment variable (very useful
    // for development, testing different builds, or CI).
    if (const auto* envPath = std::getenv("TEXTEDIT_VOICEGROUP_BRIDGE_PATH"))
        if (envPath[0] != '\0')
            return envPath;

    // Next: look for a copy that was embedded inside the .vst3 bundle
    // (Contents/Resources/libVoicegroupBridge.dylib). This makes a distributed
    // or user-installed plugin self-contained.
    {
        auto exe = juce::File::getSpecialLocation(juce::File::currentExecutableFile);
        // exe is typically .../textedit.vst3/Contents/MacOS/textedit
        auto resourcesDir = exe.getParentDirectory().getSiblingFile("Resources");
        auto candidate = resourcesDir.getChildFile("libVoicegroupBridge.dylib");
        if (candidate.existsAsFile())
            return candidate.getFullPathName();
    }

    // Fallback: the path that was baked in at configure time (points into the
    // source tree by default). Useful when developing without a full bundle
    // embedding step or when the dylib lives outside the bundle.
    return TEXTEDIT_VOICEGROUP_BRIDGE_PATH;
}

template <class Function>
Function loadFunction(juce::DynamicLibrary& library, const char* name)
{
    return reinterpret_cast<Function>(library.getFunction(name));
}

} // namespace

VoicegroupLanguageBridge::VoicegroupLanguageBridge()
{
    if (loadBridge())
    {
        service = create(nullptr);

        if (service == nullptr)
            setStatus("Language service: bridge create failed");
    }
}

VoicegroupLanguageBridge::~VoicegroupLanguageBridge()
{
    closeBridge();
}

bool VoicegroupLanguageBridge::isAvailable() const
{
    return service != nullptr && create != nullptr && destroy != nullptr && setRoot != nullptr && sync != nullptr &&
           complete != nullptr && hoverText != nullptr;
}

bool VoicegroupLanguageBridge::setProjectRoot(const juce::File& root)
{
    if (!isAvailable())
        return false;

    const auto rootPath = root.getFullPathName();
    const auto result = setRoot(service, rootPath.toRawUTF8()) != 0;
    if (!result)
        setStatus("Language service: project root rejected");
    return result;
}

bool VoicegroupLanguageBridge::syncDocument(const juce::String& uri, const juce::String& text)
{
    if (!isAvailable())
        return false;

    const auto result = sync(service, uri.toRawUTF8(), text.toRawUTF8()) != 0;
    if (!result)
        setStatus("Language service: document sync failed");
    return result;
}

std::vector<VoicegroupCompletionItem> VoicegroupLanguageBridge::completions(int line, int character)
{
    auto items = std::vector<VoicegroupCompletionItem>{};
    if (!isAvailable())
        return items;

    if (complete(service, line, character, collectCompletion, &items) == 0)
        setStatus("Language service: completion failed");
    return items;
}

std::optional<juce::String> VoicegroupLanguageBridge::hover(int line, int character)
{
    auto result = std::optional<juce::String>{};
    if (!isAvailable())
        return result;

    if (hoverText(service, line, character, collectHover, &result) == 0)
        setStatus("Language service: hover failed");
    return result;
}

std::optional<VoicegroupTabAction>
VoicegroupLanguageBridge::tabAction(int startLine, int startCharacter, int endLine, int endCharacter)
{
    if (!isAvailable())
        return std::nullopt;

    if (tab == nullptr)
        return VoicegroupTabAction{};

    auto actionKind = 0;
    auto resultStartLine = 0;
    auto resultStartCharacter = 0;
    auto resultEndLine = 0;
    auto resultEndCharacter = 0;
    const auto result = tab(service,
                            startLine,
                            startCharacter,
                            endLine,
                            endCharacter,
                            &actionKind,
                            &resultStartLine,
                            &resultStartCharacter,
                            &resultEndLine,
                            &resultEndCharacter) != 0;
    if (!result)
    {
        setStatus("Language service: tab action failed");
        return std::nullopt;
    }

    auto action = VoicegroupTabAction{};
    if (actionKind == bridgeTabActionInsertIndent)
        action.kind = VoicegroupTabActionKind::insertIndent;
    else if (actionKind == bridgeTabActionSelectRange)
        action.kind = VoicegroupTabActionKind::selectRange;
    else if (actionKind == bridgeTabActionMoveCaret)
        action.kind = VoicegroupTabActionKind::moveCaret;
    else
    {
        setStatus("Language service: unknown tab action");
        return std::nullopt;
    }

    action.startLine = resultStartLine;
    action.startCharacter = resultStartCharacter;
    action.endLine = resultEndLine;
    action.endCharacter = resultEndCharacter;
    return action;
}

juce::String VoicegroupLanguageBridge::getStatusText() const
{
    return statusText;
}

bool VoicegroupLanguageBridge::loadBridge()
{
    const auto path = bridgePath();
    if (!juce::File(path).existsAsFile())
    {
        setStatus("Language service: missing bridge " + path);
        return false;
    }

    if (!library.open(path))
    {
        setStatus("Language service: could not load bridge " + path);
        return false;
    }

    if (!loadFunctions())
    {
        setStatus("Language service: bridge ABI mismatch");
        closeBridge();
        return false;
    }

    setStatus("Language service: ready");
    return true;
}

bool VoicegroupLanguageBridge::loadFunctions()
{
    create = loadFunction<CreateFn>(library, "textedit_voicegroup_service_create");
    destroy = loadFunction<DestroyFn>(library, "textedit_voicegroup_service_destroy");
    setRoot = loadFunction<SetProjectRootFn>(library, "textedit_voicegroup_service_set_project_root");
    sync = loadFunction<SyncDocumentFn>(library, "textedit_voicegroup_service_sync_document");
    complete = loadFunction<CompleteFn>(library, "textedit_voicegroup_service_complete");
    hoverText = loadFunction<HoverFn>(library, "textedit_voicegroup_service_hover");
    tab = loadFunction<TabActionFn>(library, "textedit_voicegroup_service_tab_action");
    return create != nullptr && destroy != nullptr && setRoot != nullptr && sync != nullptr && complete != nullptr &&
           hoverText != nullptr;
}

void VoicegroupLanguageBridge::closeBridge()
{
    if (service != nullptr && destroy != nullptr)
        destroy(service);

    service = nullptr;
    library.close();
}

void VoicegroupLanguageBridge::setStatus(juce::String status)
{
    statusText = std::move(status);
}

void VoicegroupLanguageBridge::collectCompletion(const char* label,
                                                 const char* detail,
                                                 const char* insertText,
                                                 int replacementStartLine,
                                                 int replacementStartCharacter,
                                                 int replacementEndLine,
                                                 int replacementEndCharacter,
                                                 void* userData)
{
    auto* items = static_cast<std::vector<VoicegroupCompletionItem>*>(userData);
    if (items == nullptr || label == nullptr || insertText == nullptr)
        return;

    items->push_back({label,
                      detail != nullptr ? detail : "",
                      insertText,
                      replacementStartLine,
                      replacementStartCharacter,
                      replacementEndLine,
                      replacementEndCharacter});
}

void VoicegroupLanguageBridge::collectHover(const char* text, void* userData)
{
    auto* result = static_cast<std::optional<juce::String>*>(userData);
    if (result != nullptr && text != nullptr && text[0] != '\0')
        *result = juce::String(text);
}
