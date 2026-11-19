#ifndef PS4_GB_JIT_UI_HUD_H
#define PS4_GB_JIT_UI_HUD_H

#include <stdint.h>
#include <stdbool.h>

typedef struct UiHudMetrics {
    const char *focus_zone;
    const char *active_modal;
    uint32_t    last_buttons;
    bool        jit_active;
    uint32_t    jit_blocks;
    uint32_t    jit_cache_kb;
    uint32_t    fps;
    uint32_t    audio_fill_pct;
} UiHudMetrics;

void ui_hud_render(uint32_t *fb, int w, int h, int pitch, const UiHudMetrics *m);

#endif /* PS4_GB_JIT_UI_HUD_H */
