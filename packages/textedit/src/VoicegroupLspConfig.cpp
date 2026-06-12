#include "VoicegroupLspConfig.h"

#include <cstdlib>

namespace {

juce::String findExecutableOnPath(const juce::String& executableName, const juce::String& pathEnvironment)
{
    if (executableName.isEmpty() || pathEnvironment.isEmpty())
        return {};

#if JUCE_WINDOWS
    constexpr auto pathSeparator = ";";
    auto executableNames = juce::StringArray { executableName };
    if (!executableName.endsWithIgnoreCase(".exe"))
        executableNames.add(executableName + ".exe");
#else
    constexpr auto pathSeparator = ":";
    auto executableNames = juce::StringArray { executableName };
#endif

    auto directories = juce::StringArray::fromTokens(pathEnvironment, pathSeparator, {});
    for (const auto& directory : directories)
    {
        if (directory.isEmpty())
            continue;

        const juce::File baseDirectory(directory);
        for (const auto& name : executableNames)
        {
            const auto candidate = baseDirectory.getChildFile(name);
            if (candidate.existsAsFile())
                return candidate.getFullPathName();
        }
    }

    return {};
}

} // namespace

juce::String resolveVoicegroupLspServerPath(const juce::String& compileTimeDefault)
{
    const auto envOverride = std::getenv("VOICEGROUP_LSP_PATH");
    const auto pathEnvironment = std::getenv("PATH");

    return resolveVoicegroupLspServerPathForEnvironment(compileTimeDefault,
                                                        envOverride != nullptr ? juce::String(envOverride) : juce::String(),
                                                        pathEnvironment != nullptr ? juce::String(pathEnvironment) : juce::String());
}

juce::String resolveVoicegroupLspServerPathForEnvironment(const juce::String& compileTimeDefault,
                                                          const juce::String& envOverride,
                                                          const juce::String& pathEnvironment)
{
    if (envOverride.isNotEmpty())
        return envOverride;

    if (juce::File(compileTimeDefault).existsAsFile())
        return compileTimeDefault;

    const auto pathMatch = findExecutableOnPath(compileTimeDefault, pathEnvironment);
    if (pathMatch.isNotEmpty())
        return pathMatch;

    return compileTimeDefault;
}
