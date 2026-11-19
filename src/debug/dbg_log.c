#include "dbg_log.h"
#include "../hal/hal.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#if defined(_WIN32)
#include <io.h>
#include <fcntl.h>
#define DBG_OPEN_WRITE(p) _open((p), _O_WRONLY | _O_CREAT | _O_TRUNC | _O_BINARY, 0644)
#define DBG_WRITE(fd, b, n) _write((fd), (b), (unsigned int)(n))
#define DBG_FSYNC(fd) _commit((fd))
#define DBG_CLOSE(fd) _close((fd))
#else
#include <fcntl.h>
#include <unistd.h>
#define DBG_OPEN_WRITE(p) open((p), O_WRONLY | O_CREAT | O_TRUNC, 0644)
#define DBG_WRITE(fd, b, n) write((fd), (b), (n))
#define DBG_FSYNC(fd) fsync((fd))
#define DBG_CLOSE(fd) close((fd))
#endif

static int         g_log_fd = -1;
static DbgLogLevel g_min_level = DBG_LVL_INFO;
static uint32_t    g_error_count = 0;

static char        g_recent[DBG_LOG_RING_CAP][DBG_LOG_LINE_MAX];
static size_t      g_recent_head = 0;
static size_t      g_recent_count = 0;

static char        g_steps[DBG_STEP_RING_CAP][DBG_STEP_STR_MAX];
static size_t      g_step_head = 0;
static size_t      g_step_count = 0;

static const char *dbg_level_str(DbgLogLevel lvl) {
    switch (lvl) {
        case DBG_LVL_DEBUG: return "DEBUG";
        case DBG_LVL_INFO:  return "INFO";
        case DBG_LVL_WARN:  return "WARN";
        case DBG_LVL_ERROR: return "ERROR";
        case DBG_LVL_FATAL: return "FATAL";
        default:            return "UNK";
    }
}

static const char *dbg_basename(const char *path) {
    const char *p = path;
    const char *last = path;
    if (!path) {
        return "?";
    }
    while (*p) {
        if (*p == '/' || *p == '\\') {
            last = p + 1;
        }
        p++;
    }
    return last;
}

void dbg_log_init(const char *log_file_path, DbgLogLevel min_level) {
    g_min_level = min_level;
    g_error_count = 0;
    g_recent_head = 0;
    g_recent_count = 0;
    g_step_head = 0;
    g_step_count = 0;

    if (g_log_fd >= 0) {
        DBG_CLOSE(g_log_fd);
        g_log_fd = -1;
    }
    if (log_file_path && log_file_path[0] != '\0') {
        g_log_fd = DBG_OPEN_WRITE(log_file_path);
    }
}

void dbg_log_shutdown(void) {
    if (g_log_fd >= 0) {
        (void)DBG_FSYNC(g_log_fd);
        DBG_CLOSE(g_log_fd);
        g_log_fd = -1;
    }
}

void dbg_log_set_level(DbgLogLevel min_level) {
    g_min_level = min_level;
}

void dbg_log_write(const char *subsys, DbgLogLevel level,
                   const char *file, int line, const char *func,
                   const char *fmt, ...) {
    if (level >= DBG_LVL_ERROR) {
        g_error_count++;
    }
    if (level < g_min_level) {
        return;
    }

    char msg[160];
    va_list ap;
    va_start(ap, fmt);
    (void)vsnprintf(msg, sizeof(msg), fmt, ap);
    va_end(ap);

    char out[DBG_LOG_LINE_MAX];
    uint64_t ts = hal_sys_get_time_us();
    int n = snprintf(out, sizeof(out) - 2,
                     "[%llu] [%s] [%s] [%s:%d %s()] %s",
                     (unsigned long long)ts,
                     subsys ? subsys : "SYS",
                     dbg_level_str(level),
                     dbg_basename(file),
                     line,
                     func ? func : "?",
                     msg);
    if (n < 0) {
        return;
    }
    if ((size_t)n >= sizeof(out) - 2) {
        n = (int)(sizeof(out) - 2);
    }
    out[n] = '\0';

    /* Save in HUD ring buffer */
    memcpy(g_recent[g_recent_head], out, (size_t)n + 1);
    g_recent_head = (g_recent_head + 1) % DBG_LOG_RING_CAP;
    if (g_recent_count < DBG_LOG_RING_CAP) {
        g_recent_count++;
    }

    out[n] = '\n';
    out[n + 1] = '\0';

    /* Tee to platform klog / console */
    hal_sys_klog_write(out);

    /* Persist to log file */
    if (g_log_fd >= 0) {
        (void)DBG_WRITE(g_log_fd, out, (size_t)(n + 1));
        if (level >= DBG_LVL_INFO) {
            (void)DBG_FSYNC(g_log_fd);
        }
    }
}

void dbg_log_step(const char *step_desc) {
    if (!step_desc) {
        return;
    }
    size_t len = strlen(step_desc);
    if (len >= DBG_STEP_STR_MAX) {
        len = DBG_STEP_STR_MAX - 1;
    }
    memcpy(g_steps[g_step_head], step_desc, len);
    g_steps[g_step_head][len] = '\0';
    g_step_head = (g_step_head + 1) % DBG_STEP_RING_CAP;
    if (g_step_count < DBG_STEP_RING_CAP) {
        g_step_count++;
    }
}

size_t dbg_log_get_recent(char out_lines[][DBG_LOG_LINE_MAX], size_t max_lines) {
    size_t count = g_recent_count < max_lines ? g_recent_count : max_lines;
    size_t start = (g_recent_head + DBG_LOG_RING_CAP - count) % DBG_LOG_RING_CAP;
    for (size_t i = 0; i < count; i++) {
        size_t idx = (start + i) % DBG_LOG_RING_CAP;
        memcpy(out_lines[i], g_recent[idx], DBG_LOG_LINE_MAX);
    }
    return count;
}

size_t dbg_log_get_steps(char out_steps[][DBG_STEP_STR_MAX], size_t max_steps) {
    size_t count = g_step_count < max_steps ? g_step_count : max_steps;
    size_t start = (g_step_head + DBG_STEP_RING_CAP - count) % DBG_STEP_RING_CAP;
    for (size_t i = 0; i < count; i++) {
        size_t idx = (start + i) % DBG_STEP_RING_CAP;
        memcpy(out_steps[i], g_steps[idx], DBG_STEP_STR_MAX);
    }
    return count;
}

uint32_t dbg_log_get_error_count(void) {
    return g_error_count;
}

void dbg_log_reset_error_count(void) {
    g_error_count = 0;
}

int dbg_log_get_fd(void) {
    return g_log_fd;
}
