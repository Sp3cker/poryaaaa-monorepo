#include "TextEditSysex.h"

juce::String decodePlainTextSysexPayload(const void* data, int sizeInBytes)
{
    if (data == nullptr || sizeInBytes <= 0)
        return {};

    auto* bytes = static_cast<const char*>(data);
    auto size = sizeInBytes;

    if (size > 0 && static_cast<unsigned char>(bytes[0]) == 0xf0)
    {
        ++bytes;
        --size;
    }

    if (size > 0 && static_cast<unsigned char>(bytes[size - 1]) == 0xf7)
        --size;

    return juce::String::fromUTF8(bytes, size);
}
