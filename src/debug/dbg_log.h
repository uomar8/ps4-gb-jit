#ifndef PS4_GB_JIT_DBG_LOG_H
#define PS4_GB_JIT_DBG_LOG_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "dbg_sce_err.h"

typedef enum DbgLogLevel {
    DBG_LVL_DEBUG = 0,
    DBG_LVL_INFO  = 1,
    DBG_LVL_WARN  = 2,
    DBG_LVL_ERROR = 3,
    DBG_LVL_FATAL = 4
} DbgLogLevel;

#define DBG_LOG_RING_CAP   16
#define DBG_LOG_LINE_MAX   256
#define DBG_STEP_RING_CAP  8
#define DBG_STEP_STR_MAX   96

void dbg_log_init(const char *log_file_path, DbgLogLevel min_level);
void dbg_log_shutdown(void);
void dbg_log_set_level(DbgLogLevel min_level);
void dbg_log_write(const char *subsys, DbgLogLevel level,
                   const char *file, int line, const char *func,
                   const char *fmt, ...);

void dbg_log_step(const char *step_desc);
size_t dbg_log_get_recent(char out_lines[][DBG_LOG_LINE_MAX], size_t max_lines);
size_t dbg_log_get_steps(char out_steps[][DBG_STEP_STR_MAX], size_t max_steps);
uint32_t dbg_log_get_error_count(void);
void dbg_log_reset_error_count(void);
int dbg_log_get_fd(void);

#define LOG_DEBUG(sub, fmt, ...) \
    dbg_log_write((sub), DBG_LVL_DEBUG, __FILE__, __LINE__, __func__, (fmt), ##__VA_ARGS__)
#define LOG_INFO(sub, fmt, ...) \
    dbg_log_write((sub), DBG_LVL_INFO,  __FILE__, __LINE__, __func__, (fmt), ##__VA_ARGS__)
#define LOG_WARN(sub, fmt, ...) \
    dbg_log_write((sub), DBG_LVL_WARN,  __FILE__, __LINE__, __func__, (fmt), ##__VA_ARGS__)
#define LOG_ERROR(sub, fmt, ...) \
    dbg_log_write((sub), DBG_LVL_ERROR, __FILE__, __LINE__, __func__, (fmt), ##__VA_ARGS__)
#define LOG_FATAL(sub, fmt, ...) \
    dbg_log_write((sub), DBG_LVL_FATAL, __FILE__, __LINE__, __func__, (fmt), ##__VA_ARGS__)

#define LOG_RC_ERROR(sub, msg, rc) \
    LOG_ERROR((sub), "%s rc=0x%08x (%s)", (msg), (uint32_t)(rc), dbg_sce_strerror((int32_t)(rc)))

#define LOG_STEP(desc) dbg_log_step((desc))

#endif /* PS4_GB_JIT_DBG_LOG_H */
