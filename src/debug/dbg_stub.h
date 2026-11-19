#ifndef PS4_GB_JIT_DBG_STUB_H
#define PS4_GB_JIT_DBG_STUB_H

#include <stddef.h>
#include <stdint.h>
#include "../core/result.h"

#define DBG_STUB_MAX_ENTRIES 128

typedef struct DbgStubEntry {
    char     category[24];
    uint32_t id;
    char     detail[64];
    uint64_t hits;
} DbgStubEntry;

void      dbg_stub_reset(void);
void      dbg_stub_record(const char *category, uint32_t id, const char *detail);
size_t    dbg_stub_get_count(void);
AppResult dbg_stub_write_report(const char *abs_path);

#endif /* PS4_GB_JIT_DBG_STUB_H */
