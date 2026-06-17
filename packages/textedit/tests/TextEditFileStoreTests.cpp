#include "TextEditFileStore.h"
#include "TextEditFileStoreTests.h"
#include "projects_json_path.h"

#include <cstring>
#include <iostream>

namespace
{

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
    auto tempFile = juce::File("/private/tmp").getNonexistentChildFile("textedit-projects", ".json", false);
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
    auto tempFile = juce::File("/private/tmp").getNonexistentChildFile("textedit-projects", ".json", false);
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

bool expectLoadAndSaveVoicegroup()
{
    auto tempRoot = juce::File("/private/tmp").getNonexistentChildFile("textedit-project-root", "", false);
    const auto projectsJsonFile =
        juce::File("/private/tmp").getNonexistentChildFile("textedit-projects", ".json", false);
    const auto voicegroupFile = tempRoot.getChildFile("sound").getChildFile("voicegroups").getChildFile("alpha.inc");
    const auto initialText = juce::String("voice_group alpha\n\tvoice_directsound 60, 0\n");
    const auto savedText = juce::String("voice_group alpha\n\tvoice_directsound 61, 0\n");
    const auto projectsJson = "{ \"root\": \"" + tempRoot.getFullPathName() + "\", \"bank\": \"alpha\" }";
    if (!writeText(projectsJsonFile, projectsJson) || !writeText(voicegroupFile, initialText))
    {
        std::cerr << "could not create voicegroup store test files\n";
        projectsJsonFile.deleteFile();
        tempRoot.deleteRecursively();
        return false;
    }

    TextEditFileStore store(projectsJsonFile);
    TextEditVoicegroupDocument document;
    auto errorReported = false;
    store.setErrorListener([&errorReported](const auto&) { errorReported = true; });
    const auto loaded = store.loadCurrentVoicegroup(document);
    const auto saved = store.saveCurrentVoicegroup(savedText);
    const auto reloadedText = voicegroupFile.loadFileAsString();
    projectsJsonFile.deleteFile();
    tempRoot.deleteRecursively();
    if (!loaded)
    {
        std::cerr << "loadCurrentVoicegroup failed\n";
        return false;
    }

    if (document.projectRoot != tempRoot || document.bank != "alpha" || document.file != voicegroupFile ||
        document.text != initialText)
    {
        std::cerr << "loadCurrentVoicegroup loaded wrong document\n";
        return false;
    }

    if (!saved || errorReported || reloadedText != savedText)
    {
        std::cerr << "saveCurrentVoicegroup failed\n";
        return false;
    }

    return true;
}

bool expectSaveVoicegroupAs()
{
    auto tempRoot = juce::File("/private/tmp").getNonexistentChildFile("textedit-save-as", "", false);
    const auto firstFile = tempRoot.getChildFile("alpha.inc");
    const auto secondFile = tempRoot.getChildFile("beta.inc");
    const auto firstText = juce::String("voice_group alpha\n");
    const auto secondText = juce::String("voice_group beta\n");
    tempRoot.createDirectory();
    TextEditFileStore store;
    auto errorReported = false;
    store.setErrorListener([&errorReported](const auto&) { errorReported = true; });
    const auto savedAs = store.saveVoicegroupAs(firstFile, firstText);
    const auto savedCurrent = store.saveCurrentVoicegroup(secondText);
    const auto firstFileText = firstFile.loadFileAsString();
    const auto secondFileExists = secondFile.existsAsFile();
    tempRoot.deleteRecursively();
    if (!savedAs || !savedCurrent || errorReported || secondFileExists || firstFileText != secondText)
    {
        std::cerr << "saveVoicegroupAs failed\n";
        return false;
    }

    return true;
}

bool expectWriteVoicegroupFileDoesNotRetargetCurrentVoicegroup()
{
    auto tempRoot = juce::File("/private/tmp").getNonexistentChildFile("textedit-copy-as", "", false);
    const auto currentFile = tempRoot.getChildFile("alpha.inc");
    const auto copyFile = tempRoot.getChildFile("beta.inc");
    const auto currentText = juce::String("voice_group alpha\n");
    const auto copyText = juce::String("\tvoice_directsound 60, 0\n");
    const auto savedText = juce::String("voice_group alpha\n\tvoice_square_1 60, 0, 0, 2, 0, 0, 15, 0\n");
    tempRoot.createDirectory();
    TextEditFileStore store;
    auto errorReported = false;
    store.setErrorListener([&errorReported](const auto&) { errorReported = true; });
    const auto savedAs = store.saveVoicegroupAs(currentFile, currentText);
    const auto copied = store.writeVoicegroupFile(copyFile, copyText);
    const auto savedCurrent = store.saveCurrentVoicegroup(savedText);
    const auto currentFileText = currentFile.loadFileAsString();
    const auto copyFileText = copyFile.loadFileAsString();
    tempRoot.deleteRecursively();
    if (!savedAs || !copied || !savedCurrent || errorReported || currentFileText != savedText ||
        copyFileText != copyText)
    {
        std::cerr << "writeVoicegroupFile retargeted current voicegroup\n";
        return false;
    }

    return true;
}

bool expectRejectSaveBeforeLoad()
{
    TextEditFileStore store(juce::File("/private/tmp").getNonexistentChildFile("textedit-projects", ".json", false));
    auto errorReported = false;
    auto operation = TextEditFileStoreOperation::loadVoicegroup;
    juce::String message;
    store.setErrorListener(
        [&](const auto& error)
        {
            errorReported = true;
            operation = error.operation;
            message = error.message;
        });
    return !store.saveCurrentVoicegroup("voice_group alpha\n") && errorReported &&
           operation == TextEditFileStoreOperation::saveVoicegroup && message.isNotEmpty();
}

bool expectReportLoadError()
{
    TextEditFileStore store(
        juce::File("/private/tmp").getNonexistentChildFile("textedit-missing-projects", ".json", false));
    TextEditVoicegroupDocument document;
    auto errorReported = false;
    auto operation = TextEditFileStoreOperation::saveVoicegroup;
    juce::String message;
    store.setErrorListener(
        [&](const auto& error)
        {
            errorReported = true;
            operation = error.operation;
            message = error.message;
        });
    return !store.loadCurrentVoicegroup(document) && errorReported &&
           operation == TextEditFileStoreOperation::loadVoicegroup && message.isNotEmpty();
}

bool expectDefaultProjectsJsonPathUsesSharedContract()
{
    char expected[700];
    if (!poryaaaa_projects_json_default_path(expected, sizeof(expected)))
        return false;
    return getDefaultPoryaaaaProjectsJsonFile() == juce::File(expected);
}

} // namespace

