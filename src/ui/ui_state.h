#ifndef PS4_GB_JIT_UI_STATE_H
#define PS4_GB_JIT_UI_STATE_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "../core/app.h"
#include "../core/gb_machine.h"
#include "../core/jit/jit_engine.h"
#include "../hal/hal.h"

#define UI_MAX_ROMS 256

typedef enum UiFocusZone {
    UI_ZONE_ROM_BROWSER = 0,
    UI_ZONE_IN_GAME,
    UI_ZONE_PAUSE_MODAL,
    UI_ZONE_SETTINGS_MODAL,
    UI_ZONE_ERROR_MODAL
} UiFocusZone;

typedef struct UiRomEntry {
    char name[128];
    char path[APP_MAX_PATH];
} UiRomEntry;

typedef struct UiState {
    UiFocusZone  zone;
    UiFocusZone  settings_return_zone;

    UiRomEntry   roms[UI_MAX_ROMS];
    size_t       rom_count;
    int          rom_selected;
    int          rom_scroll;

    int          pause_selected;
    int          settings_selected;

    char         active_rom_name[128];
    char         active_sav_path[APP_MAX_PATH];
    char         active_state_path[APP_MAX_PATH];

    char         error_msg[192];
    char         toast_msg[128];
    uint32_t     toast_frames;

    bool         hud_visible;
    bool         fast_forward;
} UiState;

void        ui_state_init(UiState *ui);
void        ui_state_scan_roms(UiState *ui);
const char *ui_state_zone_name(UiFocusZone zone);
const char *ui_state_modal_name(UiFocusZone zone);

void        ui_state_save_config(const AppConfig *cfg);

/* Process one frame of DS4/PC input & state transitions */
void        ui_state_update(UiState *ui, AppConfig *cfg, GbMachine *gb,
                            JitEngine *jit, const HalInputState *inp);

/* Render the current UI screen / game viewport / modal onto 1920x1080 ARGB8888 */
void        ui_state_render(const UiState *ui, const AppConfig *cfg,
                            const GbMachine *gb, const JitEngine *jit,
                            uint32_t *fb, int w, int h, int pitch);

#endif /* PS4_GB_JIT_UI_STATE_H */
