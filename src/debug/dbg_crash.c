#if defined(__linux__) && !defined(_GNU_SOURCE)
#define _GNU_SOURCE
#endif

#include "dbg_crash.h"
#include "dbg_log.h"
#include "../hal/hal.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <io.h>

static LONG WINAPI dbg_win_exception_handler(EXCEPTION_POINTERS *ep) {
    uint64_t rip = 0;
    uint64_t rsp = 0;
    uint32_t code = 0;
    if (ep && ep->ContextRecord) {
        rip = (uint64_t)ep->ContextRecord->Rip;
        rsp = (uint64_t)ep->ContextRecord->Rsp;
    }
    if (ep && ep->ExceptionRecord) {
        code = (uint32_t)ep->ExceptionRecord->ExceptionCode;
    }
    char msg[256];
    int n = snprintf(msg, sizeof(msg),
                     "[CRASH] [FATAL] exception=0x%08x rip=0x%016llx rsp=0x%016llx\n",
                     code, (unsigned long long)rip, (unsigned long long)rsp);
    if (n > 0) {
        hal_sys_klog_write(msg);
        int fd = dbg_log_get_fd();
        if (fd >= 0) {
            (void)_write(fd, msg, (unsigned int)n);
            (void)_commit(fd);
        }
    }
    return EXCEPTION_EXECUTE_HANDLER;
}

void dbg_crash_install(void) {
    SetUnhandledExceptionFilter(dbg_win_exception_handler);
}

#else /* POSIX / Linux / PS4 Orbis */

#include <signal.h>
#include <unistd.h>

#if defined(__linux__)
#include <ucontext.h>
#endif

/*
 * OpenOrbis musl ucontext_t layout does NOT match the FreeBSD/Orbis kernel ucontext.
 * Read mc_rip at byte offset 224 (0xE0) and mc_rsp at byte offset 248 (0xF8) directly.
 */
#define ORBIS_UCONTEXT_RIP_OFFSET 224U
#define ORBIS_UCONTEXT_RSP_OFFSET 248U

static void dbg_posix_signal_handler(int sig, siginfo_t *info, void *ucontext) {
    uint64_t rip = 0;
    uint64_t rsp = 0;
    uint64_t fault_addr = info ? (uint64_t)(uintptr_t)info->si_addr : 0;

    if (ucontext) {
#if defined(__ORBIS__) || defined(__PS4__)
        const uint8_t *raw = (const uint8_t *)ucontext;
        memcpy(&rip, raw + ORBIS_UCONTEXT_RIP_OFFSET, sizeof(rip));
        memcpy(&rsp, raw + ORBIS_UCONTEXT_RSP_OFFSET, sizeof(rsp));
#elif defined(__linux__) && defined(__x86_64__)
        const ucontext_t *uc = (const ucontext_t *)ucontext;
        rip = (uint64_t)uc->uc_mcontext.gregs[REG_RIP];
        rsp = (uint64_t)uc->uc_mcontext.gregs[REG_RSP];
#endif
    }

    char line[256];
    int n = snprintf(line, sizeof(line),
                     "[CRASH] [FATAL] sig=%d fault=0x%016llx mc_rip=0x%016llx mc_rsp=0x%016llx\n",
                     sig,
                     (unsigned long long)fault_addr,
                     (unsigned long long)rip,
                     (unsigned long long)rsp);
    int fd = dbg_log_get_fd();
    if (n > 0) {
        hal_sys_klog_write(line);
        if (fd >= 0) {
            (void)write(fd, line, (size_t)n);
        }
    }

    char steps[DBG_STEP_RING_CAP][DBG_STEP_STR_MAX];
    size_t sc = dbg_log_get_steps(steps, DBG_STEP_RING_CAP);
    for (size_t i = 0; i < sc; i++) {
        int sn = snprintf(line, sizeof(line), "[CRASH] [STEP %zu] %s\n", i, steps[i]);
        if (sn > 0) {
            hal_sys_klog_write(line);
            if (fd >= 0) {
                (void)write(fd, line, (size_t)sn);
            }
        }
    }

    if (fd >= 0) {
        (void)fsync(fd);
    }

    hal_sys_exit_clean(128 + sig);
}

void dbg_crash_install(void) {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
#if defined(__ORBIS__) || defined(__PS4__)
    /* OpenOrbis v0.5.4 signal.h macro typo: sa_sigaction expands to __sa_handler.sa_sigaction */
    sa.__sa_handler.__sa_sigaction = dbg_posix_signal_handler;
#else
    sa.sa_sigaction = dbg_posix_signal_handler;
#endif
    sa.sa_flags = SA_SIGINFO;
    sigemptyset(&sa.sa_mask);

    (void)sigaction(SIGSEGV, &sa, NULL);
    (void)sigaction(SIGBUS,  &sa, NULL);
    (void)sigaction(SIGILL,  &sa, NULL);
    (void)sigaction(SIGABRT, &sa, NULL);
    (void)sigaction(SIGFPE,  &sa, NULL);
#ifdef SIGSYS
    (void)signal(SIGSYS, SIG_IGN);
#endif
}

#endif
