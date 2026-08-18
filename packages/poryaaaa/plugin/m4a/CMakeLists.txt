if(NOT PORYAAAA_M4A_DRIVER_SOURCES)
    include("${CMAKE_CURRENT_LIST_DIR}/m4a_driver_sources.cmake")
endif()

add_library(m4a_driver STATIC
    ${PORYAAAA_M4A_DRIVER_SOURCES}
)

if(PORYAAAA_BUILD_TRACE_TOOLS)
    add_library(poryaaaa_m4a_trace_writer STATIC
        m4a_driver_trace.c
    )
    if(COMMAND poryaaaa_enable_project_warnings)
        poryaaaa_enable_project_warnings(poryaaaa_m4a_trace_writer)
    endif()
    target_include_directories(poryaaaa_m4a_trace_writer PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/..
    )
endif()
if(COMMAND poryaaaa_enable_project_warnings)
    poryaaaa_enable_project_warnings(m4a_driver)
endif()

# The parent dir (plugin/) gives consumers `#include "m4a/..."` and also
# `#include "voicegroup/voicegroup_types.h"` for ToneData.  We do not
# link voicegroup_loader: the driver only references ToneData by *type*
# (no parser/IO calls), and every executable target already links
# voicegroup_loader directly.
target_include_directories(m4a_driver PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)
