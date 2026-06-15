cmake_minimum_required(VERSION 3.24)
project(textedit VERSION 0.1.0 LANGUAGES C CXX)
enable_testing()

# Standalone JUCE VST3 utility plugin.
# No CLAP and no dependency on poryaaaa, ccomidi, or poryaaaa-m4l.

if(APPLE)
    enable_language(OBJC OBJCXX)
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(TEXTEDIT_ROOT "${CMAKE_CURRENT_SOURCE_DIR}")
get_filename_component(TEXTEDIT_MONOREPO_ROOT "${TEXTEDIT_ROOT}/../.." ABSOLUTE)

option(TEXTEDIT_FETCH_JUCE "Fetch JUCE with FetchContent when JUCE_ROOT is not set" ON)
set(TEXTEDIT_JUCE_VERSION "8.0.6" CACHE STRING "JUCE release version used when TEXTEDIT_FETCH_JUCE is enabled")
set(TEXTEDIT_REPO_VOICEGROUP_BRIDGE_PATH "${TEXTEDIT_MONOREPO_ROOT}/packages/voicegroup-lsp/.build/release/libVoicegroupBridge.dylib")
set(TEXTEDIT_VOICEGROUP_BRIDGE_PATH "${TEXTEDIT_REPO_VOICEGROUP_BRIDGE_PATH}"
    CACHE STRING "Swift voicegroup language bridge dylib path")
set(TEXTEDIT_VST3_BUNDLE_SIGNATURE "TxEd")
set(JUCE_ROOT "" CACHE PATH "Path to an existing JUCE checkout")

if(NOT JUCE_ROOT)
    foreach(candidate
        "${TEXTEDIT_ROOT}/JUCE"
        "${TEXTEDIT_ROOT}/juce"
        "${TEXTEDIT_MONOREPO_ROOT}/third_party/JUCE"
        "${TEXTEDIT_MONOREPO_ROOT}/third_party/juce"
    )
        if(EXISTS "${candidate}/CMakeLists.txt" AND EXISTS "${candidate}/modules/juce_core/juce_core.h")
            set(JUCE_ROOT "${candidate}" CACHE PATH "Path to an existing JUCE checkout" FORCE)
            break()
        endif()
    endforeach()
endif()

