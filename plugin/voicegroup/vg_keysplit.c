#include "vg_keysplit.h"
#include "vg_log.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define INITIAL_CAPACITY 64

void vg_keysplit_map_init(KeySplitMap *map)
{
    map->entries = NULL;
    map->count = 0;
    map->capacity = 0;
}

void vg_keysplit_map_free(KeySplitMap *map)
{
    free(map->entries);
    map->entries = NULL;
    map->count = 0;
    map->capacity = 0;
}

KeySplitDef *vg_keysplit_map_find(const KeySplitMap *map, const char *name)
{
    for (int i = 0; i < map->count; i++) {
        if (strcmp(map->entries[i].name, name) == 0)
            return &map->entries[i];
    }
    return NULL;
}

/*
 * Append a zeroed KeySplitDef and return its address. Both format
 * parsers call this when encountering a new table header.
 */
static KeySplitDef *append_entry(KeySplitMap *map, const char *name, int startNote)
{
    if (map->count >= map->capacity) {
        map->capacity = map->capacity ? map->capacity * 2 : INITIAL_CAPACITY;
        map->entries = realloc(map->entries, sizeof(KeySplitDef) * map->capacity);
    }
    KeySplitDef *entry = &map->entries[map->count++];
    memset(entry, 0, sizeof(KeySplitDef));
    strncpy(entry->name, name, VG_MAX_SYMBOL_LEN - 1);
    entry->name[VG_MAX_SYMBOL_LEN - 1] = '\0';
    entry->startingNote = startNote;
    return entry;
}

static void fill_range(KeySplitDef *entry, int from, int to, uint8_t value)
{
    for (int n = from; n < to && n < 128; n++)
        entry->table[n] = value;
    if (to > entry->maxNote && to <= 128)
        entry->maxNote = to;
}

static void set_one(KeySplitDef *entry, int at, uint8_t value)
{
    if (at < 0 || at >= 128) return;
    entry->table[at] = value;
    if (at > entry->maxNote)
        entry->maxNote = at;
}

/*
 * pokeemerald macro format. Matches lines starting with "keysplit " or
 * "split ". Returns silently if the line doesn't match.
 */
static void parse_emerald_line(const char *trimmed, KeySplitMap *map,
                               KeySplitDef **current, int *lastNote)
{
    if (strncmp(trimmed, "keysplit ", 9) == 0) {
        char name[VG_MAX_SYMBOL_LEN];
        int startNote = 0;
        if (sscanf(trimmed + 9, "%[^,], %d", name, &startNote) >= 1) {
            vg_rtrim(name);
            char fullName[VG_MAX_SYMBOL_LEN];
            snprintf(fullName, sizeof(fullName), "keysplit_%s", name);
            *current = append_entry(map, fullName, startNote);
            *lastNote = startNote;
        }
        return;
    }
    if (strncmp(trimmed, "split ", 6) == 0 && *current) {
        int index, endNote;
        if (sscanf(trimmed + 6, "%d, %d", &index, &endNote) == 2) {
            fill_range(*current, *lastNote, endNote, (uint8_t)index);
            *lastNote = endNote;
        }
    }
}

/*
 * pokefirered raw format. Matches lines starting with ".set " (new
 * table) or ".byte " (per-note values, possibly multiple per line).
 */
static void parse_firered_line(const char *trimmed, KeySplitMap *map,
                               KeySplitDef **current, int *lastNote)
{
    if (strncmp(trimmed, ".set ", 5) == 0) {
        char name[VG_MAX_SYMBOL_LEN];
        int startNote = 0;
        if (sscanf(trimmed + 5, "%[^,], . - %d", name, &startNote) == 2) {
            vg_rtrim(name);
            *current = append_entry(map, name, startNote);
            *lastNote = startNote;
        }
        return;
    }
    if (strncmp(trimmed, ".byte ", 6) == 0 && *current) {
        const char *p = trimmed + 6;
        while (*p) {
            char *end;
            long val = strtol(p, &end, 10);
            if (end == p) break;
            set_one(*current, *lastNote, (uint8_t)val);
            (*lastNote)++;
            p = end;
            while (isspace((unsigned char)*p)) p++;
            if (*p == ',') p++;
            while (isspace((unsigned char)*p)) p++;
        }
    }
}

/*
 * Parse one keysplit_tables.inc. Each line is offered to both format
 * parsers; they key off disjoint leading tokens so at most one acts.
 * A file may even mix formats — the parser just follows whichever
 * keyword it sees.
 */
static void parse_keysplit_file(const char *filePath, KeySplitMap *map)
{
    FILE *f = fopen(filePath, "r");
    if (!f) {
        vg_err("cannot open %s", filePath);
        return;
    }

    char line[VG_MAX_LINE];
    KeySplitDef *current = NULL;
    int lastNote = 0;

    while (fgets(line, sizeof(line), f)) {
        vg_strip_comment(line);
        vg_rtrim(line);
        char *trimmed = vg_ltrim(line);
        parse_emerald_line(trimmed, map, &current, &lastNote);
        parse_firered_line(trimmed, map, &current, &lastNote);
    }

    fclose(f);
}

void vg_parse_keysplit_tables(const ProjectDiscovery *disc, KeySplitMap *map)
{
    for (int i = 0; i < disc->keySplitTableFiles.count; i++)
        parse_keysplit_file(disc->keySplitTableFiles.paths[i], map);
}
