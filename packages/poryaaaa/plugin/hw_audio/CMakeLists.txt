add_library(hw_audio STATIC
    hw_audio.c
    hw_psg.c
    hw_pcm.c
    hw_mix.c
    hw_resample.c
)
if(COMMAND poryaaaa_enable_project_warnings)
    poryaaaa_enable_project_warnings(hw_audio)
endif()

target_include_directories(hw_audio PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

# Chip reads M4ARegisterFile / M4APcmRing types from m4a/ public headers
# (visible via the parent include dir above).  No link-time dependency
# on m4a_driver — the v2-wiring foreach in the root CMakeLists.txt links
# both libraries to each executable target separately.
