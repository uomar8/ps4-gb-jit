#include "ui_render.h"

#include <stddef.h>

/* Compact 5x7 uppercase/ASCII bitmap font for crisp integer-scaled 1080p text */
static void ui_glyph_5x7(char c, uint8_t rows[7]) {
    /* Convert lowercase to uppercase for compact ROM-style legibility */
    if (c >= 'a' && c <= 'z') {
        c = (char)(c - 'a' + 'A');
    }
    static const uint8_t blank[7] = {0,0,0,0,0,0,0};
    for (int i = 0; i < 7; i++) rows[i] = blank[i];

    switch (c) {
        case '0': { const uint8_t g[7]={0x0E,0x11,0x13,0x15,0x19,0x11,0x0E}; for(int i=0;i<7;i++) rows[i]=g[i]; break; }
        case '1': { const uint8_t g[7]={0x04,0x0C,0x04,0x04,0x04,0x04,0x0E}; for(int i=0;i<7;i++) rows[i]=g[i]; break; }
        case '2': { const uint8_t g[7]={0x0E,0x11,0x01,0x02,0x04,0x08,0x1F}; for(int i=0;i<7;i++) rows[i]=g[i]; break; }
        case '3': { const uint8_t g[7]={0x1E,0x01,0x01,0x0E,0x01,0x01,0x1E}; for(int i=0;i<7;i++) rows[i]=g[i]; break; }
        case '4': { const uint8_t g[7]={0x02,0x06,0x0A,0x12,0x1F,0x02,0x02}; for(int i=0;i<7;i++) rows[i]=g[i]; break; }
        case '5': { const uint8_t g[7]={0x1F,0x10,0x1E,0x01,0x01,0x11,0x0E}; for(int i=0;i<7;i++) rows[i]=g[i]; break; }
        case '6': { const uint8_t g[7]={0x06,0x08,0x10,0x1E,0x11,0x11,0x0E}; for(int i=0;i<7;i++) rows[i]=g[i]; break; }
        case '7': { const uint8_t g[7]={0x1F,0x01,0x02,0x04,0x08,0x08,0x08}; for(int i=0;i<7;i++) rows[i]=g[i]; break; }
        case '8': { const uint8_t g[7]={0x0E,0x11,0x11,0x0E,0x11,0x11,0x0E}; for(int i=0;i<7;i++) rows[i]=g[i]; break; }
        case '9': { const uint8_t g[7]={0x0E,0x11,0x11,0x0F,0x01,0x02,0x0C}; for(int i=0;i<7;i++) rows[i]=g[i]; break; }
        case 'A': { const uint8_t g[7]={0x0E,0x11,0x11,0x1F,0x11,0x11,0x11}; for(int i=0;i<7;i++) rows[i]=g[i]; break; }
        case 'B': { const uint8_t g[7]={0x1E,0x11,0x11,0x1E,0x11,0x11,0x1E}; for(int i=0;i<7;i++) rows[i]=g[i]; break; }
        case 'C': { const uint8_t g[7]={0x0E,0x11,0x10,0x10,0x10,0x11,0x0E}; for(int i=0;i<7;i++) rows[i]=g[i]; break; }
        case 'D': { const uint8_t g[7]={0x1C,0x12,0x11,0x11,0x11,0x12,0x1C}; for(int i=0;i<7;i++) rows[i]=g[i]; break; }
        case 'E': { const uint8_t g[7]={0x1F,0x10,0x10,0x1E,0x10,0x10,0x1F}; for(int i=0;i<7;i++) rows[i]=g[i]; break; }
        case 'F': { const uint8_t g[7]={0x1F,0x10,0x10,0x1E,0x10,0x10,0x10}; for(int i=0;i<7;i++) rows[i]=g[i]; break; }
        case 'G': { const uint8_t g[7]={0x0E,0x11,0x10,0x17,0x11,0x11,0x0F}; for(int i=0;i<7;i++) rows[i]=g[i]; break; }
        case 'H': { const uint8_t g[7]={0x11,0x11,0x11,0x1F,0x11,0x11,0x11}; for(int i=0;i<7;i++) rows[i]=g[i]; break; }
        case 'I': { const uint8_t g[7]={0x0E,0x04,0x04,0x04,0x04,0x04,0x0E}; for(int i=0;i<7;i++) rows[i]=g[i]; break; }
        case 'J': { const uint8_t g[7]={0x07,0x02,0x02,0x02,0x12,0x12,0x0C}; for(int i=0;i<7;i++) rows[i]=g[i]; break; }
        case 'K': { const uint8_t g[7]={0x11,0x12,0x14,0x18,0x14,0x12,0x11}; for(int i=0;i<7;i++) rows[i]=g[i]; break; }
        case 'L': { const uint8_t g[7]={0x10,0x10,0x10,0x10,0x10,0x10,0x1F}; for(int i=0;i<7;i++) rows[i]=g[i]; break; }
        case 'M': { const uint8_t g[7]={0x11,0x1B,0x15,0x15,0x11,0x11,0x11}; for(int i=0;i<7;i++) rows[i]=g[i]; break; }
        case 'N': { const uint8_t g[7]={0x11,0x19,0x15,0x13,0x11,0x11,0x11}; for(int i=0;i<7;i++) rows[i]=g[i]; break; }
        case 'O': { const uint8_t g[7]={0x0E,0x11,0x11,0x11,0x11,0x11,0x0E}; for(int i=0;i<7;i++) rows[i]=g[i]; break; }
        case 'P': { const uint8_t g[7]={0x1E,0x11,0x11,0x1E,0x10,0x10,0x10}; for(int i=0;i<7;i++) rows[i]=g[i]; break; }
        case 'Q': { const uint8_t g[7]={0x0E,0x11,0x11,0x11,0x15,0x12,0x0D}; for(int i=0;i<7;i++) rows[i]=g[i]; break; }
        case 'R': { const uint8_t g[7]={0x1E,0x11,0x11,0x1E,0x14,0x12,0x11}; for(int i=0;i<7;i++) rows[i]=g[i]; break; }
        case 'S': { const uint8_t g[7]={0x0F,0x10,0x10,0x0E,0x01,0x01,0x1E}; for(int i=0;i<7;i++) rows[i]=g[i]; break; }
        case 'T': { const uint8_t g[7]={0x1F,0x04,0x04,0x04,0x04,0x04,0x04}; for(int i=0;i<7;i++) rows[i]=g[i]; break; }
        case 'U': { const uint8_t g[7]={0x11,0x11,0x11,0x11,0x11,0x11,0x0E}; for(int i=0;i<7;i++) rows[i]=g[i]; break; }
        case 'V': { const uint8_t g[7]={0x11,0x11,0x11,0x11,0x11,0x0A,0x04}; for(int i=0;i<7;i++) rows[i]=g[i]; break; }
        case 'W': { const uint8_t g[7]={0x11,0x11,0x11,0x15,0x15,0x1B,0x11}; for(int i=0;i<7;i++) rows[i]=g[i]; break; }
        case 'X': { const uint8_t g[7]={0x11,0x11,0x0A,0x04,0x0A,0x11,0x11}; for(int i=0;i<7;i++) rows[i]=g[i]; break; }
        case 'Y': { const uint8_t g[7]={0x11,0x11,0x0A,0x04,0x04,0x04,0x04}; for(int i=0;i<7;i++) rows[i]=g[i]; break; }
        case 'Z': { const uint8_t g[7]={0x1F,0x01,0x02,0x04,0x08,0x10,0x1F}; for(int i=0;i<7;i++) rows[i]=g[i]; break; }
        case ':': { const uint8_t g[7]={0x00,0x04,0x04,0x00,0x04,0x04,0x00}; for(int i=0;i<7;i++) rows[i]=g[i]; break; }
        case '.': { const uint8_t g[7]={0x00,0x00,0x00,0x00,0x00,0x06,0x06}; for(int i=0;i<7;i++) rows[i]=g[i]; break; }
        case ',': { const uint8_t g[7]={0x00,0x00,0x00,0x00,0x00,0x04,0x08}; for(int i=0;i<7;i++) rows[i]=g[i]; break; }
        case '-': { const uint8_t g[7]={0x00,0x00,0x00,0x1F,0x00,0x00,0x00}; for(int i=0;i<7;i++) rows[i]=g[i]; break; }
        case '_': { const uint8_t g[7]={0x00,0x00,0x00,0x00,0x00,0x00,0x1F}; for(int i=0;i<7;i++) rows[i]=g[i]; break; }
        case '+': { const uint8_t g[7]={0x00,0x04,0x04,0x1F,0x04,0x04,0x00}; for(int i=0;i<7;i++) rows[i]=g[i]; break; }
        case '=': { const uint8_t g[7]={0x00,0x00,0x1F,0x00,0x1F,0x00,0x00}; for(int i=0;i<7;i++) rows[i]=g[i]; break; }
        case '/': { const uint8_t g[7]={0x01,0x02,0x02,0x04,0x08,0x08,0x10}; for(int i=0;i<7;i++) rows[i]=g[i]; break; }
        case '[': { const uint8_t g[7]={0x0E,0x08,0x08,0x08,0x08,0x08,0x0E}; for(int i=0;i<7;i++) rows[i]=g[i]; break; }
        case ']': { const uint8_t g[7]={0x0E,0x02,0x02,0x02,0x02,0x02,0x0E}; for(int i=0;i<7;i++) rows[i]=g[i]; break; }
        case '(': { const uint8_t g[7]={0x02,0x04,0x08,0x08,0x08,0x04,0x02}; for(int i=0;i<7;i++) rows[i]=g[i]; break; }
        case ')': { const uint8_t g[7]={0x08,0x04,0x02,0x02,0x02,0x04,0x08}; for(int i=0;i<7;i++) rows[i]=g[i]; break; }
        case '%': { const uint8_t g[7]={0x18,0x19,0x02,0x04,0x08,0x13,0x03}; for(int i=0;i<7;i++) rows[i]=g[i]; break; }
        case '|': { const uint8_t g[7]={0x04,0x04,0x04,0x04,0x04,0x04,0x04}; for(int i=0;i<7;i++) rows[i]=g[i]; break; }
        case '>': { const uint8_t g[7]={0x10,0x08,0x04,0x02,0x04,0x08,0x10}; for(int i=0;i<7;i++) rows[i]=g[i]; break; }
        case '<': { const uint8_t g[7]={0x02,0x04,0x08,0x10,0x08,0x04,0x02}; for(int i=0;i<7;i++) rows[i]=g[i]; break; }
        case '!': { const uint8_t g[7]={0x04,0x04,0x04,0x04,0x04,0x00,0x04}; for(int i=0;i<7;i++) rows[i]=g[i]; break; }
        case '?': { const uint8_t g[7]={0x0E,0x11,0x02,0x04,0x04,0x00,0x04}; for(int i=0;i<7;i++) rows[i]=g[i]; break; }
        default: break;
    }
}

