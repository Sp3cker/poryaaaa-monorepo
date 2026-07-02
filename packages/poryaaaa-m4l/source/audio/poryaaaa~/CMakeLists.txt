include(${CMAKE_CURRENT_SOURCE_DIR}/../../max-sdk-base/script/max-pretarget.cmake)

#############################################################
# poryaaaa~ — GameBoy Advance m4a sound engine MSP external
# MIDI capture is now a thin byte buffer (recorder/midi_buffer.{h,cpp}).
# All SMF/tempo/loop/validation lives in code-src/recorder_smf_writer.ts.
#############################################################

set(PORYA_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../../../../packages/poryaaaa"
    CACHE PATH "Path to poryaaaa engine repo root (the dir containing plugin/m4a_engine.c)")

get_filename_component(PORYAAAA_M4L_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../.." ABSOLUTE)
set(PORYAAAA_VOICEGROUP_CORE_DIR "${PORYAAAA_M4L_ROOT}/../voicegroup-core"
    CACHE PATH "Path to the voicegroup-core package root")
set(PORYAAAA_SANE_CPP_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/SaneCppLibraries")
set(PORYAAAA_VOICEGROUP_CORE_STATICLIB
    "${PORYAAAA_VOICEGROUP_CORE_DIR}/target/release/libvoicegroup_core.a")
if(APPLE)
    if(CMAKE_OSX_ARCHITECTURES MATCHES "x86_64" AND CMAKE_OSX_ARCHITECTURES MATCHES "arm64")
        set(PORYAAAA_VOICEGROUP_CORE_STATICLIB
            "${PORYAAAA_VOICEGROUP_CORE_DIR}/target/universal-apple-darwin/release/libvoicegroup_core.a")
    elseif(CMAKE_OSX_ARCHITECTURES MATCHES "x86_64")
        set(PORYAAAA_VOICEGROUP_CORE_STATICLIB
            "${PORYAAAA_VOICEGROUP_CORE_DIR}/target/x86_64-apple-darwin/release/libvoicegroup_core.a")
    else()
        set(PORYAAAA_VOICEGROUP_CORE_STATICLIB
            "${PORYAAAA_VOICEGROUP_CORE_DIR}/target/aarch64-apple-darwin/release/libvoicegroup_core.a")
    endif()
endif()
if(NOT EXISTS "${PORYAAAA_VOICEGROUP_CORE_STATICLIB}")
    message(FATAL_ERROR
        "voicegroup-core static library not found at ${PORYAAAA_VOICEGROUP_CORE_STATICLIB}; "
        "run npm run build:voicegroup-core-static from packages/poryaaaa-m4l")
endif()
add_library(voicegroup_core_static STATIC IMPORTED GLOBAL)
set_target_properties(voicegroup_core_static PROPERTIES
    IMPORTED_LOCATION "${PORYAAAA_VOICEGROUP_CORE_STATICLIB}")

include_directories(
    "${MAX_SDK_INCLUDES}"
    "${MAX_SDK_MSP_INCLUDES}"
    # Wrapper-local sources first so our recorder/ headers shadow the
    # engine repo's stale copy at ${PORYA_ROOT}/plugin/recorder/.
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${PORYA_ROOT}/plugin"
    "${PORYA_ROOT}/plugin/voicegroup"
    "${PORYA_ROOT}/../../shared"
    "${PORYAAAA_VOICEGROUP_CORE_DIR}/include"
    "${PORYAAAA_SANE_CPP_DIR}/Libraries"
)

add_subdirectory("${PORYA_ROOT}/plugin/m4a" "${CMAKE_CURRENT_BINARY_DIR}/poryaaaa_m4a_driver")
add_subdirectory("${PORYA_ROOT}/plugin/hw_audio" "${CMAKE_CURRENT_BINARY_DIR}/poryaaaa_hw_audio")

# Only pick up the wrapper sources here; the engine C sources are listed
# explicitly below so we don't accidentally drag in build-system files.
file(GLOB LOCAL_SRC "*.h" "*.c" "*.cpp")
list(REMOVE_ITEM LOCAL_SRC "${CMAKE_CURRENT_SOURCE_DIR}/m4a_engine_recorder_stubs.cpp")

set(ENGINE_SRC
    "${PORYA_ROOT}/plugin/m4a_engine.c"
)

file(GLOB VG_SRC "${PORYA_ROOT}/plugin/voicegroup/*.c")

set(RECORDER_SRC
    "${CMAKE_CURRENT_SOURCE_DIR}/recorder/midi_buffer.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/recorder/export_capture.cpp"
)

set(FILE_WATCHER_SRC
    "${PORYAAAA_SANE_CPP_DIR}/Libraries/FileSystemWatcher/FileSystemWatcher.cpp"
)

add_library(
    ${PROJECT_NAME}
    MODULE
    ${LOCAL_SRC}
    ${ENGINE_SRC}
    ${VG_SRC}
    ${RECORDER_SRC}
    ${FILE_WATCHER_SRC}
)

set_target_properties(${PROJECT_NAME} PROPERTIES
    C_STANDARD 11
    C_STANDARD_REQUIRED ON
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)

target_link_libraries(${PROJECT_NAME} PRIVATE m4a_driver hw_audio voicegroup_core_static)
if(APPLE)
    target_link_libraries(${PROJECT_NAME} PRIVATE "-framework CoreServices")
endif()

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
    if(APPLE)
        add_executable(
            porya_voicegroup_reload_watcher_tests
            "${CMAKE_CURRENT_SOURCE_DIR}/voicegroup_reload_watcher.cpp"
            ${FILE_WATCHER_SRC}
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/voicegroup_reload_watcher_tests.cpp"
        )
        target_include_directories(
            porya_voicegroup_reload_watcher_tests
            PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}"
        )
        target_compile_features(porya_voicegroup_reload_watcher_tests PRIVATE cxx_std_17)
        target_link_libraries(porya_voicegroup_reload_watcher_tests PRIVATE "-framework CoreServices")
        add_test(NAME porya_voicegroup_reload_watcher_tests COMMAND porya_voicegroup_reload_watcher_tests)
    endif()
endif()
