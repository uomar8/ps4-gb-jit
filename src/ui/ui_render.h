#ifndef PS4_GB_JIT_UI_RENDER_H
#define PS4_GB_JIT_UI_RENDER_H

#include <stdint.h>

void ui_render_clear(uint32_t *fb, int w, int h, int pitch, uint32_t argb);
void ui_render_rect(uint32_t *fb, int w, int h, int pitch,
                    int x, int y, int rw, int rh, uint32_t argb);
void ui_render_rect_alpha(uint32_t *fb, int w, int h, int pitch,
                          int x, int y, int rw, int rh, uint32_t argb);
void ui_render_text(uint32_t *fb, int w, int h, int pitch,
                    int x, int y, int scale, uint32_t argb, const char *text);

#endif /* PS4_GB_JIT_UI_RENDER_H */
