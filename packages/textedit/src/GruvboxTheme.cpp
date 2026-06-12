#include "GruvboxTheme.h"

namespace {

juce::Colour rgb(uint32_t hex)
{
    return juce::Colour(0xff000000u | hex);
}

} // namespace

namespace GruvboxTheme {

juce::Colour background() { return rgb(0x282828); }
juce::Colour foreground() { return rgb(0xebdbb2); }
juce::Colour gutterBackground() { return rgb(0x1d2021); }
juce::Colour gutterText() { return rgb(0x928374); }
juce::Colour selection() { return rgb(0x504945); }
juce::Colour statusBackground() { return rgb(0x3c3836); }
juce::Colour statusText() { return rgb(0xd5c4a1); }

juce::CodeEditorComponent::ColourScheme codeColourScheme()
{
    juce::CodeEditorComponent::ColourScheme scheme;
    scheme.set("Error", rgb(0xfb4934));
    scheme.set("Comment", rgb(0x928374));
    scheme.set("Keyword", rgb(0xfb4934));
    scheme.set("Macro", rgb(0x8ec07c));
    scheme.set("Identifier", rgb(0xebdbb2));
    scheme.set("Number", rgb(0xd3869b));
    scheme.set("String", rgb(0xb8bb26));
    scheme.set("Operator", rgb(0xfe8019));
    scheme.set("Punctuation", rgb(0xa89984));
    return scheme;
}

} // namespace GruvboxTheme
