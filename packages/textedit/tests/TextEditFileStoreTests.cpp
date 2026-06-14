#include "TextEditFileStore.h"
#include "TextEditFileStoreTests.h"

#include <cstring>
#include <iostream>

namespace {

bool writeText(const juce::File& file, const juce::String& text)
{
    file.getParentDirectory().createDirectory();
    juce::FileOutputStream output(file);
    const auto* bytes = text.toRawUTF8();
    const auto wrote = output.openedOk() && output.write(bytes, std::strlen(bytes));
    output.flush();
    return wrote;
}

bool expectLoadProjectState(const juce::String& json, const juce::String& root, const juce::String& bank)
{
    auto tempFile = juce::File("/private/tmp")
                        .getNonexistentChildFile("textedit-projects", ".json", false);
    if (!writeText(tempFile, json))
    {
        std::cerr << "could not create projects.json test file\n";
        return false;
    }

    TextEditProjectState state;
    juce::String error;
    const auto loaded = loadPoryaaaaProjectState(tempFile, state, error);
    tempFile.deleteFile();
    if (!loaded)
    {
        std::cerr << "loadPoryaaaaProjectState failed: " << error << "\n";
        return false;
    }

    if (state.root.getFullPathName() != root || state.bank != bank)
    {
        std::cerr << "loadPoryaaaaProjectState loaded wrong state\n";
        return false;
    }

    return true;
}

bool expectRejectProjectState(const juce::String& json)
{
    auto tempFile = juce::File("/private/tmp")
                        .getNonexistentChildFile("textedit-projects", ".json", false);
    if (!writeText(tempFile, json))
    {
        std::cerr << "could not create projects.json test file\n";
        return false;
    }

    TextEditProjectState state;
    juce::String error;
    const auto loaded = loadPoryaaaaProjectState(tempFile, state, error);
    tempFile.deleteFile();
    if (loaded || error.isEmpty())
    {
        std::cerr << "loadPoryaaaaProjectState accepted invalid projects.json\n";
        return false;
    }

    return true;
}

} // namespace

bool runTextEditFileStoreTests()
{
    auto tempFile = juce::File("/private/tmp")
                        .getNonexistentChildFile("textedit-file-loader", ".inc", false);
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

    const auto projectStateLoaded = expectLoadProjectState(
        "{ \"root\": \"/projects/poryaaaa\", \"bank\": \"voicegroup000\" }",
        "/projects/poryaaaa",
        "voicegroup000");
    const auto missingRootRejected = expectRejectProjectState("{ \"bank\": \"voicegroup000\" }");
    const auto missingBankRejected = expectRejectProjectState("{ \"root\": \"/projects/poryaaaa\" }");
    const auto nonStringRootRejected = expectRejectProjectState("{ \"root\": 12, \"bank\": \"voicegroup000\" }");
    const auto malformedRejected = expectRejectProjectState("{ not json");
    auto missingProjectsFile = juce::File("/private/tmp")
                                   .getNonexistentChildFile("textedit-missing-projects", ".json", false);
    TextEditProjectState state;
    juce::String missingError;
    const auto missingRejected = !loadPoryaaaaProjectState(missingProjectsFile, state, missingError)
                              && missingError.isNotEmpty();

    return projectStateLoaded
        && missingRootRejected
        && missingBankRejected
        && nonStringRootRejected
        && malformedRejected
        && missingRejected;
}
