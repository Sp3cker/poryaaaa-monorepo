#ifndef VG_PARSER_H
#define VG_PARSER_H

#include "voicegroup_loader.h"   /* LoadedVoiceGroup, VoicegroupLoaderConfig */
#include "vg_discovery.h"
#include "vg_keysplit.h"
#include "vg_symbols.h"

/*
 * Locate voicegroupName in disc, parse its contents, and populate
 * vg->voices. Loaded samples and sub-voicegroups are registered with
 * vg for later cleanup by voicegroup_free().
 *
 * Returns 0 on success, -1 if the voicegroup could not be located
 * or the file could not be parsed.
 */
int vg_parse_voicegroup(const char *projectRoot,
                        const char *voicegroupName,
                        LoadedVoiceGroup *vg,
                        const SymbolMap *dsMap,
                        const SymbolMap *pwMap,
                        const KeySplitMap *ksMap,
                        const ProjectDiscovery *disc);

#endif /* VG_PARSER_H */
