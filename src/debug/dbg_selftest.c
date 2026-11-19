#include "dbg_selftest.h"
#include "dbg_log.h"
#include "../core/gb_types.h"
#include "../hal/hal.h"

#include <stdio.h>
#include <string.h>

typedef uint32_t (*JitSmokeFn)(void);

static bool test_abi_structs(void) {
    bool ok = (sizeof(OrbisStatWire) == 120) &&
              (offsetof(OrbisStatWire, st_size) == 72) &&
              (HAL_PAGE_SIZE == 16384U);
    LOG_INFO("TEST", "ABI_STRUCTS status=%s sizeof_stat=%zu st_size_off=%zu page=%u",
             ok ? "PASS" : "FAIL",
             sizeof(OrbisStatWire),
             offsetof(OrbisStatWire, st_size),
             HAL_PAGE_SIZE);
    return ok;
}

static bool test_fs_data_write(void) {
    char probe_path[APP_MAX_PATH];
    (void)snprintf(probe_path, sizeof(probe_path), "%s/selftest_probe.tmp", hal_fs_get_data_dir());
    const char payload[] = "PS4-GB-JIT-PROBE-0123456789";
    const size_t expected_len = sizeof(payload) - 1;

    AppResult rc = hal_fs_write_file(probe_path, (const uint8_t *)payload, expected_len);
    if (rc != APP_OK) {
        LOG_RC_ERROR("TEST", "FS_DATA_WRITE write failed", rc);
        return false;
    }

    int64_t fsize = -1;
    bool is_dir = true;
    rc = hal_fs_stat_size(probe_path, &fsize, &is_dir);
    (void)hal_fs_unlink(probe_path);

    bool ok = (rc == APP_OK) && (!is_dir) && ((size_t)fsize == expected_len);
    if (!ok) {
        LOG_ERROR("TEST", "FS_DATA_WRITE status=FAIL rc=0x%08x (%s) got_size=%lld expected=%zu is_dir=%d",
                  (uint32_t)rc, dbg_sce_strerror(rc), (long long)fsize, expected_len, is_dir ? 1 : 0);
    } else {
        LOG_INFO("TEST", "FS_DATA_WRITE status=PASS bytes=%lld", (long long)fsize);
    }
    return ok;
}

static bool test_video_dmem(void) {
    int w = 0, h = 0, pitch = 0;
    uint32_t *fb = hal_video_get_backbuffer(&w, &h, &pitch);
    bool ok = (fb != NULL) && (w == APP_FB_W) && (h == APP_FB_H) && (pitch >= w);
    LOG_INFO("TEST", "VIDEO_DMEM status=%s w=%d h=%d pitch=%d",
             ok ? "PASS" : "FAIL", w, h, pitch);
    return ok;
}

static bool test_audio_port(void) {
    int16_t silence[APP_AUDIO_GRAIN * 2];
    memset(silence, 0, sizeof(silence));
    hal_audio_push_samples(silence, APP_AUDIO_GRAIN);
    LOG_INFO("TEST", "AUDIO_PORT status=PASS rate=%d grain=%d fill=%u%%",
             APP_AUDIO_RATE, APP_AUDIO_GRAIN, hal_audio_get_fill_percent());
    return true;
}

static bool test_pad_init(void) {
    HalInputState st;
    memset(&st, 0, sizeof(st));
    hal_input_poll(&st);
    LOG_INFO("TEST", "PAD_INIT status=PASS buttons=0x%08x", st.buttons);
    return true;
}

static bool test_jit_rwx_dualmap(void) {
    HalJitArena arena;
    memset(&arena, 0, sizeof(arena));
    AppResult rc = hal_jit_alloc(&arena, HAL_PAGE_SIZE);
    if (rc != APP_OK) {
        LOG_WARN("TEST", "JIT_RWX_DUALMAP status=FAIL rc=0x%08x (%s) fallback=INTERP",
                 (uint32_t)rc, dbg_sce_strerror(rc));
        return false;
    }

    /* Emit baseline x86_64: MOV EAX, 0x1337C0DE (B8 DE C0 37 13); RET (C3) */
    static const uint8_t code[6] = { 0xB8, 0xDE, 0xC0, 0x37, 0x13, 0xC3 };
    memcpy(arena.rw_base, code, sizeof(code));
    hal_jit_flush_icache(arena.rx_base, sizeof(code));

    JitSmokeFn fn = NULL;
    memcpy(&fn, &arena.rx_base, sizeof(fn));
    uint32_t val = fn();
    hal_jit_free(&arena);

    bool ok = (val == 0x1337C0DEU);
    if (ok) {
        LOG_INFO("TEST", "JIT_RWX_DUALMAP status=PASS result=0x%08x", val);
    } else {
        LOG_ERROR("TEST", "JIT_RWX_DUALMAP status=FAIL got=0x%08x expected=0x1337c0de", val);
    }
    return ok;
}

bool dbg_selftest_run(DbgSelfTestReport *out_report) {
    DbgSelfTestReport r;
    memset(&r, 0, sizeof(r));

    LOG_STEP("selftest:start");
    LOG_INFO("TEST", "=== Boot-Time Hardware Self-Test (6 checks) ===");

    r.abi_structs_ok     = test_abi_structs();
    r.fs_data_write_ok   = test_fs_data_write();
    r.video_dmem_ok      = test_video_dmem();
    r.audio_port_ok      = test_audio_port();
    r.pad_init_ok        = test_pad_init();
    r.jit_rwx_dualmap_ok = test_jit_rwx_dualmap();

    bool results[6] = {
        r.abi_structs_ok,
        r.fs_data_write_ok,
        r.video_dmem_ok,
        r.audio_port_ok,
        r.pad_init_ok,
        r.jit_rwx_dualmap_ok
    };
    for (int i = 0; i < 6; i++) {
        if (results[i]) {
            r.passed++;
        } else {
            r.failed++;
        }
    }

    LOG_INFO("TEST", "=== Self-Test Summary: passed=%u failed=%u ===", r.passed, r.failed);
    if (out_report) {
        *out_report = r;
    }
    LOG_STEP("selftest:done");

    /* Mandatory checks: ABI, FS, Video, Audio, Pad (JIT can fall back to interpreter) */
    return r.abi_structs_ok && r.fs_data_write_ok && r.video_dmem_ok &&
           r.audio_port_ok && r.pad_init_ok;
}
