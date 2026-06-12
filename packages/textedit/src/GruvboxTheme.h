#pragma once

#include <juce_gui_extra/juce_gui_extra.h>

namespace GruvboxTheme {

juce::Colour background();
juce::Colour foreground();
juce::Colour gutterBackground();
juce::Colour gutterText();
juce::Colour selection();
juce::Colour statusBackground();
juce::Colour statusText();
juce::CodeEditorComponent::ColourScheme codeColourScheme();

} // namespace GruvboxTheme
