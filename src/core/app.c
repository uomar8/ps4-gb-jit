#include "app.h"
#include "gb_machine.h"
#include "jit/jit_engine.h"
#include "../hal/hal.h"
#include "../debug/dbg_log.h"
#include "../debug/dbg_stub.h"
#include "../debug/dbg_crash.h"
#include "../debug/dbg_selftest.h"
#include "../ui/ui_render.h"
#include "../ui/ui_hud.h"
#include "../ui/ui_state.h"

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
        ui_state_save_config(cfg);
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

    LOG_INFO("SYS", "ps4-gb-jit booting (DMG-01 x86_64 JIT + HLE)");

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

    /* Allocate emulator & UI state on heap to keep PS4 stack frames << 32 KiB */
    GbMachine *gb = (GbMachine *)calloc(1, sizeof(GbMachine));
    UiState   *ui = (UiState *)calloc(1, sizeof(UiState));
    JitEngine jit;
    memset(&jit, 0, sizeof(jit));

    if (!gb || !ui) {
        LOG_ERROR("SYS", "OOM allocating GbMachine/UiState");
        free(gb);
        free(ui);
        return APP_ERR_NOMEM;
    }

    gb_machine_init(gb);
    bool want_jit = cfg.jit_enabled && !cfg.force_interp;
    (void)jit_engine_init(&jit, want_jit);

    ui_state_init(ui);
    ui_state_scan_roms(ui);

    hal_sys_hide_splash();

    HalInputState inp;
    memset(&inp, 0, sizeof(inp));
    uint32_t frame = 0;
    uint32_t fps = 60;
    uint32_t fps_frames = 0;
    uint64_t fps_t0 = hal_sys_get_time_us();

    LOG_STEP("app:main_loop");
    while (max_frames == 0 || frame < max_frames) {
        uint64_t frame_start = hal_sys_get_time_us();

        hal_input_poll(&inp);
        if (inp.quit_requested) {
            break;
        }

        ui_state_update(ui, &cfg, gb, &jit, &inp);

        /* Step Game Boy emulator when IN_GAME */
        if (ui->zone == UI_ZONE_IN_GAME && gb->rom_loaded) {
            int sub_frames = ui->fast_forward ? 4 : 1;
            for (int sf = 0; sf < sub_frames; ++sf) {
                gb->apu.out_pos = 0;
                jit_engine_run_frame(&jit, gb);
                if (sf == 0 && gb->apu.out_pos > 0) {
                    hal_audio_push_samples(gb->apu.out_buf, gb->apu.out_pos);
                }
            }
        }

        /* Render 1080p frame */
        int w = 0, h = 0, pitch = 0;
        uint32_t *fb = hal_video_get_backbuffer(&w, &h, &pitch);
        if (fb) {
            ui_state_render(ui, &cfg, gb, &jit, fb, w, h, pitch);

            if (ui->hud_visible) {
                UiHudMetrics m;
                m.focus_zone = ui_state_zone_name(ui->zone);
                m.active_modal = ui_state_modal_name(ui->zone);
                m.last_buttons = inp.buttons;
                m.jit_active = jit.enabled;
                m.jit_blocks = jit.blocks_compiled;
                m.jit_cache_kb = (uint32_t)(jit.write_cursor / 1024U);
                m.fps = fps;
                m.audio_fill_pct = hal_audio_get_fill_percent();
                ui_hud_render(fb, w, h, pitch, &m);
            }
            (void)hal_video_present();
        }

        frame++;
        fps_frames++;
        uint64_t now = hal_sys_get_time_us();
        if (now - fps_t0 >= 1000000ULL) {
            fps = fps_frames;
            fps_frames = 0;
            fps_t0 = now;
        }

        /* Frame pacing (~60 Hz) when not fast-forwarding and not in headless capped run */
        if (!ui->fast_forward && max_frames == 0) {
            uint64_t elapsed = now - frame_start;
            if (elapsed < 16600ULL) {
                hal_sys_sleep_us((uint32_t)(16600ULL - elapsed));
            }
        }
    }

    LOG_STEP("app:shutdown");
    if (gb->rom_loaded && gb->cart.has_battery && gb->cart.ram_dirty &&
        ui->active_sav_path[0] != '\0') {
        (void)cart_save_sram(&gb->cart, ui->active_sav_path);
    }

    jit_engine_shutdown(&jit);
    gb_machine_destroy(gb);
    free(ui);
    free(gb);

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
