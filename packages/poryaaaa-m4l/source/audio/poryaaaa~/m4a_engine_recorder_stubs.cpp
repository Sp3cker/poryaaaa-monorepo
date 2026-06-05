// The engine repo's m4a_engine.c calls m4a_engine_recorder_init/destroy
// (the engine's own RecorderCore-field lifecycle). This M4L wrapper never
// uses the engine's recorder field — all MIDI capture lives in MidiBuffer
// owned by t_porya — so we satisfy those symbols with no-op stubs. Without
// these the engine's .c file fails to link the wrapper external.
#include "m4a_engine.h"
#include "m4a_engine_recorder.h"

extern "C" {

void m4a_engine_recorder_init(M4AEngine *engine) {
    if (engine) engine->recorder = nullptr;
}

void m4a_engine_recorder_destroy(M4AEngine *engine) {
    (void)engine;
}

}  // extern "C"
