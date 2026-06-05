include(${CMAKE_CURRENT_SOURCE_DIR}/../../max-sdk-base/script/max-pretarget.cmake)

#############################################################
# poryaaaa~ — GameBoy Advance m4a sound engine MSP external
# MIDI capture is now a thin byte buffer (recorder/midi_buffer.{h,cpp}).
# All SMF/tempo/loop/validation lives in code-src/recorder_smf_writer.ts.
#############################################################

set(PORYA_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../../../../packages/poryaaaa"
    CACHE PATH "Path to poryaaaa engine repo root (the dir containing plugin/m4a_engine.c)")

include_directories(
    "${MAX_SDK_INCLUDES}"
    "${MAX_SDK_MSP_INCLUDES}"
    # Wrapper-local sources first so our recorder/ headers shadow the
    # engine repo's stale copy at ${PORYA_ROOT}/plugin/recorder/.
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${PORYA_ROOT}/plugin"
    "${PORYA_ROOT}/plugin/voicegroup"
)

add_subdirectory("${PORYA_ROOT}/plugin/m4a" "${CMAKE_CURRENT_BINARY_DIR}/poryaaaa_m4a_driver")
add_subdirectory("${PORYA_ROOT}/plugin/hw_audio" "${CMAKE_CURRENT_BINARY_DIR}/poryaaaa_hw_audio")

# Only pick up the wrapper sources here; the engine C sources are listed
# explicitly below so we don't accidentally drag in build-system files.
file(GLOB LOCAL_SRC "*.h" "*.c" "*.cpp")
list(REMOVE_ITEM LOCAL_SRC "${CMAKE_CURRENT_SOURCE_DIR}/m4a_engine_recorder_stubs.cpp")

set(ENGINE_SRC
    "${PORYA_ROOT}/plugin/m4a_engine.c"
    "${PORYA_ROOT}/plugin/m4a_tables.c"
)

file(GLOB VG_SRC "${PORYA_ROOT}/plugin/voicegroup/*.c")

set(RECORDER_SRC
    "${CMAKE_CURRENT_SOURCE_DIR}/recorder/midi_buffer.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/recorder/export_capture.cpp"
)

add_library(
    ${PROJECT_NAME}
    MODULE
    ${LOCAL_SRC}
    ${ENGINE_SRC}
    ${VG_SRC}
    ${RECORDER_SRC}
)

set_target_properties(${PROJECT_NAME} PROPERTIES
    C_STANDARD 11
    C_STANDARD_REQUIRED ON
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)

target_link_libraries(${PROJECT_NAME} PRIVATE m4a_driver hw_audio)

include(${CMAKE_CURRENT_SOURCE_DIR}/../../max-sdk-base/script/max-posttarget.cmake)

include(CTest)
if(BUILD_TESTING)
    add_executable(
        porya_recorder_tests
        "${CMAKE_CURRENT_SOURCE_DIR}/recorder/midi_buffer.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/recorder/export_capture.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/recorder/export_capture_tests.cpp"
    )
    target_include_directories(
        porya_recorder_tests
        PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}"
    )
    target_compile_features(porya_recorder_tests PRIVATE cxx_std_17)
    add_test(NAME porya_recorder_tests COMMAND porya_recorder_tests)
endif()
