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

# Keep sources and build requirements together for normal and optimized benchmark variants.
function(poryaaaa_add_engine_library target_name)
    add_library(${target_name} STATIC
        "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../m4a_engine.c"
        "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../voicegroup_loader.c"
        "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../voicegroup_load_session.c"
        "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../voicegroup_asset_batch.c"
    )
    target_include_directories(${target_name} PUBLIC
        "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/.."
    )
    target_link_libraries(${target_name} PUBLIC
        m4a_driver
        hw_audio
    )
    if(UNIX)
        target_link_libraries(${target_name} PUBLIC m)
    endif()
endfunction()

poryaaaa_add_engine_library(poryaaaa_engine)
add_library(poryaaaa::engine ALIAS poryaaaa_engine)
