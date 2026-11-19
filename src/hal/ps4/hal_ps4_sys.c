#include "../hal.h"
#include "../../debug/dbg_log.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>

#include <orbis/libkernel.h>
#include <orbis/Sysmodule.h>
#include <orbis/SystemService.h>

AppResult hal_sys_init(void) {
    /* Load internal firmware sysmodules BEFORE calling UserService/Pad/VideoOut/AudioOut! */
    (void)sceSysmoduleLoadModuleInternal(ORBIS_SYSMODULE_INTERNAL_SYSTEM_SERVICE);
    (void)sceSysmoduleLoadModuleInternal(ORBIS_SYSMODULE_INTERNAL_USER_SERVICE);
    (void)sceSysmoduleLoadModuleInternal(ORBIS_SYSMODULE_INTERNAL_PAD);
    (void)sceSysmoduleLoadModuleInternal(ORBIS_SYSMODULE_INTERNAL_VIDEO_OUT);
    (void)sceSysmoduleLoadModuleInternal(ORBIS_SYSMODULE_INTERNAL_AUDIOOUT);
    return APP_OK;
}

void hal_sys_shutdown(void) {
}

void hal_sys_hide_splash(void) {
    (void)sceSystemServiceHideSplashScreen();
}

void hal_sys_exit_clean(int code) {
    (void)code;
    /* Never terminate via raw exit syscall on retail Orbis (raises SIGSYS). */
    (void)sceSystemServiceLoadExec("exit", NULL);
    for (;;) {
        (void)sceKernelUsleep(100000);
    }
}

uint64_t hal_sys_get_time_us(void) {
    return (uint64_t)sceKernelGetProcessTime();
}

void hal_sys_sleep_us(uint32_t us) {
    (void)sceKernelUsleep(us);
}

void hal_sys_klog_write(const char *line) {
    if (line) {
        (void)sceKernelDebugOutText(0, line);
    }
}

AppResult hal_sys_snapshot_klog(const char *dest_path) {
    if (!dest_path) {
        return APP_ERR_INVALID_ARG;
    }
    int kfd = open("/dev/klog", O_RDONLY | O_NONBLOCK);
    if (kfd < 0) {
        const char msg[] = "[PS4] /dev/klog unavailable (non-fatal)\n";
        return hal_fs_write_file(dest_path, (const uint8_t *)msg, sizeof(msg) - 1);
    }

    uint8_t buf[16384];
    ssize_t n = read(kfd, buf, sizeof(buf));
    close(kfd);
    if (n <= 0) {
        const char msg[] = "[PS4] /dev/klog empty\n";
        return hal_fs_write_file(dest_path, (const uint8_t *)msg, sizeof(msg) - 1);
    }
    return hal_fs_write_file(dest_path, buf, (size_t)n);
}
