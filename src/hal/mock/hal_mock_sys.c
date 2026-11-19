#define _POSIX_C_SOURCE 200809L
#include "../hal.h"
#include "hal_mock_ctl.h"

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

AppResult hal_sys_init(void) {
    return APP_OK;
}

void hal_sys_shutdown(void) {
}

void hal_sys_hide_splash(void) {
}

void hal_sys_exit_clean(int code) {
    exit(code);
}

uint64_t hal_sys_get_time_us(void) {
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
        return 0;
    }
    return (uint64_t)ts.tv_sec * 1000000ULL + (uint64_t)(ts.tv_nsec / 1000L);
}

void hal_sys_sleep_us(uint32_t us) {
    /* Headless mock runs as fast as possible unless explicitly delayed */
    (void)us;
}

void hal_sys_klog_write(const char *line) {
    if (line) {
        (void)fputs(line, stderr);
    }
}

AppResult hal_sys_snapshot_klog(const char *dest_path) {
    const char msg[] = "[MOCK] klog snapshot OK\n";
    return hal_fs_write_file(dest_path, (const uint8_t *)msg, sizeof(msg) - 1);
}
