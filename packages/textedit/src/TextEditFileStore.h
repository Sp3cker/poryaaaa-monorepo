#pragma once

#include <juce_core/juce_core.h>

#include <functional>

struct TextEditProjectState
{
    juce::File root;
    juce::String bank;
};

struct TextEditVoicegroupDocument
{
    juce::File projectRoot;
    juce::String bank;
    juce::File file;
    juce::String text;
};

enum class TextEditFileStoreOperation
{
    loadVoicegroup,
    saveVoicegroup
};

struct TextEditFileStoreError
{
    TextEditFileStoreOperation operation;
    juce::String message;
};

class TextEditFileStore final
{
public:
    using ErrorListener = std::function<void(const TextEditFileStoreError&)>;

    TextEditFileStore();
    explicit TextEditFileStore(juce::File projectsJsonFile);

    void setErrorListener(ErrorListener listener);
    bool loadCurrentVoicegroup(TextEditVoicegroupDocument& document);
    bool saveCurrentVoicegroup(const juce::String& text);
    bool saveVoicegroupAs(const juce::File& file, const juce::String& text);
    bool writeVoicegroupFile(const juce::File& file, const juce::String& text);

    juce::File getCurrentVoicegroupFile() const;

private:
    void reportError(TextEditFileStoreOperation operation, const juce::String& message);

    juce::File projectsJsonFile;
    juce::File currentVoicegroupFile;
    ErrorListener errorListener;
};

bool loadTextFileForEditor(const juce::String& path, juce::String& text, juce::String& errorMessage);
juce::File getDefaultPoryaaaaProjectsJsonFile();
juce::File getVoicegroupFileForProjectState(const TextEditProjectState& state);
bool loadPoryaaaaProjectState(const juce::File& projectsJsonFile,
                              TextEditProjectState& state,
                              juce::String& errorMessage);
