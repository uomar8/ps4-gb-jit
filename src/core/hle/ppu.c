#include "ppu.h"

#include <string.h>

static const uint32_t g_palettes[GB_PALETTE_COUNT][4] = {
    /* 0: Classic DMG Green */
    { 0xFF9BBC0FU, 0xFF8BAC0FU, 0xFF306230U, 0xFF0F380FU },
    /* 1: Game Boy Pocket Gray */
    { 0xFFE0F8D0U, 0xFF88C070U, 0xFF346856U, 0xFF081820U },
    /* 2: Warm Amber */
    { 0xFFFFE4A0U, 0xFFE0A040U, 0xFF884818U, 0xFF281004U },
    /* 3: Crisp High-Contrast B&W */
    { 0xFFFFFFFFU, 0xFFAAAAAAU, 0xFF555555U, 0xFF000000U }
};

static const char *g_palette_names[GB_PALETTE_COUNT] = {
    "DMG GREEN",
    "POCKET GRAY",
    "WARM AMBER",
    "HIGH-CONTRAST BW"
};

const char *hle_ppu_palette_name(uint32_t index) {
    return g_palette_names[index % GB_PALETTE_COUNT];
}

void hle_ppu_reset(GbPpu *p) {
    if (!p) return;
    memset(p, 0, sizeof(*p));
    /* DMG-01 post-boot state */
    p->lcdc = 0x91U;
    p->stat = 0x85U;
    p->bgp  = 0xFCU;
    p->obp0 = 0xFFU;
    p->obp1 = 0xFFU;
}

static void ppu_update_stat_irq(GbPpu *p, uint8_t *if_reg) {
    uint8_t mode = p->stat & 0x03U;
    bool lyc_eq = (p->ly == p->lyc);
    if (lyc_eq) {
        p->stat |= 0x04U;
    } else {
        p->stat &= (uint8_t)~0x04U;
    }

    bool line = false;
    if ((p->stat & 0x40U) && lyc_eq) line = true;
    if ((p->stat & 0x20U) && mode == 2U) line = true;
    if ((p->stat & 0x10U) && mode == 1U) line = true;
    if ((p->stat & 0x08U) && mode == 0U) line = true;

    if (line && !p->stat_irq_line && if_reg) {
        *if_reg |= 0x02U; /* LCD STAT IRQ */
    }
    p->stat_irq_line = line;
}

