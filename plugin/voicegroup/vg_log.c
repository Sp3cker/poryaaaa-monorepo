#include "vg_log.h"
#include "voicegroup_loader.h"

#include <stdarg.h>
#include <stdio.h>
#include <time.h>

static const char *s_vgLogPath = NULL;

void voicegroup_loader_set_log_path(const char *path)
{
    s_vgLogPath = path;
}

static void write_to_log_file(const char *fmt, va_list ap)
{
    if (!s_vgLogPath) return;
    FILE *f = fopen(s_vgLogPath, "a");
    if (!f) return;
    time_t t = time(NULL);
    char tbuf[32];
    strftime(tbuf, sizeof(tbuf), "%H:%M:%S", localtime(&t));
    fprintf(f, "[%s] vg_loader: ", tbuf);
    vfprintf(f, fmt, ap);
    fputc('\n', f);
    fclose(f);
}

void vg_log(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    write_to_log_file(fmt, ap);
    va_end(ap);
}

void vg_err(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    fprintf(stderr, "voicegroup_loader: ");
    vfprintf(stderr, fmt, ap);
    fputc('\n', stderr);
    va_end(ap);

    va_start(ap, fmt);
    write_to_log_file(fmt, ap);
    va_end(ap);
}
