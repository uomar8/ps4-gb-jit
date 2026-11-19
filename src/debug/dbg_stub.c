#include "dbg_stub.h"
#include "dbg_log.h"
#include "../hal/hal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static DbgStubEntry g_stubs[DBG_STUB_MAX_ENTRIES];
static size_t       g_stub_count = 0;

void dbg_stub_reset(void) {
    memset(g_stubs, 0, sizeof(g_stubs));
    g_stub_count = 0;
}

void dbg_stub_record(const char *category, uint32_t id, const char *detail) {
    const char *cat = category ? category : "UNK";
    const char *det = detail ? detail : "";

    for (size_t i = 0; i < g_stub_count; i++) {
        if (g_stubs[i].id == id && strcmp(g_stubs[i].category, cat) == 0) {
            g_stubs[i].hits++;
            return;
        }
    }

    if (g_stub_count < DBG_STUB_MAX_ENTRIES) {
        DbgStubEntry *e = &g_stubs[g_stub_count++];
        (void)snprintf(e->category, sizeof(e->category), "%s", cat);
        e->id = id;
        (void)snprintf(e->detail, sizeof(e->detail), "%s", det);
        e->hits = 1;
        LOG_WARN("STUB", "first hit category=%s id=0x%04x detail=%s", cat, id, det);
    }
}

size_t dbg_stub_get_count(void) {
    return g_stub_count;
}

static int dbg_stub_cmp_desc(const void *a, const void *b) {
    const DbgStubEntry *ea = (const DbgStubEntry *)a;
    const DbgStubEntry *eb = (const DbgStubEntry *)b;
    if (eb->hits > ea->hits) return 1;
    if (eb->hits < ea->hits) return -1;
    if (ea->id < eb->id) return -1;
    if (ea->id > eb->id) return 1;
    return strcmp(ea->category, eb->category);
}

AppResult dbg_stub_write_report(const char *abs_path) {
    if (!abs_path || abs_path[0] == '\0') {
        return APP_ERR_INVALID_ARG;
    }

    DbgStubEntry *sorted = (DbgStubEntry *)malloc(sizeof(DbgStubEntry) * DBG_STUB_MAX_ENTRIES);
    if (!sorted) {
        LOG_RC_ERROR("STUB", "malloc failed for missing_report", APP_ERR_NOMEM);
        return APP_ERR_NOMEM;
    }
    memcpy(sorted, g_stubs, sizeof(DbgStubEntry) * g_stub_count);
    if (g_stub_count > 1) {
        qsort(sorted, g_stub_count, sizeof(DbgStubEntry), dbg_stub_cmp_desc);
    }

    size_t cap = 4096 + g_stub_count * 128;
    char *buf = (char *)malloc(cap);
    if (!buf) {
        free(sorted);
        LOG_RC_ERROR("STUB", "malloc failed for report buffer", APP_ERR_NOMEM);
        return APP_ERR_NOMEM;
    }

    size_t pos = 0;
    int hdr = snprintf(buf + pos, cap - pos,
                       "# ps4-gb-jit Stub-Discovery Report (sorted by hit count)\n"
                       "# hits | category | id | detail\n");
    if (hdr > 0) {
        pos += (size_t)hdr;
    }
    for (size_t i = 0; i < g_stub_count && pos + 128 < cap; i++) {
        int n = snprintf(buf + pos, cap - pos, "%llu | %s | 0x%04x | %s\n",
                         (unsigned long long)sorted[i].hits,
                         sorted[i].category,
                         sorted[i].id,
                         sorted[i].detail);
        if (n > 0) {
            pos += (size_t)n;
        }
    }
    if (g_stub_count == 0) {
        int n = snprintf(buf + pos, cap - pos, "(none)\n");
        if (n > 0) {
            pos += (size_t)n;
        }
    }

    AppResult rc = hal_fs_write_file(abs_path, (const uint8_t *)buf, pos);
    free(buf);
    free(sorted);
    return rc;
}