static void ppu_render_scanline(GbPpu *p) {
    int y = p->ly;
    if (y >= GB_SCREEN_H) {
        return;
    }

    uint8_t *out = &p->fb_shades[y * GB_SCREEN_W];
    uint8_t bg_color_ids[GB_SCREEN_W];
    memset(bg_color_ids, 0, sizeof(bg_color_ids));

    bool lcd_on = (p->lcdc & 0x80U) != 0;
    if (!lcd_on) {
        memset(out, 0, GB_SCREEN_W);
        return;
    }

    bool bg_enable  = (p->lcdc & 0x01U) != 0;
    bool obj_enable = (p->lcdc & 0x02U) != 0;
    bool obj_tall   = (p->lcdc & 0x04U) != 0;
    uint16_t bg_map = (p->lcdc & 0x08U) ? 0x1C00U : 0x1800U;
    bool unsigned_tiles = (p->lcdc & 0x10U) != 0;
    bool win_enable = (p->lcdc & 0x20U) != 0;
    uint16_t win_map = (p->lcdc & 0x40U) ? 0x1C00U : 0x1800U;

    bool win_drawn = false;
    int wx_screen = (int)p->wx - 7;

    for (int x = 0; x < GB_SCREEN_W; x++) {
        uint8_t cid = 0;
        if (bg_enable) {
            bool in_win = win_enable && (y >= p->wy) && (x >= wx_screen);
            uint16_t map_base;
            uint8_t px, py;
            if (in_win) {
                win_drawn = true;
                map_base = win_map;
                px = (uint8_t)(x - wx_screen);
                py = p->win_line;
            } else {
                map_base = bg_map;
                px = (uint8_t)(x + p->scx);
                py = (uint8_t)(y + p->scy);
            }

            uint16_t tile_idx_addr = (uint16_t)(map_base + ((py >> 3) * 32U) + (px >> 3));
            uint8_t tile_id = p->vram[tile_idx_addr & 0x1FFFU];
            uint16_t tile_addr;
            if (unsigned_tiles) {
                tile_addr = (uint16_t)(tile_id * 16U);
            } else {
                tile_addr = (uint16_t)(0x1000 + ((int8_t)tile_id) * 16);
            }
            tile_addr = (uint16_t)(tile_addr + (py & 7U) * 2U);
            uint8_t lo = p->vram[tile_addr & 0x1FFFU];
            uint8_t hi = p->vram[(tile_addr + 1U) & 0x1FFFU];
            int bit = 7 - (px & 7);
            cid = (uint8_t)((((hi >> bit) & 1U) << 1) | ((lo >> bit) & 1U));
        }
        bg_color_ids[x] = cid;
        out[x] = (uint8_t)((p->bgp >> (cid * 2)) & 0x03U);
    }

    if (win_drawn) {
        p->win_line++;
    }

    if (!obj_enable) {
        return;
    }

    /* Select up to 10 sprites on this scanline in OAM order */
    int spr_h = obj_tall ? 16 : 8;
    int spr_indices[10];
    int spr_count = 0;
    for (int i = 0; i < 40 && spr_count < 10; i++) {
        int sy = (int)p->oam[i * 4 + 0] - 16;
        if (y >= sy && y < sy + spr_h) {
            spr_indices[spr_count++] = i;
        }
    }

    /* Stable sort by X coordinate (DMG priority: lower X wins, ties broken by lower OAM index) */
    for (int i = 1; i < spr_count; i++) {
        int key = spr_indices[i];
        uint8_t kx = p->oam[key * 4 + 1];
        int j = i - 1;
        while (j >= 0 && p->oam[spr_indices[j] * 4 + 1] > kx) {
            spr_indices[j + 1] = spr_indices[j];
            j--;
        }
        spr_indices[j + 1] = key;
    }

    /* Draw back-to-front so higher priority (earlier in sorted list) overwrites */
    for (int s = spr_count - 1; s >= 0; s--) {
        int idx = spr_indices[s];
        int sy = (int)p->oam[idx * 4 + 0] - 16;
        int sx = (int)p->oam[idx * 4 + 1] - 8;
        uint8_t tile = p->oam[idx * 4 + 2];
        uint8_t attr = p->oam[idx * 4 + 3];

        if (obj_tall) {
            tile &= 0xFEU;
        }
        int row = y - sy;
        if (attr & 0x40U) {
            row = (spr_h - 1) - row;
        }
        uint16_t taddr = (uint16_t)(tile * 16U + (uint16_t)row * 2U);
        uint8_t lo = p->vram[taddr & 0x1FFFU];
        uint8_t hi = p->vram[(taddr + 1U) & 0x1FFFU];
        uint8_t pal = (attr & 0x10U) ? p->obp1 : p->obp0;
        bool bg_prio = (attr & 0x80U) != 0;

        for (int col = 0; col < 8; col++) {
            int px = sx + col;
            if (px < 0 || px >= GB_SCREEN_W) {
                continue;
            }
            int bit = (attr & 0x20U) ? col : (7 - col);
            uint8_t cid = (uint8_t)((((hi >> bit) & 1U) << 1) | ((lo >> bit) & 1U));
            if (cid == 0) {
                continue; /* transparent */
            }
            if (bg_prio && bg_color_ids[px] != 0) {
                continue;
            }
            out[px] = (uint8_t)((pal >> (cid * 2)) & 0x03U);
        }
    }
}

uint8_t hle_ppu_read8(const GbPpu *p, uint16_t addr) {
    switch (addr) {
        case 0xFF40U: return p->lcdc;
        case 0xFF41U: return (uint8_t)(p->stat | 0x80U);
        case 0xFF42U: return p->scy;
        case 0xFF43U: return p->scx;
        case 0xFF44U: return p->ly;
        case 0xFF45U: return p->lyc;
        case 0xFF47U: return p->bgp;
        case 0xFF48U: return p->obp0;
        case 0xFF49U: return p->obp1;
        case 0xFF4AU: return p->wy;
        case 0xFF4BU: return p->wx;
        default:      return 0xFFU;
    }
}

