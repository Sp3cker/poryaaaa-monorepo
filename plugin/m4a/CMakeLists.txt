add_library(m4a_driver STATIC
    m4a_driver.c
    m4a_freq.c
    m4a_track.c
    m4a_cgb.c
    m4a_pcm.c
    m4a_main.c
)

# The parent dir (plugin/) gives consumers `#include "m4a/..."` and also
# `#include "voicegroup/voicegroup_types.h"` for ToneData.  We do not
# link voicegroup_loader: the driver only references ToneData by *type*
# (no parser/IO calls), and every executable target already links
# voicegroup_loader directly.
target_include_directories(m4a_driver PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)
