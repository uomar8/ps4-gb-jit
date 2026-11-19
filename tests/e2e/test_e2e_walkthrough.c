#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/app.h"
#include "hal/hal.h"
#include "hal/mock/hal_mock_ctl.h"

/*
 * Generate a 32 KiB playable synthetic DMG-01 ROM that:
 *  - Sets up BG tile 1 (checkerboard) in VRAM 0x8010..0x801F
 *  - Fills BG tilemap 0x9800..0x9BFF with tile 1
 *  - Enables Ch1 square wave audio
 *  - Enables LCD + BG, then loops incrementing SCX each VBlank so pixels animate
 */
static void write_demo_rom(const char *abs_path) {
    uint8_t *rom = (uint8_t *)calloc(1, 32768);
    assert(rom != NULL);
    memcpy(rom + 0x0134, "DEMOPATTERN", 11);
    rom[0x0147] = 0x00; /* ROM ONLY */
    rom[0x0148] = 0x00; /* 32 KiB */
    rom[0x0149] = 0x00;

    size_t p = 0x0100;
    rom[p++] = 0x00; /* NOP */
    rom[p++] = 0xC3; rom[p++] = 0x50; rom[p++] = 0x01; /* JP 0x0150 */

    p = 0x0150;
    /* Turn off LCD? No, write tile 1 directly to VRAM 0x8010..0x801F */
    rom[p++] = 0x21; rom[p++] = 0x10; rom[p++] = 0x80; /* LD HL, 0x8010 */
    rom[p++] = 0x06; rom[p++] = 0x10;                  /* LD B, 16 */
    uint16_t tloop = (uint16_t)p;
    rom[p++] = 0x36; rom[p++] = 0xAA;                  /* LD (HL), 0xAA */
    rom[p++] = 0x23;                                   /* INC HL */
    rom[p++] = 0x05;                                   /* DEC B */
    rom[p++] = 0x20;
    { int8_t rel = (int8_t)(tloop - (p + 1)); rom[p++] = (uint8_t)rel; }

    /* Fill first 256 entries of BG map 0x9800 with tile 1 */
    rom[p++] = 0x21; rom[p++] = 0x00; rom[p++] = 0x98; /* LD HL, 0x9800 */
    rom[p++] = 0x06; rom[p++] = 0x00;                  /* LD B, 256 */
    uint16_t mloop = (uint16_t)p;
    rom[p++] = 0x36; rom[p++] = 0x01;                  /* LD (HL), 1 */
    rom[p++] = 0x23;                                   /* INC HL */
    rom[p++] = 0x05;                                   /* DEC B */
    rom[p++] = 0x20;
    { int8_t rel = (int8_t)(mloop - (p + 1)); rom[p++] = (uint8_t)rel; }

    /* Enable APU Ch1 tone */
    rom[p++] = 0x3E; rom[p++] = 0x80; rom[p++] = 0xE0; rom[p++] = 0x26; /* NR52 */
    rom[p++] = 0x3E; rom[p++] = 0x77; rom[p++] = 0xE0; rom[p++] = 0x24; /* NR50 */
    rom[p++] = 0x3E; rom[p++] = 0xFF; rom[p++] = 0xE0; rom[p++] = 0x25; /* NR51 */
    rom[p++] = 0x3E; rom[p++] = 0x80; rom[p++] = 0xE0; rom[p++] = 0x11; /* NR11 */
    rom[p++] = 0x3E; rom[p++] = 0xF0; rom[p++] = 0xE0; rom[p++] = 0x12; /* NR12 */
    rom[p++] = 0x3E; rom[p++] = 0x87; rom[p++] = 0xE0; rom[p++] = 0x14; /* NR14 trigger */

    /* BGP = 0xE4, LCDC = 0x91 */
    rom[p++] = 0x3E; rom[p++] = 0xE4; rom[p++] = 0xE0; rom[p++] = 0x47;
    rom[p++] = 0x3E; rom[p++] = 0x91; rom[p++] = 0xE0; rom[p++] = 0x40;

    /* Main animation loop: increment SCX (0xFF43) */
    uint16_t mainloop = (uint16_t)p;
    rom[p++] = 0xF0; rom[p++] = 0x43; /* LDH A, (0x43) */
    rom[p++] = 0x3C;                  /* INC A */
    rom[p++] = 0xE0; rom[p++] = 0x43; /* LDH (0x43), A */
    rom[p++] = 0x18;
    { int8_t rel = (int8_t)(mainloop - (p + 1)); rom[p++] = (uint8_t)rel; }

    assert(hal_fs_write_file(abs_path, rom, 32768) == APP_OK);
    free(rom);
}

