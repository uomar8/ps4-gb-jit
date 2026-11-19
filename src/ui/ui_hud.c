#include "ui_hud.h"
#include "ui_render.h"
#include "../debug/dbg_log.h"

#include <stdio.h>

void ui_hud_render(uint32_t *fb, int w, int h, int pitch, const UiHudMetrics *m) {
    if (!fb || !m) {
        return;
    }
    const int x0 = 36;
    const int y0 = 36;
    const int box_w = 1120;
    const int box_h = 340;

    ui_render_rect_alpha(fb, w, h, pitch, x0, y0, box_w, box_h, 0xCC10141EU);
    ui_render_rect(fb, w, h, pitch, x0, y0, box_w, 4, 0xFF38BDF8U);

    char line[192];
    ui_render_text(fb, w, h, pitch, x0 + 16, y0 + 14, 2, 0xFF38BDF8U,
                   "LIVE DEBUG HUD (L3+R3 / F12)");

    (void)snprintf(line, sizeof(line), "ZONE: %s | MODAL: %s | BTN: 0x%05X",
                   m->focus_zone ? m->focus_zone : "-",
                   m->active_modal ? m->active_modal : "NONE",
                   m->last_buttons);
    ui_render_text(fb, w, h, pitch, x0 + 16, y0 + 42, 2, 0xFFE2E8F0U, line);

    (void)snprintf(line, sizeof(line), "EXEC: %s | BLOCKS: %u | CACHE: %u KB | FPS: %u | AUD: %u%%",
                   m->jit_active ? "JIT" : "INTERP",
                   m->jit_blocks, m->jit_cache_kb, m->fps, m->audio_fill_pct);
    ui_render_text(fb, w, h, pitch, x0 + 16, y0 + 68, 2, 0xFF86EFACU, line);

    char recent[6][DBG_LOG_LINE_MAX];
    size_t count = dbg_log_get_recent(recent, 6);
    int ly = y0 + 106;
    for (size_t i = 0; i < count; i++) {
        recent[i][88] = '\0'; /* clip to box width */
        ui_render_text(fb, w, h, pitch, x0 + 16, ly, 2, 0xFFCBD5E1U, recent[i]);
        ly += 26;
    }
}
