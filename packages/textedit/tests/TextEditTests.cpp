#include "TextEditFileStoreTests.h"
#include "TextEditProcessorTests.h"
#include "VoicegroupLanguageBridgeTests.h"

int main() {
  auto passed = true;

  passed &= runTextEditFileStoreTests();
  passed &= runTextEditProcessorTests();
  passed &= runVoicegroupLanguageBridgeTests();

  return passed ? 0 : 1;
}
