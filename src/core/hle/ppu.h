#ifndef PS4_GB_JIT_PPU_H
#define PS4_GB_JIT_PPU_H

#include <stdint.h>
#include <stdbool.h>
#include "../gb_types.h"

#define GB_PALETTE_COUNT 4

typedef struct GbPpu {
    uint8_t  vram[8192];
    uint8_t  oam[160];

    uint8_t  lcdc; /* 0xFF40 */
    uint8_t  stat; /* 0xFF41 */
    uint8_t  scy;  /* 0xFF42 */
    uint8_t  scx;  /* 0xFF43 */
    uint8_t  ly;   /* 0xFF44 */
    uint8_t  lyc;  /* 0xFF45 */
    uint8_t  bgp;  /* 0xFF47 */
    uint8_t  obp0; /* 0xFF48 */
    uint8_t  obp1; /* 0xFF49 */
    uint8_t  wy;   /* 0xFF4A */
    uint8_t  wx;   /* 0xFF4B */

    uint32_t dot_counter;
    uint8_t  win_line;
    bool     stat_irq_line;
    bool     frame_ready;

    /* 160x144 2-bit shade indices (0..3) */
    uint8_t  fb_shades[GB_SCREEN_W * GB_SCREEN_H];
} GbPpu;

void        hle_ppu_reset(GbPpu *p);
uint8_t     hle_ppu_read8(const GbPpu *p, uint16_t addr);
void        hle_ppu_write8(GbPpu *p, uint16_t addr, uint8_t val, uint8_t *if_reg);
void        hle_ppu_step(GbPpu *p, uint32_t t_cycles, uint8_t *if_reg);
const char *hle_ppu_palette_name(uint32_t index);
void        hle_ppu_blit_1080p(const GbPpu *p, uint32_t palette_idx,
                               uint32_t *dst_fb, int dst_w, int dst_h, int dst_pitch);

#endif /* PS4_GB_JIT_PPU_H */
