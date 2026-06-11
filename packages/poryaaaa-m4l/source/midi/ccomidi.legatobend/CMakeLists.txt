include(${CMAKE_CURRENT_SOURCE_DIR}/../../max-sdk-base/script/max-pretarget.cmake)

#############################################################
# ccomidi.legatobend - MIDI portamento-note-glide transform
#############################################################

file(GLOB LOCAL_SRC "*.cpp" "*.hpp")

include_directories(
    "${MAX_SDK_INCLUDES}"
)

add_library(
    ${PROJECT_NAME}
    MODULE
    ${LOCAL_SRC}
)

set_target_properties(${PROJECT_NAME} PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)

include(${CMAKE_CURRENT_SOURCE_DIR}/../../max-sdk-base/script/max-posttarget.cmake)

#############################################################
# ccomidi_legatobend_tests - pure-C++ gesture tests
#############################################################

add_executable(ccomidi_legatobend_tests
    tests/test_legatobend_core.cpp
    legatobend_core.cpp
    legatobend_parser.cpp
)
set_target_properties(ccomidi_legatobend_tests PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)
