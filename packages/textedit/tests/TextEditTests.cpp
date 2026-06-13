#include "TextEditFileLoaderTests.h"
#include "TextEditProcessorTests.h"

int main()
{
    auto passed = true;

    passed &= runTextEditFileLoaderTests();
    passed &= runTextEditProcessorTests();

    return passed ? 0 : 1;
}
