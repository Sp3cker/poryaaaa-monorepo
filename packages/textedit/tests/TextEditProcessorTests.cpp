#include "TextEditProcessorTests.h"

#include "TextEditProcessor.h"

#include <iostream>

bool runTextEditProcessorTests()
{
    TextEditProcessor processor;

    if (!processor.acceptsMidi())
        return true;

    std::cerr << "processor rejects MIDI input failed: acceptsMidi() returned true\n";
    return false;
}
