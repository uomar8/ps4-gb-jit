#include "../hal.h"
#include "../../debug/dbg_log.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <sys/mman.h>

#include <orbis/libkernel.h>

typedef int32_t (*FnJitCreateSharedMemory)(const char *name, size_t len,
                                           int max_page_prot, int *fd_out);
typedef int32_t (*FnJitCreateAliasOfSharedMemory)(int fd, int max_page_prot, int *fd_out);

#define PS4_LIBKERNEL_HANDLE 0x2001
#define PS4_PROT_RW          0x03 /* READ | WRITE */
#define PS4_PROT_RWX         0x07 /* READ | WRITE | EXEC (required on retail!) */

static FnJitCreateSharedMemory        g_fn_jit_create = NULL;
static FnJitCreateAliasOfSharedMemory g_fn_jit_alias  = NULL;
static bool                           g_jit_resolved  = false;

static void ps4_jit_resolve_symbols(void) {
    if (g_jit_resolved) {
        return;
    }
    g_jit_resolved = true;
    void *p1 = NULL;
    void *p2 = NULL;
    int32_t rc1 = sceKernelDlsym(PS4_LIBKERNEL_HANDLE, "sceKernelJitCreateSharedMemory", &p1);
    int32_t rc2 = sceKernelDlsym(PS4_LIBKERNEL_HANDLE, "sceKernelJitCreateAliasOfSharedMemory", &p2);
    if (rc1 == 0 && p1) {
        memcpy(&g_fn_jit_create, &p1, sizeof(g_fn_jit_create));
    } else {
        LOG_WARN("JIT", "dlsym sceKernelJitCreateSharedMemory failed rc=0x%08x (%s)",
                 (uint32_t)rc1, dbg_sce_strerror(rc1));
    }
    if (rc2 == 0 && p2) {
        memcpy(&g_fn_jit_alias, &p2, sizeof(g_fn_jit_alias));
    } else {
        LOG_WARN("JIT", "dlsym sceKernelJitCreateAliasOfSharedMemory failed rc=0x%08x (%s)",
                 (uint32_t)rc2, dbg_sce_strerror(rc2));
    }
}

AppResult hal_jit_alloc(HalJitArena *arena, size_t size) {
    if (!arena || size == 0 || (size % HAL_PAGE_SIZE) != 0) {
        return APP_ERR_INVALID_ARG;
    }
    memset(arena, 0, sizeof(*arena));
    arena->rx_fd = -1;
    arena->rw_fd = -1;

    ps4_jit_resolve_symbols();
    if (!g_fn_jit_create || !g_fn_jit_alias) {
        return APP_ERR_JIT_UNAVAIL;
    }

    int rx_fd = -1;
    int rw_fd = -1;
    int32_t rc = g_fn_jit_create(NULL, size, PS4_PROT_RWX, &rx_fd);
    if (rc < 0 || rx_fd < 0) {
        LOG_WARN("JIT", "sceKernelJitCreateSharedMemory failed rc=0x%08x (%s) — trying GoldHEN RWX mmap",
                 (uint32_t)rc, dbg_sce_strerror(rc));
        void *rwx = mmap(NULL, size, PROT_READ | PROT_WRITE | PROT_EXEC,
                         MAP_PRIVATE | MAP_ANON, -1, 0);
        if (rwx != MAP_FAILED && rwx != NULL) {
            arena->rx_base = rwx;
            arena->rw_base = rwx;
            arena->size = size;
            arena->rx_fd = -1;
            arena->rw_fd = -1;
            LOG_INFO("JIT", "GoldHEN RWX mmap arena ready ptr=%p size=0x%zx", rwx, size);
            return APP_OK;
        }
        LOG_WARN("JIT", "GoldHEN RWX mmap unavailable — falling back to interpreter");
        return APP_ERR_JIT_UNAVAIL;
    }

    rc = g_fn_jit_alias(rx_fd, PS4_PROT_RW, &rw_fd);
    if (rc < 0 || rw_fd < 0) {
        LOG_WARN("JIT", "sceKernelJitCreateAliasOfSharedMemory failed rc=0x%08x (%s)",
                 (uint32_t)rc, dbg_sce_strerror(rc));
        (void)sceKernelClose(rx_fd);
        return APP_ERR_JIT_UNAVAIL;
    }

    void *rx = mmap(NULL, size, PROT_READ | PROT_EXEC, MAP_SHARED, rx_fd, 0);
    if (rx == MAP_FAILED) {
        LOG_WARN("JIT", "mmap(RX) failed");
        (void)sceKernelClose(rw_fd);
        (void)sceKernelClose(rx_fd);
        return APP_ERR_JIT_UNAVAIL;
    }

    void *rw = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, rw_fd, 0);
    if (rw == MAP_FAILED) {
        LOG_WARN("JIT", "mmap(RW) failed");
        (void)munmap(rx, size);
        (void)sceKernelClose(rw_fd);
        (void)sceKernelClose(rx_fd);
        return APP_ERR_JIT_UNAVAIL;
    }

    arena->rx_base = rx;
    arena->rw_base = rw;
    arena->size = size;
    arena->rx_fd = rx_fd;
    arena->rw_fd = rw_fd;
    LOG_INFO("JIT", "dual-mapped arena ready rx=%p rw=%p size=0x%zx", rx, rw, size);
    return APP_OK;
}

void hal_jit_free(HalJitArena *arena) {
    if (!arena) {
        return;
    }
    if (arena->rx_base) {
        (void)munmap(arena->rx_base, arena->size);
    }
    if (arena->rw_base && arena->rw_base != arena->rx_base) {
        (void)munmap(arena->rw_base, arena->size);
    }
    if (arena->rw_fd >= 0) {
        (void)sceKernelClose(arena->rw_fd);
    }
    if (arena->rx_fd >= 0) {
        (void)sceKernelClose(arena->rx_fd);
    }
    memset(arena, 0, sizeof(*arena));
    arena->rx_fd = -1;
    arena->rw_fd = -1;
}

void hal_jit_flush_icache(void *rx_addr, size_t size) {
    __builtin___clear_cache((char *)rx_addr, (char *)rx_addr + size);
}
