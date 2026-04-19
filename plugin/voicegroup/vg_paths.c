#include "vg_paths.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

void pathlist_add(PathList *list, const char *path)
{
    if (list->count >= VG_MAX_DISCOVERED_PATHS) return;
    for (int i = 0; i < list->count; i++) {
        if (strcmp(list->paths[i], path) == 0) return;
    }
    strncpy(list->paths[list->count], path, VG_MAX_PATH_LEN - 1);
    list->paths[list->count][VG_MAX_PATH_LEN - 1] = '\0';
    list->count++;
}

char *vg_ltrim(char *s)
{
    while (*s && isspace((unsigned char)*s)) s++;
    return s;
}

void vg_rtrim(char *s)
{
    int len = (int)strlen(s);
    while (len > 0 && isspace((unsigned char)s[len - 1])) {
        s[--len] = '\0';
    }
}

void vg_strip_comment(char *s)
{
    char *p = strchr(s, '@');
    if (p) *p = '\0';
    p = strstr(s, "//");
    if (p) *p = '\0';
}

int vg_str_ends_with_ci(const char *s, const char *suffix)
{
    size_t slen = strlen(s);
    size_t sufflen = strlen(suffix);
    if (sufflen > slen) return 0;
    for (size_t i = 0; i < sufflen; i++) {
        if (tolower((unsigned char)s[slen - sufflen + i]) != tolower((unsigned char)suffix[i]))
            return 0;
    }
    return 1;
}

void vg_build_path(char *dest, size_t destSize, const char *base, const char *relative)
{
    snprintf(dest, destSize, "%s%c%s", base, VG_PATH_SEP, relative);
    for (char *p = dest; *p; p++) {
        if (*p == '/' || *p == '\\')
            *p = VG_PATH_SEP;
    }
}

int vg_file_exists(const char *path)
{
    struct stat st;
    return stat(path, &st) == 0 && S_ISREG(st.st_mode);
}

int vg_is_directory(const char *path)
{
    struct stat st;
    if (stat(path, &st) != 0) return 0;
    return S_ISDIR(st.st_mode);
}
