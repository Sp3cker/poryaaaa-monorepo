#include "vg_log.h"
#include "voicegroup_loader.h"

#include <stdarg.h>
#include <stdio.h>
#include <time.h>

static const char *s_vgLogPath = NULL;

#if defined(__clang__)
#define VG_LOG_DISABLE_FORMAT_NONLITERAL \
	_Pragma("clang diagnostic push") \
	_Pragma("clang diagnostic ignored \"-Wformat-nonliteral\"")
#define VG_LOG_RESTORE_FORMAT_NONLITERAL \
	_Pragma("clang diagnostic pop")
#elif defined(__GNUC__)
#define VG_LOG_DISABLE_FORMAT_NONLITERAL \
	_Pragma("GCC diagnostic push") \
	_Pragma("GCC diagnostic ignored \"-Wformat-nonliteral\"")
#define VG_LOG_RESTORE_FORMAT_NONLITERAL \
	_Pragma("GCC diagnostic pop")
#else
#define VG_LOG_DISABLE_FORMAT_NONLITERAL
#define VG_LOG_RESTORE_FORMAT_NONLITERAL
#endif

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
    VG_LOG_DISABLE_FORMAT_NONLITERAL;
    vfprintf(f, fmt, ap);
    VG_LOG_RESTORE_FORMAT_NONLITERAL;
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
    VG_LOG_DISABLE_FORMAT_NONLITERAL;
    vfprintf(stderr, fmt, ap);
    VG_LOG_RESTORE_FORMAT_NONLITERAL;
    fputc('\n', stderr);
    va_end(ap);

    va_start(ap, fmt);
    write_to_log_file(fmt, ap);
    va_end(ap);
}
