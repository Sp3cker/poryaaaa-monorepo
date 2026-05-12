#ifndef VOICEGROUP_STATE_H
#define VOICEGROUP_STATE_H

#include <stdbool.h>

#include "voicegroup_loader.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Write a voicegroup snapshot JSON to a fixed per-user location so sibling
 * tools (ccomidi, etc.) can read the currently-loaded voicegroup regardless
 * of which plugin host loaded poryaaaa. The destination is:
 *
 *   macOS:   $HOME/Library/Application Support/poryaaaa/state.json
 *   Windows: %APPDATA%\poryaaaa\state.json
 *   Linux:   $XDG_CONFIG_HOME/poryaaaa/state.json
 *            (or $HOME/.config/poryaaaa/state.json)
 *
 * Writes a temp file first then renames over the final path so readers
 * never observe a partial file.
 *
 * Returns true on success.
 */
bool voicegroup_state_write_default(const char *projectRoot,
                                    const char *voicegroupName,
                                    const LoadedVoiceGroup *vg);

#ifdef __cplusplus
}
#endif

#endif /* VOICEGROUP_STATE_H */
