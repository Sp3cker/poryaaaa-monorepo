cmake_minimum_required(VERSION 3.24)
project(poryaaaa C CXX)
if(APPLE)
    enable_language(OBJC OBJCXX)
endif()

set(CMAKE_C_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED ON)
if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>" CACHE STRING "" FORCE)
    add_compile_options("$<$<COMPILE_LANG_AND_ID:C,MSVC>:/experimental:c11atomics>")
endif()

function(poryaaaa_enable_msvc_c_atomics target_name)
    if(MSVC)
        target_compile_options(${target_name} PRIVATE
            "$<$<COMPILE_LANGUAGE:C>:/experimental:c11atomics>")
    endif()
endfunction()

function(poryaaaa_enable_project_warnings target_name)
    if(MSVC)
        target_compile_definitions(${target_name} PRIVATE
            _CRT_SECURE_NO_WARNINGS
            _CRT_NONSTDC_NO_WARNINGS)
        target_compile_options(${target_name} PRIVATE
            /W4
            /WX
            /MP
            "$<$<COMPILE_LANGUAGE:CXX>:/permissive->"
            "$<$<COMPILE_LANGUAGE:CXX>:/Zc:__cplusplus>"
            "$<$<COMPILE_LANGUAGE:CXX>:/Zc:preprocessor>")
    else()
        target_compile_options(${target_name} PRIVATE
            -Wall
            -Wextra
            -Wpedantic
            -Werror
            -Wshadow
            -Wunused
            -Wformat=2
            -Wnull-dereference
            -Wimplicit-fallthrough
            "$<$<COMPILE_LANGUAGE:CXX>:-Woverloaded-virtual>"
            "$<$<COMPILE_LANGUAGE:CXX>:-Wnon-virtual-dtor>")
    endif()
endfunction()

function(poryaaaa_enable_project_source_warnings)
    if(MSVC)
        set_property(SOURCE ${ARGN} APPEND PROPERTY COMPILE_OPTIONS
            /W4
            /WX
            "$<$<COMPILE_LANGUAGE:CXX>:/permissive->"
            "$<$<COMPILE_LANGUAGE:CXX>:/Zc:__cplusplus>"
            "$<$<COMPILE_LANGUAGE:CXX>:/Zc:preprocessor>")
    else()
        set_property(SOURCE ${ARGN} APPEND PROPERTY COMPILE_OPTIONS
            -Wall
            -Wextra
            -Wpedantic
            -Werror
            -Wshadow
            -Wunused
            -Wformat=2
            -Wnull-dereference
            -Wimplicit-fallthrough
            "$<$<COMPILE_LANGUAGE:CXX>:-Woverloaded-virtual>"
            "$<$<COMPILE_LANGUAGE:CXX>:-Wnon-virtual-dtor>")
    endif()
endfunction()

if(MSVC)
    set_source_files_properties(plugin/m4a_plugin.c plugin/m4a_params.c
        PROPERTIES COMPILE_OPTIONS "/experimental:c11atomics")
endif()

