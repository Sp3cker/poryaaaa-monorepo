#ifndef VG_KEYSPLIT_H
#define VG_KEYSPLIT_H

#include "vg_discovery.h"
#include "vg_paths.h"

#include <stdint.h>

/*
 * Keysplit routing table — maps MIDI notes 0..127 to sub-voice indices
 * inside a keysplit voicegroup. Two on-disk formats exist:
 *
 *   pokeemerald:  keysplit <name>, <startNote>
 *                 split <index>, <endNote>       (repeated)
 *
 *   pokefirered:  .set <name>, . - <startNote>
 *                 .byte <val>, <val>, ...        (optionally across lines)
 *
 * In the emerald form `name` is stored as "keysplit_<name>" — voice
 * macros reference it with that prefix. In the firered form `name` is
 * stored as-is.
 */
typedef struct {
    char name[VG_MAX_SYMBOL_LEN];
    int startingNote;
    uint8_t table[128];
    int maxNote;
} KeySplitDef;

typedef struct {
    KeySplitDef *entries;
    int count;
    int capacity;
} KeySplitMap;

void vg_keysplit_map_init(KeySplitMap *map);
void vg_keysplit_map_free(KeySplitMap *map);
KeySplitDef *vg_keysplit_map_find(const KeySplitMap *map, const char *name);

/* Parse every keysplit_tables.inc file in disc; append entries to *map. */
void vg_parse_keysplit_tables(const ProjectDiscovery *disc, KeySplitMap *map);

#endif /* VG_KEYSPLIT_H */
