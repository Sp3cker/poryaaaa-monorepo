#include "TextEditEditorCompletionTests.h"

#include "TextEditEditor.h"

#include <iostream>

bool runTextEditEditorCompletionTests()
{
    juce::CodeDocument document;
    VoicegroupTokeniser tokeniser;
    VoicegroupCodeEditor editor(document, &tokeniser);

    const auto original = juce::String("\tvoice_dir");
    const auto completion = juce::String("voice_directsound 60, 0, DirectSoundWaveData_, 255, 0, 255, 127");

    document.replaceAllContent(original);
    document.clearUndoHistory();
    editor.selectRegion(juce::CodeDocument::Position(document, 0, 1), juce::CodeDocument::Position(document, 0, 10));
    editor.insertTextAtCaret(completion);
    editor.selectRegion(juce::CodeDocument::Position(document, 0, 19), juce::CodeDocument::Position(document, 0, 21));
    editor.undo();

    if (document.getAllContent() != original)
    {
        std::cerr << "completion undo failed: expected original text, got " << document.getAllContent() << "\n";
        return false;
    }

    return true;
}
