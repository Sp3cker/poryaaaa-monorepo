#ifndef VOICEGROUP_STATE_H
#define VOICEGROUP_STATE_H

#include <stdbool.h>

#include "voicegroup_loader.h"

#ifdef __cplusplus
extern "C"
{
#endif

    /*
     * Write a voicegroup snapshot JSON to the shared per-user projects.json so
     * sibling tools can read the currently-loaded voicegroup regardless of which
     * plugin host loaded poryaaaa. The destination is:
     *
     *   macOS:   $HOME/Library/Application Support/poryaaaa/projects.json
     *   Windows: %APPDATA%\poryaaaa\projects.json
     *   Linux:   $XDG_CONFIG_HOME/poryaaaa/projects.json
     *            (or $HOME/.config/poryaaaa/projects.json)
     *
     * Writes a temp file first then renames over the final path so readers
     * never observe a partial file.
     *
     * Returns true on success.
     */
    bool
    voicegroup_state_write_default(const char* projectRoot, const char* voicegroupName, const LoadedVoiceGroup* vg);

#ifdef __cplusplus
}
#endif

#endif /* VOICEGROUP_STATE_H */