set(JUCE_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(JUCE_BUILD_EXTRAS OFF CACHE BOOL "" FORCE)
set(JUCE_ENABLE_MODULE_SOURCE_GROUPS OFF CACHE BOOL "" FORCE)

if(JUCE_ROOT)
    message(STATUS "textedit: using JUCE_ROOT=${JUCE_ROOT}")
    add_subdirectory("${JUCE_ROOT}" "${CMAKE_BINARY_DIR}/JUCE")
else()
    if(NOT TEXTEDIT_FETCH_JUCE)
        message(FATAL_ERROR "JUCE not found. Set -DJUCE_ROOT=/path/to/JUCE or enable -DTEXTEDIT_FETCH_JUCE=ON.")
    endif()

    include(FetchContent)
    FetchContent_Declare(
        JUCE
        URL "https://github.com/juce-framework/JUCE/archive/refs/tags/${TEXTEDIT_JUCE_VERSION}.tar.gz"
    )
    FetchContent_MakeAvailable(JUCE)
endif()

juce_add_plugin(textedit
    COMPANY_NAME "poryaaaa"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD TRUE
    PLUGIN_MANUFACTURER_CODE Pory
    PLUGIN_CODE TxEd
    FORMATS VST3
    PRODUCT_NAME "textedit"
)

target_sources(textedit PRIVATE
    src/CompletionList.cpp
    src/ExtractLineSelector.cpp
    src/HoverCard.cpp
    src/TextEditFileStore.cpp
    src/GruvboxTheme.cpp
    src/TextEditEditor.cpp
    src/TextEditProcessor.cpp
    src/TopToolBar.cpp
    src/VoicegroupCodeEditor.cpp
    src/VoicegroupLanguageBridge.cpp
    src/VoicegroupLanguageService.cpp
    src/VoicegroupTokeniser.cpp
)

target_include_directories(textedit PRIVATE
    src
)

target_compile_features(textedit PRIVATE cxx_std_17)

target_compile_definitions(textedit
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        TEXTEDIT_VOICEGROUP_BRIDGE_PATH="${TEXTEDIT_VOICEGROUP_BRIDGE_PATH}"
)

target_link_libraries(textedit
    PRIVATE
        juce::juce_audio_processors
        juce::juce_gui_extra
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

message(STATUS "textedit: configured as standalone JUCE VST3 plugin.")

# Embed the voicegroup bridge dylib into the .vst3 bundle as a resource
# (placed in Contents/Resources/). This makes the installed plugin
# self-contained instead of relying on an absolute path into the source tree.
# The copy only happens if the dylib is present at configure time.
# Runtime code in VoicegroupLanguageBridge prefers the bundle copy when available.
if (EXISTS "${TEXTEDIT_VOICEGROUP_BRIDGE_PATH}")
    message(STATUS "textedit: will embed voicegroup bridge dylib "
                   "${TEXTEDIT_VOICEGROUP_BRIDGE_PATH} into the VST3 bundle")
    target_sources(textedit_VST3 PRIVATE "${TEXTEDIT_VOICEGROUP_BRIDGE_PATH}")
    set_source_files_properties("${TEXTEDIT_VOICEGROUP_BRIDGE_PATH}" PROPERTIES
        HEADER_FILE_ONLY TRUE
        MACOSX_PACKAGE_LOCATION Resources)
    if(APPLE)
        get_property(TEXTEDIT_VST3_COPY_DIR GLOBAL PROPERTY JUCE_VST3_COPY_DIR)
        if(TEXTEDIT_VST3_COPY_DIR)
            add_custom_command(TARGET textedit_VST3 POST_BUILD
                COMMAND "${CMAKE_COMMAND}" -E make_directory
                    "${TEXTEDIT_VST3_COPY_DIR}/textedit.vst3/Contents/Resources"
                COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                    "${TEXTEDIT_VOICEGROUP_BRIDGE_PATH}"
                    "${TEXTEDIT_VST3_COPY_DIR}/textedit.vst3/Contents/Resources/libVoicegroupBridge.dylib"
                VERBATIM)
        endif()
    endif()
endif()

if(APPLE)
    add_custom_command(TARGET textedit_VST3 POST_BUILD
        COMMAND "${CMAKE_COMMAND}"
            "-DBUNDLE_PATH=$<TARGET_BUNDLE_DIR:textedit_VST3>"
            "-DBUNDLE_SIGNATURE=${TEXTEDIT_VST3_BUNDLE_SIGNATURE}"
            -P "${TEXTEDIT_ROOT}/cmake/FixVst3PkgInfo.cmake"
        VERBATIM)

    get_property(TEXTEDIT_VST3_COPY_DIR GLOBAL PROPERTY JUCE_VST3_COPY_DIR)
    if(TEXTEDIT_VST3_COPY_DIR)
        add_custom_command(TARGET textedit_VST3 POST_BUILD
            COMMAND "${CMAKE_COMMAND}"
                "-DBUNDLE_PATH=${TEXTEDIT_VST3_COPY_DIR}/textedit.vst3"
                "-DBUNDLE_SIGNATURE=${TEXTEDIT_VST3_BUNDLE_SIGNATURE}"
                -P "${TEXTEDIT_ROOT}/cmake/FixVst3PkgInfo.cmake"
            VERBATIM)
    endif()
endif()

add_library(textedit_bridge_missing_tab SHARED
    tests/VoicegroupBridgeMissingTab.c
)

add_executable(textedit_tests
    tests/TextEditFileStoreTests.cpp
    tests/TextEditEditorCompletionTests.cpp
    tests/TextEditProcessorTests.cpp
    tests/TextEditTests.cpp
    tests/VoicegroupLanguageBridgeTests.cpp
    src/CompletionList.cpp
    src/ExtractLineSelector.cpp
    src/HoverCard.cpp
    src/TextEditFileStore.cpp
    src/GruvboxTheme.cpp
    src/TextEditEditor.cpp
    src/TextEditProcessor.cpp
    src/TopToolBar.cpp
    src/VoicegroupCodeEditor.cpp
    src/VoicegroupLanguageBridge.cpp
    src/VoicegroupLanguageService.cpp
    src/VoicegroupTokeniser.cpp
)

target_include_directories(textedit_tests PRIVATE
    src
)

target_compile_features(textedit_tests PRIVATE cxx_std_17)
add_dependencies(textedit_tests textedit_bridge_missing_tab)

target_compile_definitions(textedit_tests
    PRIVATE
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        TEXTEDIT_VOICEGROUP_BRIDGE_PATH="${TEXTEDIT_VOICEGROUP_BRIDGE_PATH}"
        TEXTEDIT_MISSING_TAB_BRIDGE_PATH="$<TARGET_FILE:textedit_bridge_missing_tab>"
)

target_link_libraries(textedit_tests
    PRIVATE
        juce::juce_audio_processors
        juce::juce_gui_extra
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)

add_test(NAME textedit_tests COMMAND textedit_tests)