bool runTextEditFileStoreTests()
{
    auto tempFile = juce::File("/private/tmp").getNonexistentChildFile("textedit-file-loader", ".inc", false);
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
        std::cerr << "loadTextFileForEditor loaded wrong text: expected \"" << expectedText << "\", got \"" << text
                  << "\"\n";
        return false;
    }

    const auto projectStateLoaded = expectLoadProjectState(
        "{ \"root\": \"/projects/poryaaaa\", \"bank\": \"voicegroup000\" }", "/projects/poryaaaa", "voicegroup000");
    const auto missingRootRejected = expectRejectProjectState("{ \"bank\": \"voicegroup000\" }");
    const auto missingBankRejected = expectRejectProjectState("{ \"root\": \"/projects/poryaaaa\" }");
    const auto nonStringRootRejected = expectRejectProjectState("{ \"root\": 12, \"bank\": \"voicegroup000\" }");
    const auto malformedRejected = expectRejectProjectState("{ not json");
    const auto voicegroupLoadedAndSaved = expectLoadAndSaveVoicegroup();
    const auto voicegroupSavedAs = expectSaveVoicegroupAs();
    const auto voicegroupCopySavedAs = expectWriteVoicegroupFileDoesNotRetargetCurrentVoicegroup();
    const auto saveBeforeLoadRejected = expectRejectSaveBeforeLoad();
    const auto loadErrorReported = expectReportLoadError();
    const auto defaultPathUsesSharedContract = expectDefaultProjectsJsonPathUsesSharedContract();
    auto missingProjectsFile =
        juce::File("/private/tmp").getNonexistentChildFile("textedit-missing-projects", ".json", false);
    TextEditProjectState state;
    juce::String missingError;
    const auto missingRejected =
        !loadPoryaaaaProjectState(missingProjectsFile, state, missingError) && missingError.isNotEmpty();

    return projectStateLoaded && missingRootRejected && missingBankRejected && nonStringRootRejected &&
           malformedRejected && voicegroupLoadedAndSaved && voicegroupSavedAs && voicegroupCopySavedAs &&
           saveBeforeLoadRejected && loadErrorReported && defaultPathUsesSharedContract && missingRejected;
}
