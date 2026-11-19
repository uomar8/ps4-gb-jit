#include "ui_state.h"
#include "ui_render.h"
#include "../core/hle/ppu.h"
#include "../debug/dbg_log.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool has_gb_ext(const char *name) {
    size_t n = strlen(name);
    if (n < 3) return false;
    return (name[n - 3] == '.' &&
            tolower((unsigned char)name[n - 2]) == 'g' &&
            tolower((unsigned char)name[n - 1]) == 'b');
}

static int cmp_rom_entry(const void *a, const void *b) {
    const UiRomEntry *ra = (const UiRomEntry *)a;
    const UiRomEntry *rb = (const UiRomEntry *)b;
    return strcmp(ra->name, rb->name);
}

static void ui_toast(UiState *ui, const char *msg) {
    (void)snprintf(ui->toast_msg, sizeof(ui->toast_msg), "%s", msg);
    ui->toast_frames = 120;
}

static void ui_show_error(UiState *ui, const char *msg) {
    (void)snprintf(ui->error_msg, sizeof(ui->error_msg), "%s", msg);
    ui->zone = UI_ZONE_ERROR_MODAL;
    LOG_ERROR("UI", "error modal: %s", msg);
}

static void ui_flush_sram_if_dirty(GbMachine *gb, const UiState *ui) {
    if (gb->rom_loaded && gb->cart.has_battery && gb->cart.ram_dirty &&
        ui->active_sav_path[0] != '\0') {
        AppResult rc = cart_save_sram(&gb->cart, ui->active_sav_path);
        if (rc != APP_OK) {
            LOG_RC_ERROR("UI", "SRAM auto-save failed", rc);
        }
    }
}

void ui_state_save_config(const AppConfig *cfg) {
    char cfg_path[APP_MAX_PATH];
    (void)snprintf(cfg_path, sizeof(cfg_path), "%s/config.ini", hal_fs_get_data_dir());
    char buf[256];
    int n = snprintf(buf, sizeof(buf),
                     "# ps4-gb-jit config (editable via FTP :2121)\n"
                     "jit_enabled=%d\n"
                     "force_interp=%d\n"
                     "palette=%u\n"
                     "swap_ab=%d\n"
                     "log_level=%u\n",
                     cfg->jit_enabled ? 1 : 0,
                     cfg->force_interp ? 1 : 0,
                     (unsigned)cfg->palette_index,
                     cfg->swap_ab ? 1 : 0,
                     (unsigned)cfg->log_level);
    if (n > 0) {
        (void)hal_fs_write_file(cfg_path, (const uint8_t *)buf, (size_t)n);
    }
}

void ui_state_init(UiState *ui) {
    memset(ui, 0, sizeof(*ui));
    ui->zone = UI_ZONE_ROM_BROWSER;
    ui->settings_return_zone = UI_ZONE_ROM_BROWSER;
    ui->hud_visible = false;
}

void ui_state_scan_roms(UiState *ui) {
    char roms_dir[APP_MAX_PATH];
    (void)snprintf(roms_dir, sizeof(roms_dir), "%s/roms", hal_fs_get_data_dir());

    HalDirEntry *entries = (HalDirEntry *)calloc(UI_MAX_ROMS, sizeof(HalDirEntry));
    if (!entries) return;

    size_t count = 0;
    ui->rom_count = 0;
    if (hal_fs_list_dir(roms_dir, entries, UI_MAX_ROMS, &count) == APP_OK) {
        for (size_t i = 0; i < count && ui->rom_count < UI_MAX_ROMS; ++i) {
            if (!entries[i].is_dir && has_gb_ext(entries[i].name)) {
                UiRomEntry *dst = &ui->roms[ui->rom_count++];
                (void)snprintf(dst->name, sizeof(dst->name), "%.127s", entries[i].name);
                (void)snprintf(dst->path, sizeof(dst->path), "%.300s/%.127s", roms_dir, entries[i].name);
            }
        }
    }
    free(entries);

    if (ui->rom_count > 1) {
        qsort(ui->roms, ui->rom_count, sizeof(UiRomEntry), cmp_rom_entry);
    }
    if (ui->rom_selected >= (int)ui->rom_count) {
        ui->rom_selected = ui->rom_count > 0 ? (int)ui->rom_count - 1 : 0;
    }
    if (ui->rom_selected < 0) ui->rom_selected = 0;
    LOG_INFO("UI", "scanned %s: found %zu .gb ROM(s)", roms_dir, ui->rom_count);
}

