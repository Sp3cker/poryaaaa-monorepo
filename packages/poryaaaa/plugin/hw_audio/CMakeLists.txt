set(PORYAAAA_HW_AUDIO_CORE_SOURCES
    hw_audio.c
    hw_psg.c
    hw_pcm.c
    hw_mix.c
    hw_resample.c
)

add_library(hw_audio STATIC
    ${PORYAAAA_HW_AUDIO_CORE_SOURCES}
)
if(COMMAND poryaaaa_enable_project_warnings)
    poryaaaa_enable_project_warnings(hw_audio)
endif()
target_compile_definitions(hw_audio PRIVATE PORYAAAA_HW_AUDIO_TRACE=0)
target_include_directories(hw_audio PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

if(PORYAAAA_BUILD_TRACE_TOOLS)
    add_library(poryaaaa_hw_audio_trace_tool STATIC
        ${PORYAAAA_HW_AUDIO_CORE_SOURCES}
        hw_audio_trace_text.c
        hw_audio_trace_schedule.c
    )
    if(COMMAND poryaaaa_enable_project_warnings)
        poryaaaa_enable_project_warnings(poryaaaa_hw_audio_trace_tool)
    endif()
    target_compile_definitions(poryaaaa_hw_audio_trace_tool PRIVATE PORYAAAA_HW_AUDIO_TRACE=1)
    target_include_directories(poryaaaa_hw_audio_trace_tool PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/..
    )
endif()
