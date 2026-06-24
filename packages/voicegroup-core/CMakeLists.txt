cmake_minimum_required(VERSION 3.24)
project(voicegroup_core CXX)

add_library(voicegroup_core STATIC
    src/macro_catalog.cpp
    src/project_index.cpp
    src/voicegroup_parser.cpp
)

target_compile_features(voicegroup_core PUBLIC cxx_std_20)
target_include_directories(voicegroup_core PUBLIC include)

add_executable(voicegroup_core_tests
    test/voicegroup_core_tests.cpp
)
target_link_libraries(voicegroup_core_tests PRIVATE voicegroup_core)

enable_testing()
add_test(NAME voicegroup_core_tests COMMAND voicegroup_core_tests)
