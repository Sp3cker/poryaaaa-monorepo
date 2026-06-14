#pragma once

#include <juce_core/juce_core.h>

struct TextEditProjectState {
    juce::File root;
    juce::String bank;
};

bool loadTextFileForEditor(const juce::String& path, juce::String& text, juce::String& errorMessage);
juce::File getDefaultPoryaaaaProjectsJsonFile();
bool loadPoryaaaaProjectState(const juce::File& projectsJsonFile,
                              TextEditProjectState& state,
                              juce::String& errorMessage);
