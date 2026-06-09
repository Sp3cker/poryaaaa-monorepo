include(${CMAKE_CURRENT_SOURCE_DIR}/../../max-sdk-base/script/max-pretarget.cmake)

#############################################################
# porya.reverb~ - standalone Sappy/GBA-style reverb MSP effect
#############################################################

include_directories(
    "${MAX_SDK_INCLUDES}"
    "${MAX_SDK_MSP_INCLUDES}"
    "${CMAKE_CURRENT_SOURCE_DIR}"
)

file(GLOB LOCAL_SRC "*.cpp" "*.hpp")

add_library(
    ${PROJECT_NAME}
    MODULE
    ${LOCAL_SRC}
)

set_target_properties(${PROJECT_NAME} PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
)

include(${CMAKE_CURRENT_SOURCE_DIR}/../../max-sdk-base/script/max-posttarget.cmake)

set_target_properties(${PROJECT_NAME} PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
    XCODE_ATTRIBUTE_CLANG_CXX_LANGUAGE_STANDARD "gnu++23"
)
target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_23)

include(CTest)
if(BUILD_TESTING)
    add_executable(
        porya_reverb_tests
        tests/gba_reverb_tests.cpp
        gba_reverb.cpp
    )
    target_include_directories(
        porya_reverb_tests
        PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}"
    )
    target_compile_features(porya_reverb_tests PRIVATE cxx_std_23)
    add_test(NAME porya_reverb_tests COMMAND porya_reverb_tests)
endif()
