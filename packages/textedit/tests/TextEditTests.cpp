#include "TextEditFileLoaderTests.h"
#include "TextEditProcessorTests.h"
#include "VoicegroupLspConfigTests.h"

int main()
{
    auto passed = true;

    passed &= runTextEditFileLoaderTests();
    passed &= runTextEditProcessorTests();
    passed &= runVoicegroupLspConfigTests();

    return passed ? 0 : 1;
}
