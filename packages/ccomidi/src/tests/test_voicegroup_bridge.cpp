#include "plugin/voicegroup_bridge.h"

#include <cstdio>
#include <cstdlib>
#include <string>

namespace
{

int g_testsRun = 0;
int g_testsPassed = 0;

#define ASSERT_TRUE(cond, msg)                                                                                         \
    do                                                                                                                 \
    {                                                                                                                  \
        ++g_testsRun;                                                                                                  \
        if (!(cond))                                                                                                   \
            std::fprintf(stderr, "FAIL: %s (line %d)\n", msg, __LINE__);                                               \
        else                                                                                                           \
            ++g_testsPassed;                                                                                           \
    } while (0)

#define ASSERT_EQ(actual, expected, msg)                                                                               \
    do                                                                                                                 \
    {                                                                                                                  \
        ++g_testsRun;                                                                                                  \
        if ((actual) != (expected))                                                                                    \
            std::fprintf(stderr,                                                                                       \
                         "FAIL: %s: expected %d, got %d (line %d)\n",                                                  \
                         msg,                                                                                          \
                         static_cast<int>(expected),                                                                   \
                         static_cast<int>(actual),                                                                     \
                         __LINE__);                                                                                    \
        else                                                                                                           \
            ++g_testsPassed;                                                                                           \
    } while (0)

void test_parse_slots_with_drumset_metadata()
{
    const std::string body = R"json({
  "root": "/tmp/project",
  "bank": "main",
  "slots": [
    {"program": 0, "name": "Piano"},
    {"program": 1, "name": "Drums", "typeCode": 128, "drumset": [
      {"note": 36, "name": "DirectSoundWaveData_Kick"},
      {"note": "bad", "name": "Bad"},
      {"note": 38},
      {"note": 40, "name": "DirectSoundWaveData_Snare"}
    ]}
  ]
})json";

    const ccomidi::VoiceSlotLoad load = ccomidi::voicegroup_bridge_parse_state_body(body);

    ASSERT_TRUE(load.error.empty(), "valid state body parses without error");
    ASSERT_EQ(load.slots.size(), 2, "both voice slots parse");
    ASSERT_EQ(load.slots[0].program, 0, "normal slot program parses");
    ASSERT_EQ(load.slots[0].typeCode, 0, "missing typeCode defaults to zero");
    ASSERT_EQ(load.slots[0].drumset.size(), 0, "normal slot has no drum pads");

    ASSERT_EQ(load.slots[1].program, 1, "drumset slot program parses");
    ASSERT_EQ(load.slots[1].typeCode, 128, "drumset slot typeCode parses");
    ASSERT_EQ(load.slots[1].drumset.size(), 2, "malformed drum pads are ignored");
    ASSERT_EQ(load.slots[1].drumset[0].note, 36, "first drum pad note parses");
    ASSERT_TRUE(load.slots[1].drumset[0].name == "DirectSoundWaveData_Kick", "first drum pad name parses");
    ASSERT_EQ(load.slots[1].drumset[1].note, 40, "second drum pad note parses");
    ASSERT_TRUE(load.slots[1].drumset[1].name == "DirectSoundWaveData_Snare", "second drum pad name parses");
}

} // namespace

int main()
{
    test_parse_slots_with_drumset_metadata();

    std::printf("[ccomidi_voicegroup_bridge] %d/%d passed\n", g_testsPassed, g_testsRun);
    return g_testsPassed == g_testsRun ? EXIT_SUCCESS : EXIT_FAILURE;
}
