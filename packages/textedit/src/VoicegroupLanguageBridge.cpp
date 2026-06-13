#include "VoicegroupLanguageBridge.h"

#include <cstdlib>

namespace {

juce::String bridgePath()
{
    if (const auto* envPath = std::getenv("TEXTEDIT_VOICEGROUP_BRIDGE_PATH"))
        if (envPath[0] != '\0')
            return envPath;

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
    return service != nullptr
        && create != nullptr
        && destroy != nullptr
        && setRoot != nullptr
        && sync != nullptr
        && complete != nullptr
        && hoverText != nullptr;
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

std::vector<VoicegroupBridgeCompletionItem> VoicegroupLanguageBridge::completions(int line, int character)
{
    auto items = std::vector<VoicegroupBridgeCompletionItem> {};
    if (!isAvailable())
        return items;

    if (complete(service, line, character, collectCompletion, &items) == 0)
        setStatus("Language service: completion failed");
    return items;
}

std::optional<juce::String> VoicegroupLanguageBridge::hover(int line, int character)
{
    auto result = std::optional<juce::String> {};
    if (!isAvailable())
        return result;

    if (hoverText(service, line, character, collectHover, &result) == 0)
        setStatus("Language service: hover failed");
    return result;
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
    return create != nullptr
        && destroy != nullptr
        && setRoot != nullptr
        && sync != nullptr
        && complete != nullptr
        && hoverText != nullptr;
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

void VoicegroupLanguageBridge::collectCompletion(const char* label, const char* detail, void* userData)
{
    auto* items = static_cast<std::vector<VoicegroupBridgeCompletionItem>*>(userData);
    if (items == nullptr || label == nullptr)
        return;

    items->push_back({ label, detail != nullptr ? detail : "" });
}

void VoicegroupLanguageBridge::collectHover(const char* text, void* userData)
{
    auto* result = static_cast<std::optional<juce::String>*>(userData);
    if (result != nullptr && text != nullptr && text[0] != '\0')
        *result = juce::String(text);
}
