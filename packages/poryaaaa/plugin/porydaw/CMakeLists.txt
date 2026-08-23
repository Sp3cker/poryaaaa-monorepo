if(MSVC)
    add_compile_options("$<$<COMPILE_LANG_AND_ID:C,MSVC>:/experimental:c11atomics>")
endif()

if(NOT TARGET m4a_driver)
    add_subdirectory(
        "${CMAKE_CURRENT_LIST_DIR}/../m4a"
        "${CMAKE_CURRENT_BINARY_DIR}/m4a")
endif()

if(NOT TARGET hw_audio)
    add_subdirectory(
        "${CMAKE_CURRENT_LIST_DIR}/../hw_audio"
        "${CMAKE_CURRENT_BINARY_DIR}/hw_audio")
endif()

add_library(poryaaaa_engine STATIC
    "${CMAKE_CURRENT_LIST_DIR}/../m4a_engine.c"
)
add_library(poryaaaa::engine ALIAS poryaaaa_engine)

target_include_directories(poryaaaa_engine PUBLIC
    "${CMAKE_CURRENT_LIST_DIR}/.."
)
target_link_libraries(poryaaaa_engine PUBLIC
    m4a_driver
    hw_audio
)
if(TARGET voicegroup_loader)
    target_link_libraries(poryaaaa_engine PUBLIC voicegroup_loader)
endif()
if(UNIX)
    target_link_libraries(poryaaaa_engine PUBLIC m)
endif()
