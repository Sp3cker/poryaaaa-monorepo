#pragma once

#include <juce_core/juce_core.h>

juce::String resolveVoicegroupLspServerPath(const juce::String& compileTimeDefault);
juce::String resolveVoicegroupLspServerPathForEnvironment(const juce::String& compileTimeDefault,
                                                          const juce::String& envOverride,
                                                          const juce::String& pathEnvironment);
