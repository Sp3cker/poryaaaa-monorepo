#include "TextEditFileStore.h"

#include <cstdlib>
#include <utility>

TextEditFileStore::TextEditFileStore()
    : TextEditFileStore(getDefaultPoryaaaaProjectsJsonFile())
{
}

TextEditFileStore::TextEditFileStore(juce::File projectsJsonFileToUse)
    : projectsJsonFile(std::move(projectsJsonFileToUse))
{
}

void TextEditFileStore::setErrorListener(ErrorListener listener)
{
    errorListener = std::move(listener);
}

bool TextEditFileStore::loadCurrentVoicegroup(TextEditVoicegroupDocument& document)
{
    juce::String errorMessage;
    TextEditProjectState state;
    if (!loadPoryaaaaProjectState(projectsJsonFile, state, errorMessage))
    {
        reportError(TextEditFileStoreOperation::loadVoicegroup, errorMessage);
        return false;
    }

    auto voicegroupFile = getVoicegroupFileForProjectState(state);
    juce::String text;
    if (!loadTextFileForEditor(voicegroupFile.getFullPathName(), text, errorMessage))
    {
        reportError(TextEditFileStoreOperation::loadVoicegroup, errorMessage);
        return false;
    }

    currentVoicegroupFile = voicegroupFile;
    document = { state.root, state.bank, voicegroupFile, text };
    return true;
}

bool TextEditFileStore::saveCurrentVoicegroup(const juce::String& text)
{
    if (currentVoicegroupFile == juce::File())
    {
        reportError(TextEditFileStoreOperation::saveVoicegroup, "no voicegroup file is loaded");
        return false;
    }

    if (!currentVoicegroupFile.existsAsFile())
    {
        reportError(TextEditFileStoreOperation::saveVoicegroup,
                    "voicegroup file not found: " + currentVoicegroupFile.getFullPathName());
        return false;
    }

    if (!currentVoicegroupFile.replaceWithText(text, false, false, "\n"))
    {
        reportError(TextEditFileStoreOperation::saveVoicegroup,
                    "could not write voicegroup file: " + currentVoicegroupFile.getFullPathName());
        return false;
    }

    return true;
}

juce::File TextEditFileStore::getCurrentVoicegroupFile() const
{
    return currentVoicegroupFile;
}

void TextEditFileStore::reportError(TextEditFileStoreOperation operation, const juce::String& message)
{
    if (errorListener)
        errorListener({ operation, message });
}

bool loadTextFileForEditor(const juce::String& path, juce::String& text, juce::String& errorMessage)
{
    const juce::File file(path);
    if (!file.existsAsFile())
    {
        errorMessage = "file not found: " + path;
        return false;
    }

    auto input = file.createInputStream();
    if (input == nullptr || !input->openedOk())
    {
        errorMessage = "could not read file: " + path;
        return false;
    }

    text = input->readEntireStreamAsString();
    errorMessage = {};
    return true;
}

juce::File getDefaultPoryaaaaProjectsJsonFile()
{
#if JUCE_WINDOWS
    if (const auto* appData = std::getenv("APPDATA"))
        if (appData[0] != '\0')
            return juce::File(appData).getChildFile("poryaaaa").getChildFile("projects.json");

    return juce::File::getSpecialLocation(juce::File::userHomeDirectory)
        .getChildFile("AppData")
        .getChildFile("Roaming")
        .getChildFile("poryaaaa")
        .getChildFile("projects.json");
#elif JUCE_MAC
    return juce::File::getSpecialLocation(juce::File::userHomeDirectory)
        .getChildFile("Library")
        .getChildFile("Application Support")
        .getChildFile("poryaaaa")
        .getChildFile("projects.json");
#else
    if (const auto* xdgConfigHome = std::getenv("XDG_CONFIG_HOME"))
        if (xdgConfigHome[0] != '\0')
            return juce::File(xdgConfigHome).getChildFile("poryaaaa").getChildFile("projects.json");

    return juce::File::getSpecialLocation(juce::File::userHomeDirectory)
        .getChildFile(".config")
        .getChildFile("poryaaaa")
        .getChildFile("projects.json");
#endif
}

juce::File getVoicegroupFileForProjectState(const TextEditProjectState& state)
{
    return state.root
        .getChildFile("sound")
        .getChildFile("voicegroups")
        .getChildFile(state.bank + ".inc");
}

bool loadPoryaaaaProjectState(const juce::File& projectsJsonFile,
                              TextEditProjectState& state,
                              juce::String& errorMessage)
{
    if (!projectsJsonFile.existsAsFile())
    {
        errorMessage = "projects.json not found: " + projectsJsonFile.getFullPathName();
        return false;
    }

    juce::var parsed;
    const auto result = juce::JSON::parse(projectsJsonFile.loadFileAsString(), parsed);
    if (!result.wasOk())
    {
        errorMessage = "could not parse projects.json: " + result.getErrorMessage();
        return false;
    }

    auto* object = parsed.getDynamicObject();
    if (object == nullptr)
    {
        errorMessage = "projects.json must contain an object";
        return false;
    }

    const auto rootValue = object->getProperty("root");
    const auto bankValue = object->getProperty("bank");
    if (!rootValue.isString() || rootValue.toString().isEmpty())
    {
        errorMessage = "projects.json missing root";
        return false;
    }

    if (!bankValue.isString() || bankValue.toString().isEmpty())
    {
        errorMessage = "projects.json missing bank";
        return false;
    }

    state = { juce::File(rootValue.toString()), bankValue.toString() };
    errorMessage = {};
    return true;
}
