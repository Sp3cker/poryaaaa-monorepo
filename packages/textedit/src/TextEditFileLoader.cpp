#include "TextEditFileLoader.h"

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
