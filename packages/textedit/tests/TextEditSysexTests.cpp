#include "TextEditSysex.h"
#include "TextEditFileLoaderTests.h"

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

int main()
{
    auto passed = true;

    const unsigned char rawPath[] = "/tmp/current voicegroup.inc";
    passed &= expectEqual("raw payload",
                          decodePlainTextSysexPayload(rawPath, static_cast<int>(sizeof(rawPath) - 1)),
                          "/tmp/current voicegroup.inc");

    const unsigned char wrappedPath[] = { 0xf0, '/', 't', 'm', 'p', '/', 'a', '.', 'i', 'n', 'c', 0xf7 };
    passed &= expectEqual("wrapped payload",
                          decodePlainTextSysexPayload(wrappedPath, static_cast<int>(sizeof(wrappedPath))),
                          "/tmp/a.inc");
    passed &= runTextEditFileLoaderTests();

    return passed ? 0 : 1;
}
