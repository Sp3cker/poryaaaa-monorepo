#include "TextEditFileLoader.h"
#include "TextEditFileLoaderTests.h"

#include <cstring>
#include <iostream>

bool runTextEditFileLoaderTests()
{
    auto tempFile = juce::File("/private/tmp")
                        .getNonexistentChildFile("textedit-sysex-path", ".inc", false);
    const auto expectedText = juce::String("voice_directsound 60, 0\n");
    const auto* expectedBytes = expectedText.toRawUTF8();

    juce::FileOutputStream output(tempFile);
    if (!output.openedOk() || !output.write(expectedBytes, std::strlen(expectedBytes)))
    {
        std::cerr << "could not create temp file\n";
        return false;
    }
    output.flush();

    juce::String text;
    juce::String error;
    const auto loaded = loadTextFileForEditor(tempFile.getFullPathName(), text, error);
    tempFile.deleteFile();

    if (!loaded)
    {
        std::cerr << "loadTextFileForEditor failed: " << error << "\n";
        return false;
    }

    if (text != expectedText)
    {
        std::cerr << "loadTextFileForEditor loaded wrong text: expected \""
                  << expectedText << "\", got \"" << text << "\"\n";
        return false;
    }

    return true;
}
