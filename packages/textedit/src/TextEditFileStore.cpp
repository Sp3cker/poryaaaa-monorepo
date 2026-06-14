#include "TextEditFileStore.h"

#include <cstdlib>

bool loadTextFileForEditor(const juce::String& path, juce::String& text, juce::String& errorMessage)
{
    const juce::File file(path);
    if (!file.existsAsFile())
    {
        errorMessage = "file not found: " + path;
        return false;
    }

    text = file.loadFileAsString();
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
