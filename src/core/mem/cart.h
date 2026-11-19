#ifndef PS4_GB_JIT_CART_H
#define PS4_GB_JIT_CART_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "../result.h"

typedef enum GbMbcType {
    GB_MBC_NONE = 0,
    GB_MBC_1    = 1,
    GB_MBC_2    = 2,
    GB_MBC_3    = 3,
    GB_MBC_5    = 5
} GbMbcType;

typedef struct GbCart {
    uint8_t  *rom;
    size_t    rom_size;
    uint32_t  rom_banks;

    uint8_t  *ram;
    size_t    ram_size;
    uint32_t  ram_banks;

    GbMbcType mbc;
    bool      has_battery;
    bool      has_rtc;
    bool      ram_dirty;

    /* Mapper state */
    bool      ram_enabled;
    uint16_t  rom_bank;       /* Active bank in 0x4000-0x7FFF */
    uint8_t   ram_bank;       /* Active RAM bank (or RTC reg 0x08-0x0C for MBC3) */
    uint8_t   mbc1_mode;      /* 0 = ROM banking, 1 = RAM/upper-ROM banking */
    uint8_t   mbc1_bank_hi;   /* 2-bit upper bank register */
    uint8_t   rtc_regs[5];    /* S, M, H, DL, DH */
    uint8_t   rtc_latch;

    char      title[17];
    uint32_t  crc32;
    uint8_t   raw_cart_type;
} GbCart;

AppResult cart_load_buffer(GbCart *cart, const uint8_t *data, size_t size);
AppResult cart_load_file(GbCart *cart, const char *abs_path);
void      cart_unload(GbCart *cart);
void      cart_reset(GbCart *cart);

uint8_t   cart_read8(const GbCart *cart, uint16_t addr);
void      cart_write8(GbCart *cart, uint16_t addr, uint8_t val);

AppResult cart_load_sram(GbCart *cart, const char *sav_path);
AppResult cart_save_sram(GbCart *cart, const char *sav_path);

#endif /* PS4_GB_JIT_CART_H */