const char *ui_state_zone_name(UiFocusZone zone) {
    switch (zone) {
        case UI_ZONE_ROM_BROWSER:    return "ROM_BROWSER";
        case UI_ZONE_IN_GAME:        return "IN_GAME";
        case UI_ZONE_PAUSE_MODAL:    return "PAUSE_MODAL";
        case UI_ZONE_SETTINGS_MODAL: return "SETTINGS_MODAL";
        case UI_ZONE_ERROR_MODAL:    return "ERROR_MODAL";
        default:                     return "UNKNOWN";
    }
}

const char *ui_state_modal_name(UiFocusZone zone) {
    switch (zone) {
        case UI_ZONE_PAUSE_MODAL:    return "PAUSE";
        case UI_ZONE_SETTINGS_MODAL: return "SETTINGS";
        case UI_ZONE_ERROR_MODAL:    return "ERROR";
        default:                     return "NONE";
    }
}

static void launch_selected_rom(UiState *ui, GbMachine *gb, JitEngine *jit) {
    if (ui->rom_count == 0 || ui->rom_selected < 0 ||
        (size_t)ui->rom_selected >= ui->rom_count) {
        return;
    }
    const UiRomEntry *e = &ui->roms[ui->rom_selected];
    AppResult rc = gb_machine_load_rom_file(gb, e->path);
    if (rc != APP_OK) {
        char msg[192];
        (void)snprintf(msg, sizeof(msg), "FAILED TO LOAD ROM: %s", e->name);
        ui_show_error(ui, msg);
        return;
    }

    jit_engine_flush(jit);
    (void)snprintf(ui->active_rom_name, sizeof(ui->active_rom_name), "%s", e->name);
    (void)snprintf(ui->active_sav_path, sizeof(ui->active_sav_path),
                   "%s/saves/%s.sav", hal_fs_get_data_dir(), e->name);
    (void)snprintf(ui->active_state_path, sizeof(ui->active_state_path),
                   "%s/saves/%s.st0", hal_fs_get_data_dir(), e->name);

    if (gb->cart.has_battery) {
        (void)cart_load_sram(&gb->cart, ui->active_sav_path);
    }

    ui->zone = UI_ZONE_IN_GAME;
    ui->pause_selected = 0;
    ui_toast(ui, "ROM LOADED");
    LOG_INFO("UI", "launched ROM name=%s title=%s", e->name, gb->cart.title);
}

