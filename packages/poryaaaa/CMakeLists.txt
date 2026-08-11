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
# Vendored from the old ccomidi_recorder.clap project.  Pure C++23.
# Engine will own a RecorderCore instance; M4L wrapper consumes it via
# the engine accessor.  See recorder integration plan.md.
add_subdirectory(plugin/recorder)

# ---- v2 audio engine: m4a software driver + hw_audio chip emulation ----
add_subdirectory(plugin/m4a)
add_subdirectory(plugin/hw_audio)

# ---- Porydaw-compatible static engine core ----
add_subdirectory(plugin/porydaw)

# m4a audio engine sources (shared between plugin, renderer, tests).
set(ENGINE_SOURCES
    plugin/m4a_engine.c
)

# ---- CLAP Plugin ----
set(PORYAAAA_FONT_DIR "${PORYAAAA_MONOREPO_ROOT}/shared/assets/fonts/Calamity")

add_library(poryaaaa MODULE
    plugin/m4a_plugin.c
    plugin/m4a_params.c
    plugin/m4a_engine_recorder.cpp
    plugin/m4a_gui_font_assets.cpp
    plugin/voicegroup/project_asset_index.c
    plugin/m4a_gui.cpp
    ${ENGINE_SOURCES}
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
    plugin/m4a_engine_recorder.cpp
    plugin/m4a_gui_font_assets.cpp
    plugin/voicegroup/project_asset_index.c
    plugin/m4a_gui.cpp
    ${ENGINE_SOURCES}
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
    ${ENGINE_SOURCES}
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
    ${ENGINE_SOURCES}
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

# ---- Cycle-domain native audio trace recorder ----
add_executable(poryaaaa_audio_trace
    cmd/poryaaaa_audio_trace.c
)
poryaaaa_enable_project_warnings(poryaaaa_audio_trace)
target_include_directories(poryaaaa_audio_trace PRIVATE plugin)
target_link_libraries(poryaaaa_audio_trace PRIVATE hw_audio)
if(NOT MSVC)
    target_link_libraries(poryaaaa_audio_trace PRIVATE m)
endif()

# ---- Focused native audio trace tests ----
add_executable(poryaaaa_hw_audio_trace_tests
    test/test_hw_audio_trace.c
)
poryaaaa_enable_project_warnings(poryaaaa_hw_audio_trace_tests)
target_compile_definitions(poryaaaa_hw_audio_trace_tests PRIVATE
    PORYAAAA_HW_AUDIO_TRACE_TEST_MAIN=1
)
target_include_directories(poryaaaa_hw_audio_trace_tests PRIVATE plugin test)
target_link_libraries(poryaaaa_hw_audio_trace_tests PRIVATE hw_audio)
if(NOT MSVC)
    target_link_libraries(poryaaaa_hw_audio_trace_tests PRIVATE m)
endif()

# ---- Focused native audio trace contract tests ----
add_executable(poryaaaa_hw_audio_trace_contract_tests
    test/test_hw_audio_trace_contracts.c
)
poryaaaa_enable_project_warnings(poryaaaa_hw_audio_trace_contract_tests)
target_compile_definitions(poryaaaa_hw_audio_trace_contract_tests PRIVATE
    PORYAAAA_HW_AUDIO_TRACE_CONTRACTS_TEST_MAIN=1
)
target_include_directories(poryaaaa_hw_audio_trace_contract_tests PRIVATE plugin test)
target_link_libraries(poryaaaa_hw_audio_trace_contract_tests PRIVATE hw_audio)
if(NOT MSVC)
    target_link_libraries(poryaaaa_hw_audio_trace_contract_tests PRIVATE m)
endif()

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
        add_executable(mgba_audio_trace_replay
            tools/mgba-reference/mgba_audio_trace_replay.c
        )
        set_property(TARGET mgba_audio_trace_replay PROPERTY C_STANDARD 11)
        set_property(TARGET mgba_audio_trace_replay PROPERTY C_STANDARD_REQUIRED ON)
        poryaaaa_enable_project_warnings(mgba_audio_trace_replay)
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

        # ---- Direct pinned mGBA frontend parity harness ----
        add_executable(poryaaaa_frontend_parity_tests
            test/test_hw_resample_parity.c
        )
        set_property(TARGET poryaaaa_frontend_parity_tests PROPERTY C_STANDARD 11)
        set_property(TARGET poryaaaa_frontend_parity_tests PROPERTY C_STANDARD_REQUIRED ON)
        poryaaaa_enable_project_warnings(poryaaaa_frontend_parity_tests)
        target_include_directories(poryaaaa_frontend_parity_tests PRIVATE
            plugin
            test
            "${_mgba_source}/include")
        target_compile_definitions(poryaaaa_frontend_parity_tests PRIVATE
            "$<TARGET_PROPERTY:mgba,COMPILE_DEFINITIONS>")
        target_link_libraries(poryaaaa_frontend_parity_tests PRIVATE hw_audio mgba)
        if(NOT MSVC)
            target_link_libraries(poryaaaa_frontend_parity_tests PRIVATE m)
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
    test/test_gui_assets.cpp
    test/test_hw_mix.c
    test/test_hw_audio_trace.c
    test/test_hw_audio_trace_contracts.c
    test/test_recorder_core.cpp
    test/test_voicegroup_loader.c
    ${PORYAAAA_VOICEGROUP_CORE_PARITY_SOURCES}
    plugin/m4a_engine_recorder.cpp
    plugin/m4a_gui_font_assets.cpp
    ${ENGINE_SOURCES}
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
    plugin/m4a_engine_recorder.cpp
    ${ENGINE_SOURCES}
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

# ---- v2 link wiring ----
set(_PORYAAAA_V2_TARGETS poryaaaa poryaaaa_test poryaaaa_render
                         poryaaaa_unit_tests poryaaaa_engine_lifecycle)

foreach(_tgt IN LISTS _PORYAAAA_V2_TARGETS)
    target_link_libraries(${_tgt} PRIVATE m4a_driver hw_audio)
endforeach()
