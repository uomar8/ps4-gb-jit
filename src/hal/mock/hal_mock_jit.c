#define _GNU_SOURCE
#include "../hal.h"
#include "hal_mock_ctl.h"
#include "../../debug/dbg_log.h"

#include <sys/mman.h>
#include <unistd.h>
#include <string.h>

static bool g_fail_eacces = false;

void hal_mock_jit_set_fail_eacces(bool enable) {
    g_fail_eacces = enable;
}

AppResult hal_jit_alloc(HalJitArena *arena, size_t size) {
    if (!arena || size == 0 || (size % HAL_PAGE_SIZE) != 0) {
        return APP_ERR_INVALID_ARG;
    }
    memset(arena, 0, sizeof(*arena));
    arena->rx_fd = -1;
    arena->rw_fd = -1;

    if (g_fail_eacces) {
        LOG_WARN("JIT", "mock fault injection: returning APP_ERR_JIT_UNAVAIL (simulated EACCES)");
        return APP_ERR_JIT_UNAVAIL;
    }

    int fd = memfd_create("ps4_gb_jit_cache", MFD_CLOEXEC);
    if (fd < 0) {
        return APP_ERR_JIT_UNAVAIL;
    }
    if (ftruncate(fd, (off_t)size) != 0) {
        close(fd);
        return APP_ERR_NOMEM;
    }

    void *rw = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (rw == MAP_FAILED) {
        close(fd);
        return APP_ERR_NOMEM;
    }
    void *rx = mmap(NULL, size, PROT_READ | PROT_EXEC, MAP_SHARED, fd, 0);
    if (rx == MAP_FAILED) {
        munmap(rw, size);
        close(fd);
        return APP_ERR_JIT_UNAVAIL;
    }

    arena->rx_base = rx;
    arena->rw_base = rw;
    arena->size = size;
    arena->rx_fd = fd;
    arena->rw_fd = -1;
    return APP_OK;
}

void hal_jit_free(HalJitArena *arena) {
    if (!arena) {
        return;
    }
    if (arena->rx_base) {
        munmap(arena->rx_base, arena->size);
    }
    if (arena->rw_base) {
        munmap(arena->rw_base, arena->size);
    }
    if (arena->rx_fd >= 0) {
        close(arena->rx_fd);
    }
    memset(arena, 0, sizeof(*arena));
    arena->rx_fd = -1;
    arena->rw_fd = -1;
}

void hal_jit_flush_icache(void *rx_addr, size_t size) {
    __builtin___clear_cache((char *)rx_addr, (char *)rx_addr + size);
}