set(PORYAAAA_MONOREPO_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../..")
set(PORYAAAA_THIRD_PARTY_DIR "${PORYAAAA_MONOREPO_ROOT}/third_party")
set(PORYAAAA_SHARED_GUI_DIR "${PORYAAAA_MONOREPO_ROOT}/shared/gui/pugl")
set(PORYAAAA_FILEBROWSER_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui-filebrowser")
set(PORYAAAA_VOICEGROUP_CORE_DIR "${PORYAAAA_MONOREPO_ROOT}/packages/voicegroup-core")
set(PORYAAAA_VOICEGROUP_CORE_STATICLIB "${PORYAAAA_VOICEGROUP_CORE_DIR}/target/release/libvoicegroup_core.a")
if(APPLE)
    set(PORYAAAA_VOICEGROUP_CORE_RUST_TARGET "aarch64-apple-darwin")
    if(CMAKE_OSX_ARCHITECTURES MATCHES "x86_64")
        set(PORYAAAA_VOICEGROUP_CORE_RUST_TARGET "x86_64-apple-darwin")
    endif()
    set(PORYAAAA_VOICEGROUP_CORE_STATICLIB
        "${PORYAAAA_VOICEGROUP_CORE_DIR}/target/${PORYAAAA_VOICEGROUP_CORE_RUST_TARGET}/release/libvoicegroup_core.a")
endif()
if(EXISTS "${PORYAAAA_VOICEGROUP_CORE_STATICLIB}")
    add_library(voicegroup_core_static STATIC IMPORTED GLOBAL)
    set_target_properties(voicegroup_core_static PROPERTIES
        IMPORTED_LOCATION "${PORYAAAA_VOICEGROUP_CORE_STATICLIB}")
endif()


# ---- CLAP SDK ----
set(CLAP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
add_subdirectory("${PORYAAAA_THIRD_PARTY_DIR}/clap" "${CMAKE_CURRENT_BINARY_DIR}/third_party/clap")

# ---- Pugl ----
set(PUGL_DIR "${PORYAAAA_THIRD_PARTY_DIR}/pugl")

# Platform-specific Pugl sources
if(WIN32)
    set(PUGL_PLATFORM_SOURCES
        ${PUGL_DIR}/src/win.c
        ${PUGL_DIR}/src/win_gl.c
        ${PUGL_DIR}/src/win_stub.c
    )
elseif(APPLE)
    set(PUGL_PLATFORM_SOURCES
        ${PUGL_DIR}/src/mac.m
        ${PUGL_DIR}/src/mac_stub.m
        ${PORYAAAA_SHARED_GUI_DIR}/pugl_mac_metal.m
    )
else()
    set(PUGL_PLATFORM_SOURCES
        ${PUGL_DIR}/src/x11.c
        ${PUGL_DIR}/src/x11_gl.c
        ${PUGL_DIR}/src/x11_stub.c
    )
endif()

set(PUGL_SOURCES
    ${PUGL_DIR}/src/common.c
    ${PUGL_DIR}/src/internal.c
    ${PUGL_PLATFORM_SOURCES}
)

# ---- Dear ImGui ----
set(IMGUI_DIR "${PORYAAAA_THIRD_PARTY_DIR}/imgui")
set(IMGUI_SOURCES
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${PORYAAAA_SHARED_GUI_DIR}/imgui_impl_pugl.cpp
    ${PORYAAAA_SHARED_GUI_DIR}/imgui_pugl_shell.cpp
)
if(APPLE)
    list(APPEND IMGUI_SOURCES ${IMGUI_DIR}/backends/imgui_impl_metal.mm)
    set_source_files_properties(${IMGUI_DIR}/backends/imgui_impl_metal.mm
        PROPERTIES COMPILE_OPTIONS -Wno-arc-bridge-casts-disallowed-in-nonarc)
else()
    list(APPEND IMGUI_SOURCES ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp)
endif()

# ---- Voicegroup loader (decomp parser, reusable by external tools) ----
# Pure C, no audio-engine dependencies. Defined in its own
# CMakeLists so external projects (ccomidi etc.) can add_subdirectory
# just the library without dragging in poryaaaa's plugin /
# renderer / test targets.
add_subdirectory(plugin/voicegroup)

# ---- Recorder (MIDI capture + SMF writer) ----
# Pure C++23. The CLAP plugin owns an M4ARecorder; the M4L external has a
# separate capture path. See recorder integration plan.md.
add_subdirectory(plugin/recorder)

# Trace capture/replay is a developer toolchain, never part of the shipping
# plugin, normal renderer, or default unit-test graph.
option(PORYAAAA_BUILD_TRACE_TOOLS "Build audio trace capture and replay tools" OFF)
enable_testing()

# ---- Direct audio runtime libraries: M4ADriver + HwAudio ----
add_subdirectory(plugin/m4a)
add_subdirectory(plugin/hw_audio)

# ---- Porydaw-compatible static engine core ----
add_subdirectory(plugin/porydaw)


# ---- CLAP Plugin ----
set(PORYAAAA_FONT_DIR "${PORYAAAA_MONOREPO_ROOT}/shared/assets/fonts/Calamity")

add_library(poryaaaa MODULE
    plugin/m4a_plugin.c
    plugin/m4a_params.c
    plugin/m4a_recorder.cpp
    plugin/m4a_gui_font_assets.cpp
    plugin/voicegroup/project_asset_index.c
    plugin/m4a_gui.cpp
    ${IMGUI_SOURCES}
    ${PUGL_SOURCES}
)
target_compile_features(poryaaaa PRIVATE cxx_std_23)
poryaaaa_enable_msvc_c_atomics(poryaaaa)
if(MSVC)
    target_compile_definitions(poryaaaa PRIVATE
        _CRT_SECURE_NO_WARNINGS
        _CRT_NONSTDC_NO_WARNINGS)
endif()
poryaaaa_enable_project_source_warnings(
    plugin/m4a_plugin.c
    plugin/m4a_params.c
    plugin/m4a_recorder.cpp
    plugin/m4a_gui_font_assets.cpp
    plugin/voicegroup/project_asset_index.c
    plugin/m4a_gui.cpp
)

target_include_directories(poryaaaa PRIVATE
    plugin
    ${PORYAAAA_SHARED_GUI_DIR}
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
    ${PUGL_DIR}/include
    ${PUGL_DIR}/src
    "${PORYAAAA_VOICEGROUP_CORE_DIR}/include"
)
target_include_directories(poryaaaa SYSTEM PRIVATE
    ${PORYAAAA_FILEBROWSER_DIR}
)

# Build Pugl as a static library (not a DLL) on all platforms
target_compile_definitions(poryaaaa PRIVATE
    PUGL_STATIC
    PORYAAAA_FONT_DIR="${PORYAAAA_FONT_DIR}"
)

# On macOS, compile m4a_gui.cpp as Objective-C++ so it can use Metal APIs
if(APPLE)
    set_source_files_properties(${PORYAAAA_SHARED_GUI_DIR}/imgui_pugl_shell.cpp PROPERTIES LANGUAGE OBJCXX)
    set_source_files_properties(plugin/m4a_gui.cpp PROPERTIES LANGUAGE OBJCXX)
endif()

# Use C++ linker (required because we mix C and C++ sources)
set_target_properties(poryaaaa PROPERTIES
    PREFIX ""
    SUFFIX ".clap"
    LINKER_LANGUAGE CXX
)

target_link_libraries(poryaaaa PRIVATE clap voicegroup_loader recorder)
if(TARGET voicegroup_core_static)
    target_link_libraries(poryaaaa PRIVATE voicegroup_core_static)
endif()


if(UNIX AND NOT APPLE)
    target_link_libraries(poryaaaa PRIVATE m X11 Xcursor Xrandr Xext)
endif()

# Graphics
if(APPLE)
    target_link_libraries(poryaaaa PRIVATE "-framework Metal" "-framework QuartzCore" "-framework Cocoa")
elseif(WIN32)
    target_link_libraries(poryaaaa PRIVATE opengl32)
else()
    target_link_libraries(poryaaaa PRIVATE GL)
endif()

# Platform-specific settings
if(WIN32)
    target_link_libraries(poryaaaa PRIVATE dwmapi)
    if(MINGW)
        target_link_libraries(poryaaaa PRIVATE -static-libgcc -static-libstdc++)
    endif()
elseif(APPLE)
    set_target_properties(poryaaaa PROPERTIES
        BUNDLE TRUE
        BUNDLE_EXTENSION "clap"
    )

    # MACOSX_BUNDLE_INFO_PLIST only works for MACOSX_BUNDLE (app bundles).
    # For a generic BUNDLE (.clap), copy Info.plist manually.
    add_custom_command(TARGET poryaaaa POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/plugin/Info.plist"
            "$<TARGET_BUNDLE_CONTENT_DIR:poryaaaa>/Info.plist"
    )

    set(PORYAAAA_CLAP_INSTALL_DIR "$ENV{HOME}/Library/Audio/Plug-Ins/CLAP")
    add_custom_command(TARGET poryaaaa POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "${PORYAAAA_CLAP_INSTALL_DIR}"
        COMMAND ${CMAKE_COMMAND} -E rm -rf
            "${PORYAAAA_CLAP_INSTALL_DIR}/poryaaaa.clap"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "$<TARGET_BUNDLE_DIR:poryaaaa>"
            "${PORYAAAA_CLAP_INSTALL_DIR}/poryaaaa.clap"
        COMMENT "Installing poryaaaa.clap to ${PORYAAAA_CLAP_INSTALL_DIR}"
    )
endif()

# ---- WAV Export Test ----
add_executable(poryaaaa_test
    test/test_wav_export.c
)
poryaaaa_enable_msvc_c_atomics(poryaaaa_test)
poryaaaa_enable_project_warnings(poryaaaa_test)

target_include_directories(poryaaaa_test PRIVATE
    plugin
)

target_link_libraries(poryaaaa_test PRIVATE voicegroup_loader)
if(NOT MSVC)
    target_link_libraries(poryaaaa_test PRIVATE m)
endif()

# ---- CLI Renderer ----
add_executable(poryaaaa_render
    cmd/poryaaaa_render.c
)
poryaaaa_enable_msvc_c_atomics(poryaaaa_render)
poryaaaa_enable_project_warnings(poryaaaa_render)
target_include_directories(poryaaaa_render PRIVATE plugin third_party)
target_link_libraries(poryaaaa_render PRIVATE voicegroup_loader)
if(NOT MSVC)
    target_link_libraries(poryaaaa_render PRIVATE m)
endif()
if(UNIX AND NOT APPLE)
    target_link_libraries(poryaaaa_render PRIVATE pthread dl)
endif()
if(APPLE)
    target_link_libraries(poryaaaa_render PRIVATE "-framework CoreAudio" "-framework AudioToolbox" "-framework CoreFoundation")
endif()
if(WIN32)
    if(MINGW)
        target_link_libraries(poryaaaa_render PRIVATE -static-libgcc)
    endif()
endif()

if(PORYAAAA_BUILD_TRACE_TOOLS)
    # ---- Cycle-domain native audio trace recorder ----
    add_executable(poryaaaa_audio_trace
        cmd/poryaaaa_audio_trace.c
    )
    poryaaaa_enable_project_warnings(poryaaaa_audio_trace)
    target_include_directories(poryaaaa_audio_trace PRIVATE plugin)
    target_link_libraries(poryaaaa_audio_trace PRIVATE poryaaaa_hw_audio_trace_tool)
    if(NOT MSVC)
        target_link_libraries(poryaaaa_audio_trace PRIVATE m)
    endif()

    # ---- Driver lifecycle candidate trace ----
    # Loads one supported voicegroup slot, derives its hardware family from
    # ToneData, drives one fixed lifecycle, and serializes its pending bus events.
    add_executable(poryaaaa_driver_trace
        cmd/poryaaaa_driver_trace.c
    )
    poryaaaa_enable_project_warnings(poryaaaa_driver_trace)
    target_include_directories(poryaaaa_driver_trace PRIVATE plugin)
    target_link_libraries(poryaaaa_driver_trace PRIVATE m4a_driver poryaaaa_m4a_trace_writer voicegroup_loader)
    if(NOT MSVC)
        target_link_libraries(poryaaaa_driver_trace PRIVATE m)
    endif()


    # ---- Retained trace-tool smoke ----
    add_executable(poryaaaa_audio_trace_tool_tests
        test/test_hw_audio_trace.c
    )
    poryaaaa_enable_project_warnings(poryaaaa_audio_trace_tool_tests)
    target_compile_definitions(poryaaaa_audio_trace_tool_tests PRIVATE
        PORYAAAA_HW_AUDIO_TRACE_TEST_MAIN=1
    )
    target_include_directories(poryaaaa_audio_trace_tool_tests PRIVATE plugin test)
    target_link_libraries(poryaaaa_audio_trace_tool_tests PRIVATE poryaaaa_hw_audio_trace_tool)
    if(NOT MSVC)
        target_link_libraries(poryaaaa_audio_trace_tool_tests PRIVATE m)
    endif()
endif()

# ---- Independent installed mGBA 0.10.5 frontend reference ----
option(PORYAAAA_BUILD_FRONTEND_PARITY_TESTS
    "Build exact mGBA 0.10.5 blip_buf frontend parity tests" OFF)
set(PORYAAAA_MGBA_0105_SOURCE "" CACHE PATH
    "Untouched mGBA 0.10.5 source checkout for frontend parity")
set(PORYAAAA_MGBA_0105_EXPECTED_REVISION
    "26b7884bc25a5933960f3cdcd98bac1ae14d42e2" CACHE STRING
    "Pinned mGBA 0.10.5 revision")


# ---- Full mGBA MP2K reference recorder ----
option(PORYAAAA_BUILD_MGBA_REFERENCE
    "Build the full-ROM mGBA reference recorder" OFF)
set(PORYAAAA_MGBA_SOURCE "" CACHE PATH
    "Authoritative patched mGBA afd6f14eaf8bd35214ed3fb9dc69a92bfc3877a9 source checkout")
set(PORYAAAA_MGBA_ROOT "" CACHE PATH
    "Non-authoritative installed mGBA prefix for the reference recorder")
set(PORYAAAA_MGBA_EXPECTED_REVISION "afd6f14eaf8bd35214ed3fb9dc69a92bfc3877a9")

if(PORYAAAA_BUILD_MGBA_REFERENCE)
    add_executable(mgba_mp2k_reference
        tools/mgba-reference/mgba_mp2k_reference.c
    )
    set_property(TARGET mgba_mp2k_reference PROPERTY C_STANDARD 11)
    set_property(TARGET mgba_mp2k_reference PROPERTY C_STANDARD_REQUIRED ON)
    target_include_directories(mgba_mp2k_reference PRIVATE plugin)
    poryaaaa_enable_project_warnings(mgba_mp2k_reference)
    get_target_property(_mgba_recorder_compile_options mgba_mp2k_reference COMPILE_OPTIONS)
    if(_mgba_recorder_compile_options STREQUAL "_mgba_recorder_compile_options-NOTFOUND")
        set(_mgba_recorder_compile_options)
    endif()
    string(JOIN " " _mgba_recorder_compiler_flags
        "${CMAKE_C_FLAGS}" ${_mgba_recorder_compile_options})
    string(REPLACE ";" " " _mgba_recorder_compiler_flags "${_mgba_recorder_compiler_flags}")
    string(REPLACE "\"" "\\\"" _mgba_recorder_compiler_flags "${_mgba_recorder_compiler_flags}")
    target_compile_definitions(mgba_mp2k_reference PRIVATE
        "PORYAAAA_MGBA_RECORDER_COMPILER=\"${CMAKE_C_COMPILER_ID} ${CMAKE_C_COMPILER_VERSION} ${CMAKE_C_COMPILER}\""
        "PORYAAAA_MGBA_RECORDER_COMPILE_FLAGS=\"${_mgba_recorder_compiler_flags}\"")
    target_compile_definitions(mgba_mp2k_reference PRIVATE ENABLE_VFS=1)
    if(PORYAAAA_MGBA_SOURCE)
        get_filename_component(_mgba_source "${PORYAAAA_MGBA_SOURCE}" REALPATH)
        if(NOT EXISTS "${_mgba_source}/CMakeLists.txt" OR NOT EXISTS "${_mgba_source}/.git")
            message(FATAL_ERROR
                "PORYAAAA_MGBA_SOURCE must be the pinned mGBA git worktree, not an installation")
        endif()
        find_package(Git REQUIRED)
        execute_process(
            COMMAND "${GIT_EXECUTABLE}" -C "${_mgba_source}" rev-parse HEAD
            RESULT_VARIABLE _mgba_revision_status
            OUTPUT_VARIABLE _mgba_revision
            ERROR_VARIABLE _mgba_revision_error
            OUTPUT_STRIP_TRAILING_WHITESPACE)
        if(NOT _mgba_revision_status STREQUAL "0" OR
           NOT _mgba_revision STREQUAL PORYAAAA_MGBA_EXPECTED_REVISION)
            message(FATAL_ERROR
                "PORYAAAA_MGBA_SOURCE must be at ${PORYAAAA_MGBA_EXPECTED_REVISION}; "
                "got '${_mgba_revision}' (${_mgba_revision_error})")
        endif()

        execute_process(
            COMMAND "${GIT_EXECUTABLE}" -C "${_mgba_source}" diff --name-only
                    "${PORYAAAA_MGBA_EXPECTED_REVISION}"
            RESULT_VARIABLE _mgba_diff_status
            OUTPUT_VARIABLE _mgba_changed_paths
            ERROR_VARIABLE _mgba_diff_error
            OUTPUT_STRIP_TRAILING_WHITESPACE)
        if(NOT _mgba_diff_status STREQUAL "0")
            message(FATAL_ERROR "Could not inspect PORYAAAA_MGBA_SOURCE: ${_mgba_diff_error}")
        endif()
        set(_mgba_observation_patch_paths
            "include/mgba/internal/gba/audio.h"
            "src/gba/audio.c"
            "src/gba/io.c")
        set(_mgba_source_dirty 0)
        if(_mgba_changed_paths)
            set(_mgba_source_dirty 1)
            string(REPLACE "\n" ";" _mgba_changed_path_list "${_mgba_changed_paths}")
            foreach(_mgba_changed_path IN LISTS _mgba_changed_path_list)
                list(FIND _mgba_observation_patch_paths "${_mgba_changed_path}" _mgba_known_patch_path)
                if(_mgba_known_patch_path EQUAL -1)
                    message(FATAL_ERROR
                        "PORYAAAA_MGBA_SOURCE has an unexpected change: ${_mgba_changed_path}. "
                        "Only the observation patch may differ from ${PORYAAAA_MGBA_EXPECTED_REVISION}.")
                endif()
            endforeach()
        endif()
        execute_process(
            COMMAND "${GIT_EXECUTABLE}" -C "${_mgba_source}" ls-files --others --exclude-standard
            RESULT_VARIABLE _mgba_untracked_status
            OUTPUT_VARIABLE _mgba_untracked_paths
            ERROR_VARIABLE _mgba_untracked_error
            OUTPUT_STRIP_TRAILING_WHITESPACE)
        if(NOT _mgba_untracked_status STREQUAL "0" OR _mgba_untracked_paths)
            message(FATAL_ERROR
                "PORYAAAA_MGBA_SOURCE has unexpected untracked files: "
                "${_mgba_untracked_paths}${_mgba_untracked_error}")
        endif()

        execute_process(
            COMMAND "${GIT_EXECUTABLE}" -C "${_mgba_source}" diff --binary
                    "${PORYAAAA_MGBA_EXPECTED_REVISION}"
            RESULT_VARIABLE _mgba_patch_status
            OUTPUT_VARIABLE _mgba_observation_patch
            ERROR_VARIABLE _mgba_patch_error)
        if(NOT _mgba_patch_status STREQUAL "0")
            message(FATAL_ERROR "Could not hash mGBA observation patch: ${_mgba_patch_error}")
        endif()
        string(SHA256 _mgba_observation_patch_sha256 "${_mgba_observation_patch}")

        set(LIBMGBA_ONLY ON CACHE BOOL "" FORCE)
        set(M_CORE_GBA ON CACHE BOOL "" FORCE)
        set(M_CORE_GB OFF CACHE BOOL "" FORCE)
        # mTimingGlobalTime is a true absolute clock only in this pinned mGBA
        # configuration when its existing debugger timing counter is enabled.
        set(ENABLE_DEBUGGERS ON CACHE BOOL "" FORCE)
        set(USE_EDITLINE OFF CACHE BOOL "" FORCE)
        add_subdirectory("${_mgba_source}" "${CMAKE_CURRENT_BINARY_DIR}/mgba-reference" EXCLUDE_FROM_ALL)
        if(NOT TARGET mgba)
            message(FATAL_ERROR "Pinned mGBA source did not define the mgba library target")
        endif()
        if(PORYAAAA_BUILD_TRACE_TOOLS)
            add_executable(mgba_audio_trace_replay
                tools/mgba-reference/mgba_audio_trace_replay.c
            )
            set_property(TARGET mgba_audio_trace_replay PROPERTY C_STANDARD 11)
            set_property(TARGET mgba_audio_trace_replay PROPERTY C_STANDARD_REQUIRED ON)
            poryaaaa_enable_project_warnings(mgba_audio_trace_replay)
            target_include_directories(mgba_audio_trace_replay PRIVATE plugin)
            get_target_property(_mgba_replay_compile_options mgba_audio_trace_replay COMPILE_OPTIONS)
            if(_mgba_replay_compile_options STREQUAL "_mgba_replay_compile_options-NOTFOUND")
                set(_mgba_replay_compile_options)
            endif()
            string(JOIN " " _mgba_replay_compiler_flags
                "${CMAKE_C_FLAGS}" ${_mgba_replay_compile_options})
            string(REPLACE ";" " " _mgba_replay_compiler_flags "${_mgba_replay_compiler_flags}")
            string(REPLACE "\"" "\\\"" _mgba_replay_compiler_flags "${_mgba_replay_compiler_flags}")
            target_compile_definitions(mgba_audio_trace_replay PRIVATE ENABLE_VFS=1)
            target_link_libraries(mgba_audio_trace_replay PRIVATE mgba)
            # The replay adapter reads internal mGBA state, so its feature layout
            # must match the linked mGBA target exactly.
            target_compile_definitions(mgba_audio_trace_replay PRIVATE
                "$<TARGET_PROPERTY:mgba,COMPILE_DEFINITIONS>"
                "PORYAAAA_MGBA_REPLAY_COMPILER=\"${CMAKE_C_COMPILER_ID} ${CMAKE_C_COMPILER_VERSION} ${CMAKE_C_COMPILER}\""
                "PORYAAAA_MGBA_REPLAY_COMPILE_FLAGS=\"${_mgba_replay_compiler_flags}\""
                PORYAAAA_MGBA_BASE_REVISION="${PORYAAAA_MGBA_EXPECTED_REVISION}"
                PORYAAAA_MGBA_OBSERVATION_PATCH_SHA256="${_mgba_observation_patch_sha256}"
                PORYAAAA_MGBA_SOURCE_DIRTY=${_mgba_source_dirty}
                PORYAAAA_MGBA_NATIVE_CAPTURE_AVAILABLE=1
                PORYAAAA_MGBA_SOURCE_POLICY="authoritative-pinned-source")
            if(NOT MSVC)
                target_link_libraries(mgba_audio_trace_replay PRIVATE m)
            endif()
        endif()

        target_link_libraries(mgba_mp2k_reference PRIVATE mgba)
        # mCore's public layout depends on mGBA's feature definitions. Compile the
        # recorder with the library's exact definitions to preserve its ABI.
        target_compile_definitions(mgba_mp2k_reference PRIVATE
            "$<TARGET_PROPERTY:mgba,COMPILE_DEFINITIONS>")
        target_compile_definitions(mgba_mp2k_reference PRIVATE
            PORYAAAA_MGBA_BASE_REVISION="${PORYAAAA_MGBA_EXPECTED_REVISION}"
            PORYAAAA_MGBA_OBSERVATION_PATCH_SHA256="${_mgba_observation_patch_sha256}"
            PORYAAAA_MGBA_SOURCE_DIRTY=${_mgba_source_dirty}
            PORYAAAA_MGBA_NATIVE_CAPTURE_AVAILABLE=1
            PORYAAAA_MGBA_SOURCE_POLICY="authoritative-pinned-source")
    else()
        message(WARNING
            "PORYAAAA_MGBA_ROOT installed-library mode is non-authoritative and frontend-only. "
            "Native parity capture requires PORYAAAA_MGBA_SOURCE.")
        find_path(PORYAAAA_MGBA_INCLUDE_DIR mgba/core/core.h
            HINTS
                "${PORYAAAA_MGBA_ROOT}/include"
                /usr/local/include
                /opt/homebrew/include)
        find_library(PORYAAAA_MGBA_LIBRARY mgba
            HINTS
                "${PORYAAAA_MGBA_ROOT}/lib"
                /usr/local/lib
                /opt/homebrew/lib)
        if(NOT PORYAAAA_MGBA_INCLUDE_DIR OR NOT PORYAAAA_MGBA_LIBRARY)
            message(FATAL_ERROR
                "PORYAAAA_BUILD_MGBA_REFERENCE requires PORYAAAA_MGBA_SOURCE or an mGBA development install")
        endif()
        target_include_directories(mgba_mp2k_reference PRIVATE "${PORYAAAA_MGBA_INCLUDE_DIR}")
        target_link_libraries(mgba_mp2k_reference PRIVATE "${PORYAAAA_MGBA_LIBRARY}")
        target_compile_definitions(mgba_mp2k_reference PRIVATE
            PORYAAAA_MGBA_BASE_REVISION="unverified-installed-library"
            PORYAAAA_MGBA_OBSERVATION_PATCH_SHA256="unverified-installed-library"
            PORYAAAA_MGBA_SOURCE_DIRTY=0
            PORYAAAA_MGBA_NATIVE_CAPTURE_AVAILABLE=0
            PORYAAAA_MGBA_SOURCE_POLICY="non-authoritative-installed-library")
    endif()

    if(NOT MSVC)
        target_link_libraries(mgba_mp2k_reference PRIVATE m)
    endif()
endif()

if(PORYAAAA_BUILD_FRONTEND_PARITY_TESTS)
    if(NOT PORYAAAA_MGBA_0105_SOURCE)
        message(FATAL_ERROR
            "PORYAAAA_BUILD_FRONTEND_PARITY_TESTS requires PORYAAAA_MGBA_0105_SOURCE")
    endif()

    get_filename_component(_mgba0105_source "${PORYAAAA_MGBA_0105_SOURCE}" REALPATH)
    if(NOT EXISTS "${_mgba0105_source}/CMakeLists.txt" OR
       NOT EXISTS "${_mgba0105_source}/.git")
        message(FATAL_ERROR
            "PORYAAAA_MGBA_0105_SOURCE must be an untouched mGBA 0.10.5 git checkout")
    endif()
    if(NOT EXISTS "${_mgba0105_source}/src/third-party/blip_buf/blip_buf.c" OR
       NOT EXISTS "${_mgba0105_source}/include/mgba/core/blip_buf.h")
        message(FATAL_ERROR
            "PORYAAAA_MGBA_0105_SOURCE is missing mGBA 0.10.5 blip_buf")
    endif()

    find_package(Git REQUIRED)
    execute_process(
        COMMAND "${GIT_EXECUTABLE}" -C "${_mgba0105_source}" rev-parse HEAD
        RESULT_VARIABLE _mgba0105_revision_status
        OUTPUT_VARIABLE _mgba0105_revision
        ERROR_VARIABLE _mgba0105_revision_error
        OUTPUT_STRIP_TRAILING_WHITESPACE)
    if(NOT "${_mgba0105_revision_status}" STREQUAL "0" OR
       NOT "${_mgba0105_revision}" STREQUAL "${PORYAAAA_MGBA_0105_EXPECTED_REVISION}")
        message(FATAL_ERROR
            "PORYAAAA_MGBA_0105_SOURCE must be at ${PORYAAAA_MGBA_0105_EXPECTED_REVISION}; "
            "got '${_mgba0105_revision}' (${_mgba0105_revision_error})")
    endif()

    execute_process(
        COMMAND "${GIT_EXECUTABLE}" -C "${_mgba0105_source}" diff --quiet HEAD --
        RESULT_VARIABLE _mgba0105_dirty_status
        ERROR_VARIABLE _mgba0105_dirty_error)
    if(NOT "${_mgba0105_dirty_status}" STREQUAL "0")
        message(FATAL_ERROR
            "PORYAAAA_MGBA_0105_SOURCE must be clean: ${_mgba0105_dirty_error}")
    endif()
    execute_process(
        COMMAND "${GIT_EXECUTABLE}" -C "${_mgba0105_source}" ls-files --others --exclude-standard
        RESULT_VARIABLE _mgba0105_untracked_status
        OUTPUT_VARIABLE _mgba0105_untracked_paths
        ERROR_VARIABLE _mgba0105_untracked_error
        OUTPUT_STRIP_TRAILING_WHITESPACE)
    if(NOT "${_mgba0105_untracked_status}" STREQUAL "0" OR _mgba0105_untracked_paths)
        message(FATAL_ERROR
            "PORYAAAA_MGBA_0105_SOURCE must have no untracked paths: "
            "${_mgba0105_untracked_paths}${_mgba0105_untracked_error}")
    endif()

    # Compile untouched upstream code without project warning/Werror settings.
    # These private renames keep every mGBA blip symbol out of production targets.
    add_library(poryaaaa_mgba0105_blip_reference STATIC
        "${_mgba0105_source}/src/third-party/blip_buf/blip_buf.c")
    set_property(TARGET poryaaaa_mgba0105_blip_reference PROPERTY C_STANDARD 11)
    set_property(TARGET poryaaaa_mgba0105_blip_reference PROPERTY C_STANDARD_REQUIRED ON)
    target_include_directories(poryaaaa_mgba0105_blip_reference PRIVATE
        "${_mgba0105_source}/include")
    target_compile_definitions(poryaaaa_mgba0105_blip_reference PRIVATE
        blip_new=poryaaaa_mgba0105_blip_new
        blip_set_rates=poryaaaa_mgba0105_blip_set_rates
        blip_clear=poryaaaa_mgba0105_blip_clear
        blip_add_delta=poryaaaa_mgba0105_blip_add_delta
        blip_add_delta_fast=poryaaaa_mgba0105_blip_add_delta_fast
        blip_clocks_needed=poryaaaa_mgba0105_blip_clocks_needed
        blip_end_frame=poryaaaa_mgba0105_blip_end_frame
        blip_samples_avail=poryaaaa_mgba0105_blip_samples_avail
        blip_read_samples=poryaaaa_mgba0105_blip_read_samples
        blip_delete=poryaaaa_mgba0105_blip_delete)

    add_executable(poryaaaa_frontend_parity_tests
        test/test_hw_resample_parity.c)
    set_property(TARGET poryaaaa_frontend_parity_tests PROPERTY C_STANDARD 11)
    set_property(TARGET poryaaaa_frontend_parity_tests PROPERTY C_STANDARD_REQUIRED ON)
    poryaaaa_enable_msvc_c_atomics(poryaaaa_frontend_parity_tests)
    poryaaaa_enable_project_warnings(poryaaaa_frontend_parity_tests)
    target_include_directories(poryaaaa_frontend_parity_tests PRIVATE plugin test)
    target_link_libraries(poryaaaa_frontend_parity_tests PRIVATE
        hw_audio
        poryaaaa_mgba0105_blip_reference)
    if(NOT MSVC)
        target_link_libraries(poryaaaa_frontend_parity_tests PRIVATE m)
    endif()
    add_test(NAME poryaaaa_frontend_parity_tests COMMAND poryaaaa_frontend_parity_tests)
endif()

# ---- Headless VBA-M libretro reference recorder ----
option(PORYAAAA_BUILD_VBAM_REFERENCE
    "Build the headless VBA-M libretro reference recorder" OFF)
set(PORYAAAA_VBAM_SOURCE "" CACHE PATH
    "VBA-M source checkout containing src/libretro/libretro-common/include")

if(PORYAAAA_BUILD_VBAM_REFERENCE)
    find_path(PORYAAAA_LIBRETRO_INCLUDE_DIR libretro.h
        HINTS "${PORYAAAA_VBAM_SOURCE}/src/libretro/libretro-common/include")
    if(NOT PORYAAAA_LIBRETRO_INCLUDE_DIR)
        message(FATAL_ERROR
            "PORYAAAA_BUILD_VBAM_REFERENCE requires a VBA-M source checkout; "
            "set PORYAAAA_VBAM_SOURCE")
    endif()

    add_executable(vbam_mp2k_reference
        tools/vbam-reference/vbam_mp2k_reference.c
    )
    poryaaaa_enable_project_warnings(vbam_mp2k_reference)
    target_include_directories(vbam_mp2k_reference PRIVATE
        "${PORYAAAA_LIBRETRO_INCLUDE_DIR}"
    )
    if(APPLE)
        target_link_libraries(vbam_mp2k_reference PRIVATE dl)
    elseif(UNIX)
        target_link_libraries(vbam_mp2k_reference PRIVATE dl m)
    endif()
endif()

# ---- Audio Engine Benchmark ----
add_executable(poryaaaa_audio_bench
    bench/audio_engine_bench.c
)
poryaaaa_enable_msvc_c_atomics(poryaaaa_audio_bench)
poryaaaa_enable_project_warnings(poryaaaa_audio_bench)
target_link_libraries(poryaaaa_audio_bench PRIVATE m4a_driver hw_audio)
if(NOT MSVC)
    target_link_libraries(poryaaaa_audio_bench PRIVATE m)
endif()

# ---- Node N-API voicegroup parser ----
find_path(NODE_API_INCLUDE_DIR node_api.h
    HINTS /usr/local/include/node /opt/homebrew/include/node)
if(NODE_API_INCLUDE_DIR AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/napi/poryaaaa_voicegroup_napi.c")
    add_library(poryaaaa_napi MODULE
        napi/poryaaaa_voicegroup_napi.c
    )
    poryaaaa_enable_project_warnings(poryaaaa_napi)
    target_include_directories(poryaaaa_napi PRIVATE
        plugin
        ${NODE_API_INCLUDE_DIR}
    )
    target_link_libraries(poryaaaa_napi PRIVATE voicegroup_loader)
    set_target_properties(poryaaaa_napi PROPERTIES
        PREFIX ""
        OUTPUT_NAME "poryaaaa_voicegroup"
        SUFFIX ".node"
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/../poryaaaa-m4l/javascript"
    )
    if(APPLE)
        target_link_options(poryaaaa_napi PRIVATE "-undefined" "dynamic_lookup")
    endif()
endif()

# ---- Unit Tests ----
set(PORYAAAA_VOICEGROUP_CORE_PARITY_SOURCES)
if(TARGET voicegroup_core_static)
    set(PORYAAAA_VOICEGROUP_CORE_PARITY_SOURCES test/test_voicegroup_core_parity.c)
endif()

add_executable(poryaaaa_unit_tests
    test/test_engine.c
    test/test_m4a_v2.c
    test/test_porydaw_engine.c
    plugin/m4a_engine.c
    test/test_gui_assets.cpp
    test/test_hw_mix.c
    test/test_recorder_core.cpp
    test/test_voicegroup_loader.c
    test/test_hw_audio_contracts.c
    ${PORYAAAA_VOICEGROUP_CORE_PARITY_SOURCES}
    plugin/m4a_recorder.cpp
    plugin/m4a_gui_font_assets.cpp
)
target_compile_features(poryaaaa_unit_tests PRIVATE cxx_std_23)
poryaaaa_enable_msvc_c_atomics(poryaaaa_unit_tests)
poryaaaa_enable_project_warnings(poryaaaa_unit_tests)
target_compile_definitions(poryaaaa_unit_tests PRIVATE
    PORYAAAA_FONT_DIR="${PORYAAAA_FONT_DIR}"
)

target_include_directories(poryaaaa_unit_tests PRIVATE
    plugin
)

target_link_libraries(poryaaaa_unit_tests PRIVATE voicegroup_loader recorder)
if(TARGET voicegroup_core_static)
    target_compile_definitions(poryaaaa_unit_tests PRIVATE PORYAAAA_HAS_VOICEGROUP_CORE_PARITY=1)
    target_include_directories(poryaaaa_unit_tests PRIVATE "${PORYAAAA_VOICEGROUP_CORE_DIR}/include")
    target_link_libraries(poryaaaa_unit_tests PRIVATE voicegroup_core_static)
endif()
if(NOT MSVC)
    target_link_libraries(poryaaaa_unit_tests PRIVATE m)
endif()

# ---- Engine lifecycle/leak harness ----
add_executable(poryaaaa_engine_lifecycle
    test/test_engine_lifecycle.c
    plugin/m4a_recorder.cpp
)
target_compile_features(poryaaaa_engine_lifecycle PRIVATE cxx_std_23)
poryaaaa_enable_msvc_c_atomics(poryaaaa_engine_lifecycle)
poryaaaa_enable_project_warnings(poryaaaa_engine_lifecycle)

target_include_directories(poryaaaa_engine_lifecycle PRIVATE
    plugin
)

target_link_libraries(poryaaaa_engine_lifecycle PRIVATE voicegroup_loader recorder)
if(NOT MSVC)
    target_link_libraries(poryaaaa_engine_lifecycle PRIVATE m)
endif()

# ---- Direct runtime link wiring: M4ADriver + HwAudio ----
set(_PORYAAAA_V2_TARGETS poryaaaa poryaaaa_test poryaaaa_render
                         poryaaaa_unit_tests poryaaaa_engine_lifecycle)

foreach(_tgt IN LISTS _PORYAAAA_V2_TARGETS)
    target_link_libraries(${_tgt} PRIVATE m4a_driver hw_audio)
endforeach()
# These product and test runtimes link the direct driver/chip pair, never a
# public engine wrapper. Its canonical render sequence is documented in
# plugin/hw_audio/hw_audio.h.

# Link-graph gates keep shipping targets free of trace parser, scheduler,
# replay, writer, and trace-runtime objects. They inspect CMake's resolved
# target properties rather than requiring a built artifact.
file(GENERATE
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/poryaaaa_default_trace_link_graph.cmake"
    CONTENT [=[
set(_default_links
    "$<TARGET_PROPERTY:poryaaaa,LINK_LIBRARIES>"
    "$<TARGET_PROPERTY:poryaaaa_render,LINK_LIBRARIES>"
    "$<TARGET_PROPERTY:poryaaaa_unit_tests,LINK_LIBRARIES>"
    "$<TARGET_PROPERTY:hw_audio,LINK_LIBRARIES>"
    "$<TARGET_PROPERTY:m4a_driver,LINK_LIBRARIES>")
set(_default_sources
    "$<TARGET_PROPERTY:poryaaaa,SOURCES>"
    "$<TARGET_PROPERTY:poryaaaa_render,SOURCES>"
    "$<TARGET_PROPERTY:poryaaaa_unit_tests,SOURCES>"
    "$<TARGET_PROPERTY:hw_audio,SOURCES>"
    "$<TARGET_PROPERTY:m4a_driver,SOURCES>")
foreach(_surface IN LISTS _default_links _default_sources)
    if(_surface MATCHES "poryaaaa_hw_audio_trace_tool|poryaaaa_m4a_trace_writer|hw_audio_trace_(text|schedule)\\.c|m4a_driver_trace\\.c|poryaaaa_audio_trace|poryaaaa_driver_trace|mgba_audio_trace_replay|poryaaaa_mgba0105_blip_reference|mgba_mp2k_reference|(^|;)mgba(;|$)|src/third-party/blip_buf/blip_buf\\.c|tools/mgba-reference/|atomic_publish|driver_compare|native_compare|waveform_compare")
        message(FATAL_ERROR "Default target graph contains trace or opt-in mGBA reference tooling: ${_surface}")
    endif()
endforeach()
]=])
add_test(NAME poryaaaa_default_trace_link_graph
    COMMAND "${CMAKE_COMMAND}" -P "${CMAKE_CURRENT_BINARY_DIR}/poryaaaa_default_trace_link_graph.cmake")

if(PORYAAAA_BUILD_TRACE_TOOLS)
    file(GENERATE
        OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/poryaaaa_opt_in_trace_link_graph.cmake"
        CONTENT [=[
set(_audio_trace_links "$<TARGET_PROPERTY:poryaaaa_audio_trace,LINK_LIBRARIES>")
set(_driver_trace_links "$<TARGET_PROPERTY:poryaaaa_driver_trace,LINK_LIBRARIES>")
set(_trace_test_links "$<TARGET_PROPERTY:poryaaaa_audio_trace_tool_tests,LINK_LIBRARIES>")
if(NOT _audio_trace_links MATCHES "(^|;)poryaaaa_hw_audio_trace_tool($|;)" OR
   _audio_trace_links MATCHES "(^|;)hw_audio($|;)")
    message(FATAL_ERROR "poryaaaa_audio_trace must link only the trace-enabled HwAudio variant")
endif()
if(NOT _driver_trace_links MATCHES "(^|;)poryaaaa_m4a_trace_writer($|;)")
    message(FATAL_ERROR "poryaaaa_driver_trace must link poryaaaa_m4a_trace_writer")
endif()
if(NOT _trace_test_links MATCHES "(^|;)poryaaaa_hw_audio_trace_tool($|;)" OR
   _trace_test_links MATCHES "(^|;)hw_audio($|;)")
    message(FATAL_ERROR "poryaaaa_audio_trace_tool_tests must link only the trace-enabled HwAudio variant")
endif()
foreach(_target_links IN ITEMS _audio_trace_links _driver_trace_links _trace_test_links)
    if("${${_target_links}}" MATCHES "(^|;)hw_audio(;|$)" AND
       "${${_target_links}}" MATCHES "(^|;)poryaaaa_hw_audio_trace_tool(;|$)")
        message(FATAL_ERROR "A trace tool links both HwAudio variants: ${${_target_links}}")
    endif()
endforeach()
]=])
    add_test(NAME poryaaaa_opt_in_trace_link_graph
        COMMAND "${CMAKE_COMMAND}" -P "${CMAKE_CURRENT_BINARY_DIR}/poryaaaa_opt_in_trace_link_graph.cmake")
endif()
