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
