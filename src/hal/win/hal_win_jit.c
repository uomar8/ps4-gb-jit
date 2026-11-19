#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "../hal.h"

#include <string.h>

AppResult hal_jit_alloc(HalJitArena *arena, size_t size) {
    if (!arena || size == 0 || (size % HAL_PAGE_SIZE) != 0) {
        return APP_ERR_INVALID_ARG;
    }
    memset(arena, 0, sizeof(*arena));
    arena->rx_fd = -1;
    arena->rw_fd = -1;

    DWORD hi = (DWORD)((uint64_t)size >> 32);
    DWORD lo = (DWORD)(size & 0xFFFFFFFFU);
    HANDLE hMap = CreateFileMappingA(INVALID_HANDLE_VALUE, NULL,
                                     PAGE_EXECUTE_READWRITE, hi, lo, NULL);
    if (!hMap) {
        return APP_ERR_JIT_UNAVAIL;
    }

    void *rw = MapViewOfFile(hMap, FILE_MAP_WRITE, 0, 0, size);
    if (!rw) {
        CloseHandle(hMap);
        return APP_ERR_NOMEM;
    }
    void *rx = MapViewOfFile(hMap, FILE_MAP_READ | FILE_MAP_EXECUTE, 0, 0, size);
    if (!rx) {
        UnmapViewOfFile(rw);
        CloseHandle(hMap);
        return APP_ERR_JIT_UNAVAIL;
    }

    arena->rx_base = rx;
    arena->rw_base = rw;
    arena->size = size;
    arena->platform_handle = (void *)hMap;
    return APP_OK;
}

void hal_jit_free(HalJitArena *arena) {
    if (!arena) {
        return;
    }
    if (arena->rx_base) {
        UnmapViewOfFile(arena->rx_base);
    }
    if (arena->rw_base) {
        UnmapViewOfFile(arena->rw_base);
    }
    if (arena->platform_handle) {
        CloseHandle((HANDLE)arena->platform_handle);
    }
    memset(arena, 0, sizeof(*arena));
    arena->rx_fd = -1;
    arena->rw_fd = -1;
}

void hal_jit_flush_icache(void *rx_addr, size_t size) {
    FlushInstructionCache(GetCurrentProcess(), rx_addr, size);
}