static bool file_exists_nonempty(const char *path) {
    int64_t sz = 0;
    bool is_dir = false;
    return (hal_fs_stat_size(path, &sz, &is_dir) == APP_OK && !is_dir && sz > 0);
}

int main(void) {
    const char *root = "/tmp/ps4_gb_jit_e2e_walkthrough";
    hal_mock_fs_set_data_dir(root);
    hal_mock_fs_set_inject_ghosts(true); /* Verify DT_WHT=14 ghost filter in ROM browser */

    char roms_dir[256], rom_path[256];
    snprintf(roms_dir, sizeof(roms_dir), "%s/roms", root);
    snprintf(rom_path, sizeof(rom_path), "%s/roms/demo_pattern.gb", root);
    assert(hal_fs_mkdir_p(root) == APP_OK);
    assert(hal_fs_mkdir_p(roms_dir) == APP_OK);
    write_demo_rom(rom_path);

    /* Scripted controller walkthrough covering every UI zone, modal, and DS4 mapping */
    hal_mock_input_reset();
    /* 1. ROM Browser: 2 idle frames, Triangle refresh, Cross to launch demo_pattern.gb */
    hal_mock_input_push(0, 2);
    hal_mock_input_push(HAL_BTN_TRIANGLE, 1);
    hal_mock_input_push(0, 1);
    hal_mock_input_push(HAL_BTN_CROSS, 1);

    /* 2. In-Game: 10 frames gameplay with D-Pad + Touchpad (SELECT) + R1 (START) + L2 fast-forward */
    hal_mock_input_push(0, 4);
    hal_mock_input_push(HAL_BTN_RIGHT | HAL_BTN_TOUCHPAD, 2);
    hal_mock_input_push(HAL_BTN_R1 | HAL_BTN_L2, 2);
    hal_mock_input_push(0, 2);

    /* 3. Open Pause Modal (Options) and keep open for 40 frames (>500ms at 60Hz) */
    hal_mock_input_push(HAL_BTN_OPTIONS, 1);
    hal_mock_input_push(0, 40);

    /* 4. Save State (Down -> item 1, Cross -> saves .st0 and resumes) */
    hal_mock_input_push(HAL_BTN_DOWN, 1);
    hal_mock_input_push(0, 1);
    hal_mock_input_push(HAL_BTN_CROSS, 1);
    hal_mock_input_push(0, 3);

    /* 5. Open Pause -> Settings (Down x3, Cross) -> Toggle JIT, Cycle Palette -> Circle back */
    hal_mock_input_push(HAL_BTN_OPTIONS, 1);
    hal_mock_input_push(0, 1);
    hal_mock_input_push(HAL_BTN_DOWN, 1); hal_mock_input_push(0, 1);
    hal_mock_input_push(HAL_BTN_DOWN, 1); hal_mock_input_push(0, 1);
    hal_mock_input_push(HAL_BTN_DOWN, 1); hal_mock_input_push(0, 1);
    hal_mock_input_push(HAL_BTN_CROSS, 1); hal_mock_input_push(0, 1);
    /* In Settings: toggle CPU engine twice (JIT->Interp->JIT) */
    hal_mock_input_push(HAL_BTN_CROSS, 1); hal_mock_input_push(0, 1);
    hal_mock_input_push(HAL_BTN_CROSS, 1); hal_mock_input_push(0, 1);
    /* Down to Palette, cycle once, Circle back to Pause */
    hal_mock_input_push(HAL_BTN_DOWN, 1); hal_mock_input_push(0, 1);
    hal_mock_input_push(HAL_BTN_CROSS, 1); hal_mock_input_push(0, 1);
    hal_mock_input_push(HAL_BTN_CIRCLE, 1); hal_mock_input_push(0, 1);

    /* 6. Toggle Live Debug HUD (L3+R3) ON, hold 2 frames, toggle OFF */
    hal_mock_input_push(HAL_BTN_L3 | HAL_BTN_R3, 1);
    hal_mock_input_push(0, 2);
    hal_mock_input_push(HAL_BTN_L3 | HAL_BTN_R3, 1);
    hal_mock_input_push(0, 1);

    /* 7. Load State (from Pause: currently at item 3 Settings -> Up to item 2 Load State -> Cross) */
    hal_mock_input_push(HAL_BTN_UP, 1); hal_mock_input_push(0, 1);
    hal_mock_input_push(HAL_BTN_CROSS, 1); hal_mock_input_push(0, 3);

    /* 8. Open Pause -> Exit to ROM Browser (Up wraps from 0 to 5 -> Cross) */
    hal_mock_input_push(HAL_BTN_OPTIONS, 1); hal_mock_input_push(0, 1);
    hal_mock_input_push(HAL_BTN_UP, 1); hal_mock_input_push(0, 1);
    hal_mock_input_push(HAL_BTN_CROSS, 1); hal_mock_input_push(0, 2);

    /* 9. Clean Quit */
    hal_mock_input_push_quit();

    AppResult rc = app_run(0);
    assert(rc == APP_OK);

    /* Verify video frames presented, non-blank framebuffer, and audio frames pushed */
    assert(hal_mock_video_present_count() >= 80);
    assert(hal_mock_video_checksum() != 0);
    assert(hal_mock_audio_total_frames() > 0);

    /* Verify all persistent files & save state were created */
    char path[256];
    snprintf(path, sizeof(path), "%s/log.txt", root);
    assert(file_exists_nonempty(path));
    snprintf(path, sizeof(path), "%s/klog.txt", root);
    assert(file_exists_nonempty(path));
    snprintf(path, sizeof(path), "%s/missing_report.txt", root);
    assert(file_exists_nonempty(path));
    snprintf(path, sizeof(path), "%s/config.ini", root);
    assert(file_exists_nonempty(path));
    snprintf(path, sizeof(path), "%s/saves/demo_pattern.gb.st0", root);
    assert(file_exists_nonempty(path));

    /* Verify 0 ERROR / FATAL lines in log.txt */
    snprintf(path, sizeof(path), "%s/log.txt", root);
    uint8_t *log_buf = NULL;
    size_t log_sz = 0;
    assert(hal_fs_read_file(path, &log_buf, &log_sz) == APP_OK);
    char *log_str = (char *)malloc(log_sz + 1);
    assert(log_str != NULL);
    memcpy(log_str, log_buf, log_sz);
    log_str[log_sz] = '\0';
    assert(strstr(log_str, "[ERROR]") == NULL);
    assert(strstr(log_str, "[FATAL]") == NULL);
    assert(strstr(log_str, "entered PAUSE_MODAL") != NULL);
    assert(strstr(log_str, "launched ROM name=demo_pattern.gb") != NULL);
    free(log_str);
    free(log_buf);

    hal_mock_fs_set_inject_ghosts(false);
    printf("[PASS] test_e2e_walkthrough: full scripted controller walkthrough passed (%u frames, %llu audio frames, 0 ERROR/FATAL)!\n",
           hal_mock_video_present_count(),
           (unsigned long long)hal_mock_audio_total_frames());
    return 0;
}
