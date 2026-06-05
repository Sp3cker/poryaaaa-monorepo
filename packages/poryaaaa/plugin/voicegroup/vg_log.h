#ifndef VG_LOG_H
#define VG_LOG_H

/*
 * Diagnostic logging for the voicegroup loader.
 *
 * vg_log: writes a line to the optional log file set via
 *         voicegroup_loader_set_log_path(). No-op when no path is set.
 *
 * vg_err: writes a line to stderr AND appends it to the log file
 *         (if a path is set). Every user-visible error the loader
 *         reports goes through this, so the diagnostic log captures
 *         the same messages an operator would see on the terminal.
 */
void vg_log(const char *fmt, ...);
void vg_err(const char *fmt, ...);

#endif /* VG_LOG_H */