void ui_state_update(UiState *ui, AppConfig *cfg, GbMachine *gb,
                     JitEngine *jit, const HalInputState *inp) {
    if (ui->toast_frames > 0) ui->toast_frames--;

    uint32_t pressed = inp->buttons & ~inp->prev_buttons;

    /* Global HUD toggle: L3+R3 or F12 */
    bool l3r3_pressed = ((inp->buttons & (HAL_BTN_L3 | HAL_BTN_R3)) == (HAL_BTN_L3 | HAL_BTN_R3)) &&
                        ((inp->prev_buttons & (HAL_BTN_L3 | HAL_BTN_R3)) != (HAL_BTN_L3 | HAL_BTN_R3));
    if (l3r3_pressed || (pressed & HAL_BTN_HUD_KEY)) {
        ui->hud_visible = !ui->hud_visible;
        LOG_INFO("UI", "hud_visible=%d", ui->hud_visible ? 1 : 0);
    }

    ui->fast_forward = false;

    switch (ui->zone) {
        case UI_ZONE_ROM_BROWSER:
            if (pressed & HAL_BTN_UP) {
                if (ui->rom_count > 0) {
                    ui->rom_selected = (ui->rom_selected > 0) ? (ui->rom_selected - 1) : ((int)ui->rom_count - 1);
                }
            } else if (pressed & HAL_BTN_DOWN) {
                if (ui->rom_count > 0) {
                    ui->rom_selected = (ui->rom_selected + 1) % (int)ui->rom_count;
                }
            } else if (pressed & HAL_BTN_TRIANGLE) {
                ui_state_scan_roms(ui);
                ui_toast(ui, "ROM LIST REFRESHED");
            } else if (pressed & HAL_BTN_SQUARE) {
                ui->settings_return_zone = UI_ZONE_ROM_BROWSER;
                ui->settings_selected = 0;
                ui->zone = UI_ZONE_SETTINGS_MODAL;
            } else if (pressed & HAL_BTN_CROSS) {
                launch_selected_rom(ui, gb, jit);
            }
            break;

        case UI_ZONE_IN_GAME:
            if (pressed & HAL_BTN_OPTIONS) {
                ui_flush_sram_if_dirty(gb, ui);
                ui->pause_selected = 0;
                ui->zone = UI_ZONE_PAUSE_MODAL;
                LOG_INFO("UI", "entered PAUSE_MODAL");
                break;
            }
            if (pressed & HAL_BTN_R2) {
                if (gb_machine_save_state(gb, ui->active_state_path) == APP_OK) {
                    ui_toast(ui, "QUICK SAVED (SLOT 0)");
                } else {
                    ui_toast(ui, "QUICK SAVE FAILED");
                }
            }
            if (pressed & HAL_BTN_L1) {
                if (gb_machine_load_state(gb, ui->active_state_path) == APP_OK) {
                    jit_engine_flush(jit);
                    ui_toast(ui, "QUICK LOADED (SLOT 0)");
                } else {
                    ui_toast(ui, "NO SAVE STATE IN SLOT 0");
                }
            }
            ui->fast_forward = (inp->buttons & HAL_BTN_L2) != 0;

            /* Map DS4 -> DMG Joypad (NO Share binding: Touchpad=SELECT, R1=START) */
            {
                uint8_t keys = 0;
                if (inp->buttons & HAL_BTN_RIGHT)    keys |= GB_KEY_RIGHT;
                if (inp->buttons & HAL_BTN_LEFT)     keys |= GB_KEY_LEFT;
                if (inp->buttons & HAL_BTN_UP)       keys |= GB_KEY_UP;
                if (inp->buttons & HAL_BTN_DOWN)     keys |= GB_KEY_DOWN;
                if (cfg->swap_ab) {
                    if (inp->buttons & HAL_BTN_SQUARE) keys |= GB_KEY_A;
                    if (inp->buttons & HAL_BTN_CROSS)  keys |= GB_KEY_B;
                } else {
                    if (inp->buttons & HAL_BTN_CROSS)  keys |= GB_KEY_A;
                    if (inp->buttons & HAL_BTN_SQUARE) keys |= GB_KEY_B;
                }
                if (inp->buttons & HAL_BTN_TOUCHPAD) keys |= GB_KEY_SELECT;
                if (inp->buttons & HAL_BTN_R1)       keys |= GB_KEY_START;
                gb_machine_set_keys(gb, keys);
            }
            break;

        case UI_ZONE_PAUSE_MODAL:
            if (pressed & HAL_BTN_UP) {
                ui->pause_selected = (ui->pause_selected + 5) % 6;
            } else if (pressed & HAL_BTN_DOWN) {
                ui->pause_selected = (ui->pause_selected + 1) % 6;
            } else if ((pressed & HAL_BTN_CIRCLE) || (pressed & HAL_BTN_OPTIONS)) {
                ui->zone = UI_ZONE_IN_GAME;
            } else if (pressed & HAL_BTN_CROSS) {
                switch (ui->pause_selected) {
                    case 0: /* RESUME */
                        ui->zone = UI_ZONE_IN_GAME;
                        break;
                    case 1: /* SAVE STATE */
                        if (gb_machine_save_state(gb, ui->active_state_path) == APP_OK) {
                            ui_toast(ui, "SAVED STATE (SLOT 0)");
                        } else {
                            ui_toast(ui, "SAVE STATE FAILED");
                        }
                        ui->zone = UI_ZONE_IN_GAME;
                        break;
                    case 2: /* LOAD STATE */
                        if (gb_machine_load_state(gb, ui->active_state_path) == APP_OK) {
                            jit_engine_flush(jit);
                            ui_toast(ui, "LOADED STATE (SLOT 0)");
                        } else {
                            ui_toast(ui, "NO SAVE STATE FOUND");
                        }
                        ui->zone = UI_ZONE_IN_GAME;
                        break;
                    case 3: /* SETTINGS */
                        ui->settings_return_zone = UI_ZONE_PAUSE_MODAL;
                        ui->settings_selected = 0;
                        ui->zone = UI_ZONE_SETTINGS_MODAL;
                        break;
                    case 4: /* RESET GAME */
                        gb_machine_reset(gb);
                        jit_engine_flush(jit);
                        ui_toast(ui, "GAME RESET");
                        ui->zone = UI_ZONE_IN_GAME;
                        break;
                    case 5: /* EXIT TO ROM BROWSER */
                        ui_flush_sram_if_dirty(gb, ui);
                        ui->zone = UI_ZONE_ROM_BROWSER;
                        ui_state_scan_roms(ui);
                        break;
                }
            }
            break;

        case UI_ZONE_SETTINGS_MODAL:
            if (pressed & HAL_BTN_UP) {
                ui->settings_selected = (ui->settings_selected + 3) % 4;
            } else if (pressed & HAL_BTN_DOWN) {
                ui->settings_selected = (ui->settings_selected + 1) % 4;
            } else if (pressed & HAL_BTN_CIRCLE) {
                ui_state_save_config(cfg);
                ui->zone = ui->settings_return_zone;
            } else if ((pressed & HAL_BTN_CROSS) || (pressed & HAL_BTN_LEFT) || (pressed & HAL_BTN_RIGHT)) {
                switch (ui->settings_selected) {
                    case 0: /* CPU ENGINE */
                        cfg->jit_enabled = !cfg->jit_enabled;
                        cfg->force_interp = !cfg->jit_enabled;
                        jit_engine_set_enabled(jit, cfg->jit_enabled);
                        ui_state_save_config(cfg);
                        break;
                    case 1: /* PALETTE */
                        cfg->palette_index = (cfg->palette_index + 1U) % GB_PALETTE_COUNT;
                        ui_state_save_config(cfg);
                        break;
                    case 2: /* SWAP A/B */
                        cfg->swap_ab = !cfg->swap_ab;
                        ui_state_save_config(cfg);
                        break;
                    case 3: /* BACK */
                        ui_state_save_config(cfg);
                        ui->zone = ui->settings_return_zone;
                        break;
                }
            }
            break;

        case UI_ZONE_ERROR_MODAL:
            if ((pressed & HAL_BTN_CROSS) || (pressed & HAL_BTN_CIRCLE)) {
                ui->zone = UI_ZONE_ROM_BROWSER;
            }
            break;
    }
}