void hle_ppu_write8(GbPpu *p, uint16_t addr, uint8_t val, uint8_t *if_reg) {
    switch (addr) {
        case 0xFF40U: {
            bool was_on = (p->lcdc & 0x80U) != 0;
            bool now_on = (val & 0x80U) != 0;
            p->lcdc = val;
            if (was_on && !now_on) {
                p->ly = 0;
                p->dot_counter = 0;
                p->win_line = 0;
                p->stat = (uint8_t)(p->stat & ~0x03U);
            }
            break;
        }
        case 0xFF41U:
            p->stat = (uint8_t)((p->stat & 0x07U) | (val & 0x78U) | 0x80U);
            ppu_update_stat_irq(p, if_reg);
            break;
        case 0xFF42U: p->scy = val; break;
        case 0xFF43U: p->scx = val; break;
        case 0xFF44U: break; /* LY is read-only */
        case 0xFF45U:
            p->lyc = val;
            ppu_update_stat_irq(p, if_reg);
            break;
        case 0xFF47U: p->bgp  = val; break;
        case 0xFF48U: p->obp0 = val; break;
        case 0xFF49U: p->obp1 = val; break;
        case 0xFF4AU: p->wy   = val; break;
        case 0xFF4BU: p->wx   = val; break;
        default: break;
    }
}

void hle_ppu_step(GbPpu *p, uint32_t t_cycles, uint8_t *if_reg) {
    if ((p->lcdc & 0x80U) == 0) {
        return;
    }

    p->dot_counter += t_cycles;
    while (p->dot_counter >= 456U) {
        p->dot_counter -= 456U;

        if (p->ly < 144U) {
            ppu_render_scanline(p);
        }

        p->ly++;
        if (p->ly == 144U) {
            p->stat = (uint8_t)((p->stat & ~0x03U) | 1U); /* VBlank */
            p->frame_ready = true;
            if (if_reg) {
                *if_reg |= 0x01U; /* VBlank IRQ */
            }
        } else if (p->ly > 153U) {
            p->ly = 0;
            p->win_line = 0;
            p->stat = (uint8_t)((p->stat & ~0x03U) | 2U); /* OAM */
        }
        ppu_update_stat_irq(p, if_reg);
    }

    /* Update current mode within visible scanline */
    if (p->ly < 144U) {
        uint8_t new_mode;
        if (p->dot_counter < 80U) {
            new_mode = 2U; /* OAM */
        } else if (p->dot_counter < 252U) {
            new_mode = 3U; /* Draw */
        } else {
            new_mode = 0U; /* HBlank */
        }
        if ((p->stat & 0x03U) != new_mode) {
            p->stat = (uint8_t)((p->stat & ~0x03U) | new_mode);
            ppu_update_stat_irq(p, if_reg);
        }
    }
}

void hle_ppu_blit_1080p(const GbPpu *p, uint32_t palette_idx,
                        uint32_t *dst_fb, int dst_w, int dst_h, int dst_pitch) {
    if (!p || !dst_fb) {
        return;
    }
    const uint32_t *pal = g_palettes[palette_idx % GB_PALETTE_COUNT];
    /* 7x integer scale: 160*7 = 1120, 144*7 = 1008, centered in 1920x1080 */
    const int scale = 7;
    const int out_w = GB_SCREEN_W * scale;
    const int out_h = GB_SCREEN_H * scale;
    const int ox = (dst_w - out_w) / 2;
    const int oy = (dst_h - out_h) / 2;

    for (int sy = 0; sy < GB_SCREEN_H; sy++) {
        const uint8_t *src_row = &p->fb_shades[sy * GB_SCREEN_W];
        uint32_t scaled_row[GB_SCREEN_W * 7];
        for (int sx = 0; sx < GB_SCREEN_W; sx++) {
            uint32_t col = pal[src_row[sx] & 3U];
            int bx = sx * scale;
            scaled_row[bx + 0] = col;
            scaled_row[bx + 1] = col;
            scaled_row[bx + 2] = col;
            scaled_row[bx + 3] = col;
            scaled_row[bx + 4] = col;
            scaled_row[bx + 5] = col;
            scaled_row[bx + 6] = col;
        }
        for (int dy = 0; dy < scale; dy++) {
            uint32_t *dst = dst_fb + (size_t)(oy + sy * scale + dy) * (size_t)dst_pitch + ox;
            memcpy(dst, scaled_row, sizeof(scaled_row));
        }
    }
}
