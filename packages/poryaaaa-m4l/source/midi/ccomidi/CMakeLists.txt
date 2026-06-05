include(${CMAKE_CURRENT_SOURCE_DIR}/../../max-sdk-base/script/max-pretarget.cmake)

#############################################################
# ccomidi — MIDI effect: MIDI passthrough + m4a CC sender
#############################################################

set(CCO_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../../../../packages/ccomidi"
    CACHE PATH "Path to ccomidi repo root (the dir containing src/core/sender_core.cpp)")

include_directories(
    "${MAX_SDK_INCLUDES}"
    "${CCO_ROOT}/src"
)

file(GLOB LOCAL_SRC "*.cpp" "*.h")

set(CORE_SRC
    "${CCO_ROOT}/src/core/sender_core.cpp"
)

add_library(
    ${PROJECT_NAME}
    MODULE
    ${LOCAL_SRC}
    ${CORE_SRC}
)

set_target_properties(${PROJECT_NAME} PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)

include(${CMAKE_CURRENT_SOURCE_DIR}/../../max-sdk-base/script/max-posttarget.cmake)

#############################################################
# ccomidi_parser_tests — unit tests for the wrapper-side
# pure-C++ logic (MIDI byte parser).
# Builds as a plain executable; run directly, no Max needed.
#############################################################

add_executable(ccomidi_parser_tests
    tests/test_ccomidi_parser.cpp
    ccomidi_parser.cpp
)
set_target_properties(ccomidi_parser_tests PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)
