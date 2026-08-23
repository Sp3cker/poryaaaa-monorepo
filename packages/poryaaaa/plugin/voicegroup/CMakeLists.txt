# voicegroup_loader — pure-C parser for pokeemerald/pokefirered
# voicegroup files. No audio-engine dependencies; any tool that wants
# to read a decomp project's voices can link this library.

add_library(voicegroup_loader STATIC
    voicegroup_loader.c
    voicegroup_materializer.c
    voicegroup_overflow.c
    voicegroup_project.c
    voicegroup_project_result.c
    voicegroup_project_asset.c
    vg_log.c
    vg_paths.c
    vg_discovery.c
    vg_symbols.c
    vg_keysplit.c
    vg_source.c
    vg_aiff.c
    vg_wav.c
    vg_parser.c
    voicegroup_project_state.c
)
if(COMMAND poryaaaa_enable_project_warnings)
    poryaaaa_enable_project_warnings(voicegroup_loader)
endif()

# PUBLIC so consumers pick up the "voicegroup/xxx.h" prefix we use
# throughout the project.
target_include_directories(voicegroup_loader PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_include_directories(voicegroup_loader PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../shared
    "${PORYAAAA_VOICEGROUP_CORE_DIR}/include"
)
if(TARGET voicegroup_core_static)
    target_link_libraries(voicegroup_loader PUBLIC voicegroup_core_static)
endif()

option(PORYAAAA_BUILD_VOICEGROUP_PROJECT_HARNESS
       "Build the focused VoicegroupProject C harness"
       OFF)
if(PORYAAAA_BUILD_VOICEGROUP_PROJECT_HARNESS AND TARGET voicegroup_core_static)
    add_executable(voicegroup_project_harness voicegroup_project_harness.c)
    target_link_libraries(voicegroup_project_harness PRIVATE voicegroup_loader)
    target_include_directories(voicegroup_project_harness PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../../../../shared
        "${PORYAAAA_VOICEGROUP_CORE_DIR}/include")
endif()
