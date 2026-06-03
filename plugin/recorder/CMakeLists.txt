# recorder — MIDI capture core + SMF writer vendored from the
# ccomidi_recorder.clap project.  Pure C++17, no audio-engine
# dependencies; the engine will own a RecorderCore instance and
# expose it via a small accessor in a future turn.

set(MIDIFILE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/midifile)

add_library(recorder STATIC
    recorder_core.cpp
    smf_writer.cpp
    ${MIDIFILE_DIR}/src/Binasc.cpp
    ${MIDIFILE_DIR}/src/MidiEvent.cpp
    ${MIDIFILE_DIR}/src/MidiEventList.cpp
    ${MIDIFILE_DIR}/src/MidiFile.cpp
    ${MIDIFILE_DIR}/src/MidiMessage.cpp
    ${MIDIFILE_DIR}/src/Options.cpp
)
poryaaaa_enable_project_source_warnings(
    ${CMAKE_CURRENT_SOURCE_DIR}/recorder_core.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/smf_writer.cpp
)

# PUBLIC so consumers can write `#include "recorder/recorder_core.h"`.
target_include_directories(recorder PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)

# midifile headers are an implementation detail of smf_writer.cpp.
target_include_directories(recorder PRIVATE ${MIDIFILE_DIR}/include)
