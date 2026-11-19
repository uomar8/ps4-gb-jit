#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "../hal.h"

#include <stdio.h>
#include <stdlib.h>

static LARGE_INTEGER g_qpc_freq;

AppResult hal_sys_init(void) {
    QueryPerformanceFrequency(&g_qpc_freq);
    return APP_OK;
}

void hal_sys_shutdown(void) {
}

void hal_sys_hide_splash(void) {
}

void hal_sys_exit_clean(int code) {
    ExitProcess((UINT)code);
}

uint64_t hal_sys_get_time_us(void) {
    LARGE_INTEGER now;
    if (g_qpc_freq.QuadPart == 0) {
        QueryPerformanceFrequency(&g_qpc_freq);
    }
    QueryPerformanceCounter(&now);
    return (uint64_t)((now.QuadPart * 1000000LL) / (g_qpc_freq.QuadPart ? g_qpc_freq.QuadPart : 1LL));
}

void hal_sys_sleep_us(uint32_t us) {
    DWORD ms = us / 1000U;
    Sleep(ms > 0 ? ms : 1);
}

void hal_sys_klog_write(const char *line) {
    if (line) {
        OutputDebugStringA(line);
        (void)fputs(line, stderr);
    }
}

AppResult hal_sys_snapshot_klog(const char *dest_path) {
    const char msg[] = "[WIN64] klog snapshot (OutputDebugString)\n";
    return hal_fs_write_file(dest_path, (const uint8_t *)msg, sizeof(msg) - 1);
}
