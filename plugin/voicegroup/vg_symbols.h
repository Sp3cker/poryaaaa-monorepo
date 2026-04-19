#ifndef VG_SYMBOLS_H
#define VG_SYMBOLS_H

#include "vg_discovery.h"
#include "vg_paths.h"

/*
 * Symbol map: associates an assembly label (symbol) with the file
 * path it points at via .incbin. Built by scanning a project's
 * direct_sound_data.inc and programmable_wave_data.inc files.
 */
typedef struct {
    char symbol[VG_MAX_SYMBOL_LEN];
    char filePath[VG_MAX_PATH_LEN];
} SymbolMapping;

typedef struct {
    SymbolMapping *entries;
    int count;
    int capacity;
} SymbolMap;

void vg_symbol_map_init(SymbolMap *map);
void vg_symbol_map_free(SymbolMap *map);
const char *vg_symbol_map_find(const SymbolMap *map, const char *symbol);

/*
 * Parse every direct_sound_data.inc (or programmable_wave_data.inc)
 * file discovered in disc; append symbol -> path mappings to *map.
 * Both files share the same <label>:: + .incbin syntax.
 */
void vg_parse_direct_sound_data(const ProjectDiscovery *disc, SymbolMap *map);
void vg_parse_prog_wave_data(const ProjectDiscovery *disc, SymbolMap *map);

#endif /* VG_SYMBOLS_H */