void ui_render_clear(uint32_t *fb, int w, int h, int pitch, uint32_t argb) {
    if (!fb) return;
    for (int y = 0; y < h; y++) {
        uint32_t *row = fb + (size_t)y * (size_t)pitch;
        for (int x = 0; x < w; x++) {
            row[x] = argb;
        }
    }
}

void ui_render_rect(uint32_t *fb, int w, int h, int pitch,
                    int x, int y, int rw, int rh, uint32_t argb) {
    if (!fb) return;
    int x0 = x < 0 ? 0 : x;
    int y0 = y < 0 ? 0 : y;
    int x1 = (x + rw > w) ? w : (x + rw);
    int y1 = (y + rh > h) ? h : (y + rh);
    for (int py = y0; py < y1; py++) {
        uint32_t *row = fb + (size_t)py * (size_t)pitch;
        for (int px = x0; px < x1; px++) {
            row[px] = argb;
        }
    }
}

void ui_render_rect_alpha(uint32_t *fb, int w, int h, int pitch,
                          int x, int y, int rw, int rh, uint32_t argb) {
    if (!fb) return;
    uint32_t a = (argb >> 24) & 0xFFU;
    uint32_t sr = (argb >> 16) & 0xFFU;
    uint32_t sg = (argb >> 8) & 0xFFU;
    uint32_t sb = argb & 0xFFU;
    uint32_t inv_a = 255U - a;

    int x0 = x < 0 ? 0 : x;
    int y0 = y < 0 ? 0 : y;
    int x1 = (x + rw > w) ? w : (x + rw);
    int y1 = (y + rh > h) ? h : (y + rh);
    for (int py = y0; py < y1; py++) {
        uint32_t *row = fb + (size_t)py * (size_t)pitch;
        for (int px = x0; px < x1; px++) {
            uint32_t d = row[px];
            uint32_t dr = (d >> 16) & 0xFFU;
            uint32_t dg = (d >> 8) & 0xFFU;
            uint32_t db = d & 0xFFU;
            uint32_t or_ = (sr * a + dr * inv_a) / 255U;
            uint32_t og_ = (sg * a + dg * inv_a) / 255U;
            uint32_t ob_ = (sb * a + db * inv_a) / 255U;
            row[px] = 0xFF000000U | (or_ << 16) | (og_ << 8) | ob_;
        }
    }
}

void ui_render_text(uint32_t *fb, int w, int h, int pitch,
                    int x, int y, int scale, uint32_t argb, const char *text) {
    if (!fb || !text || scale <= 0) return;
    int cx = x;
    int cy = y;
    for (const char *p = text; *p; p++) {
        if (*p == '\n') {
            cx = x;
            cy += 9 * scale;
            continue;
        }
        uint8_t rows[7];
        ui_glyph_5x7(*p, rows);
        for (int r = 0; r < 7; r++) {
            uint8_t bits = rows[r];
            for (int col = 0; col < 5; col++) {
                if (bits & (1U << (4 - col))) {
                    ui_render_rect(fb, w, h, pitch,
                                   cx + col * scale, cy + r * scale,
                                   scale, scale, argb);
                }
            }
        }
        cx += 6 * scale;
    }
}
