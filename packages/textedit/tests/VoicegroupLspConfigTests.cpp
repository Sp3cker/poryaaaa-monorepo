#include "VoicegroupLspConfig.h"

#include "VoicegroupLspConfigTests.h"

#include <cstring>
#include <iostream>

namespace {

bool expectEqual(const char* name, const juce::String& actual, const juce::String& expected)
{
    if (actual == expected)
        return true;

    std::cerr << name << " failed: expected \"" << expected << "\", got \"" << actual << "\"\n";
    return false;
}

} // namespace

bool runVoicegroupLspConfigTests()
{
    auto passed = true;

    passed &= expectEqual("env override",
                          resolveVoicegroupLspServerPathForEnvironment("/compile/default/voicegroup-lsp",
                                                                       "/tmp/voicegroup-lsp",
                                                                       {}),
                          "/tmp/voicegroup-lsp");

    auto tempDirectory = juce::File("/private/tmp").getNonexistentChildFile("textedit-lsp-path", "", false);
    if (!tempDirectory.createDirectory())
    {
        std::cerr << "could not create temp directory\n";
        return false;
    }

#if JUCE_WINDOWS
    auto executable = tempDirectory.getChildFile("voicegroup-lsp.exe");
#else
    auto executable = tempDirectory.getChildFile("voicegroup-lsp");
#endif
    const auto* contents = "";
    juce::FileOutputStream output(executable);
    if (!output.openedOk() || !output.write(contents, std::strlen(contents)))
    {
        std::cerr << "could not create path test executable\n";
        tempDirectory.deleteRecursively();
        return false;
    }
    output.flush();

    passed &= expectEqual("PATH fallback",
                          resolveVoicegroupLspServerPathForEnvironment("voicegroup-lsp",
                                                                       {},
                                                                       tempDirectory.getFullPathName()),
                          executable.getFullPathName());

    passed &= expectEqual("unresolved fallback",
                          resolveVoicegroupLspServerPathForEnvironment("voicegroup-lsp", {}, {}),
                          "voicegroup-lsp");

    tempDirectory.deleteRecursively();

    return passed;
}