void ui_state_render(const UiState *ui, const AppConfig *cfg,
                     const GbMachine *gb, const JitEngine *jit,
                     uint32_t *fb, int w, int h, int pitch) {
    if (ui->zone == UI_ZONE_ROM_BROWSER ||
        (ui->zone == UI_ZONE_SETTINGS_MODAL && ui->settings_return_zone == UI_ZONE_ROM_BROWSER) ||
        ui->zone == UI_ZONE_ERROR_MODAL) {
        ui_render_clear(fb, w, h, pitch, 0xFF0F172AU);
        ui_render_rect(fb, w, h, pitch, 0, 0, w, 108, 0xFF1E293BU);
        ui_render_text(fb, w, h, pitch, 48, 32, 4, 0xFF38BDF8U, "PS4 GAME BOY JIT + HLE");

        char sub[128];
        (void)snprintf(sub, sizeof(sub), "ENGINE: %s  |  PALETTE: %s  |  ROMS: %zu",
                       jit->enabled ? "X86_64 JIT" : "INTERPRETER",
                       hle_ppu_palette_name(cfg->palette_index),
                       ui->rom_count);
        ui_render_text(fb, w, h, pitch, 48, 76, 2, 0xFF94A3B8U, sub);

        if (ui->rom_count == 0) {
            char empty_hdr[192], step2[192];
            (void)snprintf(empty_hdr, sizeof(empty_hdr), "NO .GB ROMS FOUND IN %.120s/roms/", hal_fs_get_data_dir());
            (void)snprintf(step2, sizeof(step2), "2. COPY DMG .GB FILES INTO %.120s/roms/", hal_fs_get_data_dir());
            ui_render_rect(fb, w, h, pitch, 120, 260, w - 240, 260, 0xFF1E293BU);
            ui_render_text(fb, w, h, pitch, 160, 310, 3, 0xFFFDE047U, empty_hdr);
            ui_render_text(fb, w, h, pitch, 160, 370, 2, 0xFFE2E8F0U, "1. ON PS4: USE GOLDHEN FTP (:2121) | ON PC: OPEN DATA/ROMS FOLDER");
            ui_render_text(fb, w, h, pitch, 160, 410, 2, 0xFFE2E8F0U, step2);
            ui_render_text(fb, w, h, pitch, 160, 450, 2, 0xFF86EFACU, "3. PRESS TRIANGLE (PC: R) TO REFRESH LIST");
        } else {
            int max_vis = 14;
            int start = 0;
            if (ui->rom_selected >= max_vis) {
                start = ui->rom_selected - max_vis + 1;
            }
            for (int i = 0; i < max_vis && (size_t)(start + i) < ui->rom_count; ++i) {
                int idx = start + i;
                int y = 140 + i * 56;
                bool sel = (idx == ui->rom_selected);
                if (sel) {
                    ui_render_rect(fb, w, h, pitch, 48, y - 6, w - 96, 46, 0xFF0284C7U);
                }
                ui_render_text(fb, w, h, pitch, 72, y + 6, 3,
                               sel ? 0xFFFFFFFFU : 0xFFCBD5E1U, ui->roms[idx].name);
            }
        }

        /* Footer legend */
        ui_render_rect(fb, w, h, pitch, 0, h - 64, w, 64, 0xFF1E293BU);
        ui_render_text(fb, w, h, pitch, 48, h - 42, 2, 0xFF94A3B8U,
                       "[X] LAUNCH   [TRIANGLE] REFRESH   [SQUARE] SETTINGS   [L3+R3] DEBUG HUD");
    } else {
        /* Game Boy 1080p 7x integer viewport */
        hle_ppu_blit_1080p(&gb->ppu, cfg->palette_index, fb, w, h, pitch);
    }

    /* Modals */
    if (ui->zone == UI_ZONE_PAUSE_MODAL) {
        ui_render_rect_alpha(fb, w, h, pitch, 0, 0, w, h, 0xA0000000U);
        int mx = (w - 640) / 2, my = (h - 500) / 2;
        ui_render_rect(fb, w, h, pitch, mx, my, 640, 500, 0xFF1E293BU);
        ui_render_text(fb, w, h, pitch, mx + 36, my + 28, 3, 0xFF38BDF8U, "PAUSED");

        static const char *const kPauseItems[6] = {
            "RESUME GAME",
            "SAVE STATE (SLOT 0)",
            "LOAD STATE (SLOT 0)",
            "SETTINGS",
            "RESET GAME",
            "EXIT TO ROM BROWSER"
        };
        for (int i = 0; i < 6; ++i) {
            int iy = my + 96 + i * 58;
            bool sel = (i == ui->pause_selected);
            if (sel) {
                ui_render_rect(fb, w, h, pitch, mx + 24, iy - 6, 592, 46, 0xFF0284C7U);
            }
            ui_render_text(fb, w, h, pitch, mx + 44, iy + 6, 3,
                           sel ? 0xFFFFFFFFU : 0xFFCBD5E1U, kPauseItems[i]);
        }
    } else if (ui->zone == UI_ZONE_SETTINGS_MODAL) {
        ui_render_rect_alpha(fb, w, h, pitch, 0, 0, w, h, 0xB0000000U);
        int mx = (w - 760) / 2, my = (h - 420) / 2;
        ui_render_rect(fb, w, h, pitch, mx, my, 760, 420, 0xFF1E293BU);
        ui_render_text(fb, w, h, pitch, mx + 36, my + 28, 3, 0xFF38BDF8U, "SETTINGS");

        char lines[4][96];
        (void)snprintf(lines[0], sizeof(lines[0]), "CPU ENGINE : %s",
                       jit->enabled ? "X86_64 JIT" : (jit->available ? "INTERPRETER" : "INTERP (NO JIT)"));
        (void)snprintf(lines[1], sizeof(lines[1]), "PALETTE    : %s",
                       hle_ppu_palette_name(cfg->palette_index));
        (void)snprintf(lines[2], sizeof(lines[2]), "SWAP A/B   : %s",
                       cfg->swap_ab ? "ON" : "OFF");
        (void)snprintf(lines[3], sizeof(lines[3]), "BACK");

        for (int i = 0; i < 4; ++i) {
            int iy = my + 100 + i * 64;
            bool sel = (i == ui->settings_selected);
            if (sel) {
                ui_render_rect(fb, w, h, pitch, mx + 24, iy - 6, 712, 48, 0xFF0284C7U);
            }
            ui_render_text(fb, w, h, pitch, mx + 44, iy + 6, 3,
                           sel ? 0xFFFFFFFFU : 0xFFCBD5E1U, lines[i]);
        }
    } else if (ui->zone == UI_ZONE_ERROR_MODAL) {
        ui_render_rect_alpha(fb, w, h, pitch, 0, 0, w, h, 0xB0000000U);
        int mx = (w - 820) / 2, my = (h - 260) / 2;
        ui_render_rect(fb, w, h, pitch, mx, my, 820, 260, 0xFF450A0AU);
        ui_render_text(fb, w, h, pitch, mx + 36, my + 32, 3, 0xFFFCA5A5U, "ERROR");
        ui_render_text(fb, w, h, pitch, mx + 36, my + 104, 2, 0xFFFFFFFFU, ui->error_msg);
        ui_render_text(fb, w, h, pitch, mx + 36, my + 190, 2, 0xFF94A3B8U, "PRESS [X] OR [O] TO DISMISS");
    }

    /* Toast banner */
    if (ui->toast_frames > 0 && ui->toast_msg[0] != '\0') {
        ui_render_rect_alpha(fb, w, h, pitch, w - 520, 24, 496, 52, 0xD00F172AU);
        ui_render_text(fb, w, h, pitch, w - 500, 40, 2, 0xFF86EFACU, ui->toast_msg);
    }
}
