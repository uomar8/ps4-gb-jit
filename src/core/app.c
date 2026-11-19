#include "app.h"
#include "../hal/hal.h"
#include "../debug/dbg_log.h"
#include "../debug/dbg_stub.h"
#include "../debug/dbg_crash.h"
#include "../debug/dbg_selftest.h"
#include "../ui/ui_render.h"
#include "../ui/ui_hud.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void app_load_config(AppConfig *cfg) {
    cfg->jit_enabled = true;
    cfg->force_interp = false;
    cfg->palette_index = 0;
    cfg->log_level = DBG_LVL_INFO;
    cfg->swap_ab = false;

    char cfg_path[APP_MAX_PATH];
    (void)snprintf(cfg_path, sizeof(cfg_path), "%s/config.ini", hal_fs_get_data_dir());

    uint8_t *buf = NULL;
    size_t sz = 0;
    if (hal_fs_read_file(cfg_path, &buf, &sz) != APP_OK) {
        /* Write default config.ini on first run */
        const char def[] =
            "# ps4-gb-jit config (editable via FTP :2121)\n"
            "jit_enabled=1\n"
            "force_interp=0\n"
            "palette=0\n"
            "swap_ab=0\n"
            "log_level=1\n";
        (void)hal_fs_write_file(cfg_path, (const uint8_t *)def, sizeof(def) - 1);
        return;
    }

    char *text = (char *)malloc(sz + 1);
    if (text) {
        memcpy(text, buf, sz);
        text[sz] = '\0';
        char *line = text;
        while (line && *line) {
            char *nl = strchr(line, '\n');
            if (nl) *nl = '\0';
            if (strncmp(line, "jit_enabled=", 12) == 0)  cfg->jit_enabled = (atoi(line + 12) != 0);
            if (strncmp(line, "force_interp=", 13) == 0) cfg->force_interp = (atoi(line + 13) != 0);
            if (strncmp(line, "palette=", 8) == 0)       cfg->palette_index = (uint32_t)atoi(line + 8);
            if (strncmp(line, "swap_ab=", 8) == 0)       cfg->swap_ab = (atoi(line + 8) != 0);
            if (strncmp(line, "log_level=", 10) == 0)    cfg->log_level = (uint32_t)atoi(line + 10);
            line = nl ? (nl + 1) : NULL;
        }
        free(text);
    }
    free(buf);
}

AppResult app_run(uint32_t max_frames) {
    dbg_crash_install();
    LOG_STEP("app:hal_sys_init");
    (void)hal_sys_init();

    /* Ensure /data/gbjit, roms, saves exist */
    char dir_path[APP_MAX_PATH];
    (void)hal_fs_mkdir_p(hal_fs_get_data_dir());
    (void)snprintf(dir_path, sizeof(dir_path), "%s/roms", hal_fs_get_data_dir());
    (void)hal_fs_mkdir_p(dir_path);
    (void)snprintf(dir_path, sizeof(dir_path), "%s/saves", hal_fs_get_data_dir());
    (void)hal_fs_mkdir_p(dir_path);

    char log_path[APP_MAX_PATH];
    (void)snprintf(log_path, sizeof(log_path), "%s/log.txt", hal_fs_get_data_dir());
    dbg_log_init(log_path, DBG_LVL_INFO);
    dbg_stub_reset();

    LOG_INFO("SYS", "ps4-gb-jit booting (Phase 1 Harness)");

    AppConfig cfg;
    app_load_config(&cfg);
    dbg_log_set_level((DbgLogLevel)cfg.log_level);

    LOG_STEP("app:init_subsystems");
    AppResult vrc = hal_video_init();
    AppResult arc = hal_audio_init();
    AppResult irc = hal_input_init();
    if (vrc != APP_OK) LOG_RC_ERROR("SYS", "hal_video_init failed", vrc);
    if (arc != APP_OK) LOG_RC_ERROR("SYS", "hal_audio_init failed", arc);
    if (irc != APP_OK) LOG_RC_ERROR("SYS", "hal_input_init failed", irc);

    DbgSelfTestReport report;
    bool selftest_ok = dbg_selftest_run(&report);

    char klog_path[APP_MAX_PATH];
    (void)snprintf(klog_path, sizeof(klog_path), "%s/klog.txt", hal_fs_get_data_dir());
    (void)hal_sys_snapshot_klog(klog_path);

    hal_sys_hide_splash();

    HalInputState inp;
    memset(&inp, 0, sizeof(inp));
    bool hud_visible = true;
    uint32_t frame = 0;

    LOG_STEP("app:main_loop");
    while (max_frames == 0 || frame < max_frames) {
        hal_input_poll(&inp);
        if (inp.quit_requested) {
            break;
        }

        uint32_t pressed = inp.buttons & ~inp.prev_buttons;
        bool l3r3_pressed = ((inp.buttons & (HAL_BTN_L3 | HAL_BTN_R3)) == (HAL_BTN_L3 | HAL_BTN_R3)) &&
                            ((inp.prev_buttons & (HAL_BTN_L3 | HAL_BTN_R3)) != (HAL_BTN_L3 | HAL_BTN_R3));
        if (l3r3_pressed || (pressed & HAL_BTN_HUD_KEY)) {
            hud_visible = !hud_visible;
        }

        int w = 0, h = 0, pitch = 0;
        uint32_t *fb = hal_video_get_backbuffer(&w, &h, &pitch);
        if (fb) {
            ui_render_clear(fb, w, h, pitch, 0xFF0F172AU);
            ui_render_text(fb, w, h, pitch, 64, 420, 4, 0xFF38BDF8U, "PS4 GAME BOY JIT + HLE");

            char summary[128];
            (void)snprintf(summary, sizeof(summary),
                           "PHASE 1 HARNESS | SELFTEST: %u/6 PASS (%s) | JIT: %s",
                           report.passed,
                           selftest_ok ? "OK" : "FAIL",
                           (report.jit_rwx_dualmap_ok && cfg.jit_enabled && !cfg.force_interp) ? "READY" : "INTERP FALLBACK");
            ui_render_text(fb, w, h, pitch, 64, 480, 3, selftest_ok ? 0xFF86EFACU : 0xFFFCA5A5U, summary);

            if (hud_visible) {
                UiHudMetrics m;
                m.focus_zone = "BOOT_HARNESS";
                m.active_modal = "NONE";
                m.last_buttons = inp.buttons;
                m.jit_active = report.jit_rwx_dualmap_ok && cfg.jit_enabled && !cfg.force_interp;
                m.jit_blocks = 0;
                m.jit_cache_kb = 0;
                m.fps = 60;
                m.audio_fill_pct = hal_audio_get_fill_percent();
                ui_hud_render(fb, w, h, pitch, &m);
            }
            (void)hal_video_present();
        }

        frame++;
        hal_sys_sleep_us(16000);
    }

    LOG_STEP("app:shutdown");
    char missing_path[APP_MAX_PATH];
    (void)snprintf(missing_path, sizeof(missing_path), "%s/missing_report.txt", hal_fs_get_data_dir());
    (void)dbg_stub_write_report(missing_path);
    (void)hal_sys_snapshot_klog(klog_path);

    hal_input_shutdown();
    hal_audio_shutdown();
    hal_video_shutdown();
    LOG_INFO("SYS", "ps4-gb-jit clean exit frames=%u", frame);
    dbg_log_shutdown();
    hal_sys_shutdown();

    return selftest_ok ? APP_OK : APP_ERR_SELFTEST_FAIL;
}
