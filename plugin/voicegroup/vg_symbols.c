#include "vg_symbols.h"
#include "vg_alloc.h"
#include "vg_log.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define INITIAL_CAPACITY 64

void vg_symbol_map_init(SymbolMap *map)
{
    map->entries = NULL;
    map->count = 0;
    map->capacity = 0;
}

void vg_symbol_map_free(SymbolMap *map)
{
    free(map->entries);
    map->entries = NULL;
    map->count = 0;
    map->capacity = 0;
}

static bool symbol_map_reserve(SymbolMap *map, int needed)
{
    if (needed <= map->capacity)
        return true;

    int newCapacity = map->capacity ? map->capacity : INITIAL_CAPACITY;
    while (newCapacity < needed) {
        if (newCapacity > INT_MAX / 2)
            return false;
        newCapacity *= 2;
    }

    SymbolMapping *entries = vg_realloc_array(map->entries, (size_t)newCapacity,
                                              sizeof(*map->entries));
    if (!entries)
        return false;

    map->entries = entries;
    map->capacity = newCapacity;
    return true;
}

static bool symbol_map_add(SymbolMap *map, const char *symbol, const char *path)
{
    if (!symbol_map_reserve(map, map->count + 1))
        return false;

    strncpy(map->entries[map->count].symbol, symbol, VG_MAX_SYMBOL_LEN - 1);
    map->entries[map->count].symbol[VG_MAX_SYMBOL_LEN - 1] = '\0';
    strncpy(map->entries[map->count].filePath, path, VG_MAX_PATH_LEN - 1);
    map->entries[map->count].filePath[VG_MAX_PATH_LEN - 1] = '\0';
    map->count++;
    return true;
}

const char *vg_symbol_map_find(const SymbolMap *map, const char *symbol)
{
    for (int i = 0; i < map->count; i++) {
        if (strcmp(map->entries[i].symbol, symbol) == 0)
            return map->entries[i].filePath;
    }
    return NULL;
}

/*
 * DirectSound and ProgrammableWave symbol files share the same
 * on-disk format — a series of `<label>::` lines each followed by a
 * `.incbin "relative/path"` line — so one parser handles both.
 */
static bool parse_symbol_inc_file(const char *filePath, SymbolMap *map)
{
    FILE *f = fopen(filePath, "r");
    if (!f) {
        vg_err("cannot open %s", filePath);
        return true;
    }

    char line[VG_MAX_LINE];
    char currentSymbol[VG_MAX_SYMBOL_LEN] = {0};

    while (fgets(line, sizeof(line), f)) {
        vg_strip_comment(line);
        vg_rtrim(line);
        char *trimmed = vg_ltrim(line);

        char *colonColon = strstr(trimmed, "::");
        if (colonColon && colonColon > trimmed) {
            *colonColon = '\0';
            strncpy(currentSymbol, trimmed, VG_MAX_SYMBOL_LEN - 1);
            currentSymbol[VG_MAX_SYMBOL_LEN - 1] = '\0';
            continue;
        }

        if (currentSymbol[0] && strstr(trimmed, ".incbin")) {
            char *quote1 = strchr(trimmed, '"');
            if (quote1) {
                quote1++;
                char *quote2 = strchr(quote1, '"');
                if (quote2) {
                    *quote2 = '\0';
                    if (!symbol_map_add(map, currentSymbol, quote1)) {
                        vg_err("out of memory while parsing symbols in %s", filePath);
                        fclose(f);
                        return false;
                    }
                }
            }
            currentSymbol[0] = '\0';
        }
    }

    fclose(f);
    return true;
}

bool vg_parse_direct_sound_data(const ProjectDiscovery *disc, SymbolMap *map)
{
    for (int i = 0; i < disc->directSoundDataFiles.count; i++) {
        if (!parse_symbol_inc_file(disc->directSoundDataFiles.paths[i], map))
            return false;
    }
    return true;
}

bool vg_parse_prog_wave_data(const ProjectDiscovery *disc, SymbolMap *map)
{
    for (int i = 0; i < disc->progWaveDataFiles.count; i++) {
        if (!parse_symbol_inc_file(disc->progWaveDataFiles.paths[i], map))
            return false;
    }
    return true;
}
