#include "../../src/core/app.h"
#include "../../src/core/gb_types.h"
#include "../../src/core/result.h"
#include "../../src/debug/dbg_log.h"
#include "../../src/debug/dbg_sce_err.h"
#include "../../src/debug/dbg_stub.h"
#include "../../src/debug/dbg_selftest.h"
#include "../../src/hal/hal.h"
#include "../../src/hal/mock/hal_mock_ctl.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void test_abi_and_err_decoder(void) {
    assert(sizeof(OrbisStatWire) == 120);
    assert(offsetof(OrbisStatWire, st_size) == 72);
    assert(offsetof(OrbisStatWire, st_blocks) == 80);
    assert(sizeof(GbSaveStateHeader) == 64);

    assert(strcmp(dbg_sce_strerror(0), "OK") == 0);
    assert(strcmp(dbg_sce_strerror((int32_t)0x8002000DU), "SCE_KERNEL_ERROR_EACCES") == 0);
    assert(strcmp(dbg_sce_strerror((int32_t)0x80290013U), "SCE_VIDEO_OUT_ERROR_INVALID_MEMORY") == 0);
    assert(strcmp(dbg_sce_strerror((int32_t)0x80260008U), "SCE_AUDIO_OUT_ERROR_INVALID_SAMPLE_RATE") == 0);
    assert(strcmp(dbg_sce_strerror(APP_ERR_JIT_UNAVAIL), "APP_ERR_JIT_UNAVAIL") == 0);
}

static void test_stub_discovery_sorted(const char *data_dir) {
    dbg_stub_reset();
    dbg_stub_record("MMIO", 0xFF4D, "KEY1_GBC_SPEED");
    dbg_stub_record("MMIO", 0xFF4F, "VBK_GBC_VRAM");
    dbg_stub_record("MMIO", 0xFF4F, "VBK_GBC_VRAM");
    dbg_stub_record("MMIO", 0xFF4F, "VBK_GBC_VRAM");
    dbg_stub_record("MBC",  0x001E, "MBC6_UNSUPPORTED");
    assert(dbg_stub_get_count() == 3);

    char path[APP_MAX_PATH];
    (void)snprintf(path, sizeof(path), "%s/test_missing_report.txt", data_dir);
    assert(dbg_stub_write_report(path) == APP_OK);

    uint8_t *buf = NULL;
    size_t sz = 0;
    assert(hal_fs_read_file(path, &buf, &sz) == APP_OK);
    char *txt = (char *)malloc(sz + 1);
    assert(txt != NULL);
    memcpy(txt, buf, sz);
    txt[sz] = '\0';

    /* Highest hit count (3 | MMIO | 0xff4f) must appear before 1-hit entries */
    char *pos_ff4f = strstr(txt, "3 | MMIO | 0xff4f");
    char *pos_ff4d = strstr(txt, "1 | MMIO | 0xff4d");
    assert(pos_ff4f != NULL);
    assert(pos_ff4d != NULL);
    assert(pos_ff4f < pos_ff4d);

    free(txt);
    free(buf);
}

static void test_mock_orbis_fault_injection(const char *data_dir) {
    /* 1. Read-only /app0/ must reject writes */
    char ro_path[APP_MAX_PATH];
    (void)snprintf(ro_path, sizeof(ro_path), "%s/forbidden.bin", hal_fs_get_app_dir());
    const uint8_t b[4] = {1, 2, 3, 4};
    assert(hal_fs_write_file(ro_path, b, sizeof(b)) != APP_OK);

    /* 2. DT_WHT=14 / d_fileno==0 ghosts must be filtered out */
    hal_mock_fs_set_inject_ghosts(true);
    char roms_dir[APP_MAX_PATH];
    (void)snprintf(roms_dir, sizeof(roms_dir), "%s/roms", data_dir);
    assert(hal_fs_mkdir_p(roms_dir) == APP_OK);
    char rom_file[APP_MAX_PATH + 32];
    (void)snprintf(rom_file, sizeof(rom_file), "%s/real_game.gb", roms_dir);
    assert(hal_fs_write_file(rom_file, b, sizeof(b)) == APP_OK);

    HalDirEntry entries[16];
    size_t count = 0;
    assert(hal_fs_list_dir(roms_dir, entries, 16, &count) == APP_OK);
    assert(count == 1);
    assert(strcmp(entries[0].name, "real_game.gb") == 0);
    hal_mock_fs_set_inject_ghosts(false);

    /* 3. JIT EACCES fault injection -> clean APP_ERR_JIT_UNAVAIL fallback */
    hal_mock_jit_set_fail_eacces(true);
    HalJitArena arena;
    assert(hal_jit_alloc(&arena, HAL_PAGE_SIZE) == APP_ERR_JIT_UNAVAIL);
    hal_mock_jit_set_fail_eacces(false);

    /* 4. Unaligned JIT size (< 16 KiB) must be rejected */
    assert(hal_jit_alloc(&arena, 4096) == APP_ERR_INVALID_ARG);
}

static void test_app_harness_e2e_walkthrough(const char *data_dir) {
    hal_mock_input_reset();
    /* Toggle HUD off and back on via L3+R3, then quit cleanly */
    hal_mock_input_push(0, 2);
    hal_mock_input_push(HAL_BTN_L3 | HAL_BTN_R3, 1);
    hal_mock_input_push(0, 2);
    hal_mock_input_push(HAL_BTN_L3 | HAL_BTN_R3, 1);
    hal_mock_input_push(0, 2);
    hal_mock_input_push_quit();

    AppResult rc = app_run(30);
    assert(rc == APP_OK);
    assert(dbg_log_get_error_count() == 0);
    assert(hal_mock_video_present_count() >= 5);
    assert(hal_mock_audio_total_frames() >= APP_AUDIO_GRAIN);

    /* Verify /data/gbjit/ diagnostic artifacts were created */
    char p[APP_MAX_PATH];
    int64_t sz = 0;
    bool is_dir = false;
    (void)snprintf(p, sizeof(p), "%s/log.txt", data_dir);
    assert(hal_fs_stat_size(p, &sz, &is_dir) == APP_OK && sz > 0 && !is_dir);
    (void)snprintf(p, sizeof(p), "%s/klog.txt", data_dir);
    assert(hal_fs_stat_size(p, &sz, &is_dir) == APP_OK && sz > 0 && !is_dir);
    (void)snprintf(p, sizeof(p), "%s/missing_report.txt", data_dir);
    assert(hal_fs_stat_size(p, &sz, &is_dir) == APP_OK && sz > 0 && !is_dir);
    (void)snprintf(p, sizeof(p), "%s/config.ini", data_dir);
    assert(hal_fs_stat_size(p, &sz, &is_dir) == APP_OK && sz > 0 && !is_dir);
}

int main(void) {
    const char *data_dir = "/tmp/ps4_gb_jit_test_phase1";
    hal_mock_fs_set_data_dir(data_dir);
    hal_mock_fs_set_app_dir("/tmp/ps4_gb_jit_test_app0");
    assert(hal_fs_mkdir_p(data_dir) == APP_OK);

    test_abi_and_err_decoder();
    test_stub_discovery_sorted(data_dir);
    test_mock_orbis_fault_injection(data_dir);
    test_app_harness_e2e_walkthrough(data_dir);

    printf("[PASS] test_phase1_harness: all Phase 1 harness, ABI, JIT RW/RX, Stub-Discovery, and Mock-Orbis tests passed!\n");
    return 0;
}
