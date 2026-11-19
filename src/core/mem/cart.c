#include "cart.h"
#include "../../hal/hal.h"
#include "../../debug/dbg_log.h"
#include "../../debug/dbg_stub.h"

#include <stdlib.h>
#include <string.h>

static uint32_t cart_crc32(const uint8_t *data, size_t len) {
    uint32_t crc = 0xFFFFFFFFU;
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (int b = 0; b < 8; b++) {
            uint32_t mask = (uint32_t)-(int32_t)(crc & 1U);
            crc = (crc >> 1) ^ (0xEDB88320U & mask);
        }
    }
    return ~crc;
}

static size_t cart_ram_size_from_code(uint8_t code) {
    switch (code) {
        case 0x00: return 0;
        case 0x01: return 2048;
        case 0x02: return 8192;
        case 0x03: return 32768;
        case 0x04: return 131072;
        case 0x05: return 65536;
        default:   return 0;
    }
}

void cart_reset(GbCart *cart) {
    if (!cart) return;
    cart->ram_enabled = false;
    cart->rom_bank = 1;
    cart->ram_bank = 0;
    cart->mbc1_mode = 0;
    cart->mbc1_bank_hi = 0;
    memset(cart->rtc_regs, 0, sizeof(cart->rtc_regs));
    cart->rtc_latch = 0;
}

AppResult cart_load_buffer(GbCart *cart, const uint8_t *data, size_t size) {
    if (!cart || !data || size < 0x150) {
        return APP_ERR_BAD_FORMAT;
    }
    cart_unload(cart);

    /* Pad to at least 32 KiB (2 banks) and multiple of 16 KiB */
    size_t alloc_sz = size < 32768 ? 32768 : ((size + 16383U) & ~16383U);
    cart->rom = (uint8_t *)malloc(alloc_sz);
    if (!cart->rom) {
        return APP_ERR_NOMEM;
    }
    memset(cart->rom, 0xFF, alloc_sz);
    memcpy(cart->rom, data, size);
    cart->rom_size = alloc_sz;
    cart->rom_banks = (uint32_t)(alloc_sz / 16384U);
    if (cart->rom_banks == 0) cart->rom_banks = 1;

    /* Extract ASCII title */
    for (int i = 0; i < 16; i++) {
        uint8_t c = cart->rom[0x0134 + i];
        cart->title[i] = (c >= 0x20 && c <= 0x7E) ? (char)c : '\0';
    }
    cart->title[16] = '\0';

    uint8_t ct = cart->rom[0x0147];
    cart->raw_cart_type = ct;
    cart->has_battery = false;
    cart->has_rtc = false;

    switch (ct) {
        case 0x00: cart->mbc = GB_MBC_NONE; break;
        case 0x01: cart->mbc = GB_MBC_1; break;
        case 0x02: cart->mbc = GB_MBC_1; break;
        case 0x03: cart->mbc = GB_MBC_1; cart->has_battery = true; break;
        case 0x05: cart->mbc = GB_MBC_2; break;
        case 0x06: cart->mbc = GB_MBC_2; cart->has_battery = true; break;
        case 0x0F: cart->mbc = GB_MBC_3; cart->has_battery = true; cart->has_rtc = true; break;
        case 0x10: cart->mbc = GB_MBC_3; cart->has_battery = true; cart->has_rtc = true; break;
        case 0x11: cart->mbc = GB_MBC_3; break;
        case 0x12: cart->mbc = GB_MBC_3; break;
        case 0x13: cart->mbc = GB_MBC_3; cart->has_battery = true; break;
        case 0x19: cart->mbc = GB_MBC_5; break;
        case 0x1A: cart->mbc = GB_MBC_5; break;
        case 0x1B: cart->mbc = GB_MBC_5; cart->has_battery = true; break;
        case 0x1C: cart->mbc = GB_MBC_5; break;
        case 0x1D: cart->mbc = GB_MBC_5; break;
        case 0x1E: cart->mbc = GB_MBC_5; cart->has_battery = true; break;
        default:
            dbg_stub_record("MBC", ct, "UNSUPPORTED_MAPPER_FALLBACK_MBC0");
            cart->mbc = GB_MBC_NONE;
            break;
    }

    size_t rsz = (cart->mbc == GB_MBC_2) ? 512U : cart_ram_size_from_code(cart->rom[0x0149]);
    if (rsz > 0) {
        cart->ram = (uint8_t *)calloc(1, rsz);
        if (!cart->ram) {
            cart_unload(cart);
            return APP_ERR_NOMEM;
        }
        cart->ram_size = rsz;
        cart->ram_banks = (uint32_t)(rsz / 8192U);
        if (cart->ram_banks == 0) cart->ram_banks = 1;
    }

    cart->crc32 = cart_crc32(data, size);
    cart_reset(cart);
    LOG_INFO("CART", "loaded title='%s' mbc=%d type=0x%02x rom=%zuB ram=%zuB crc=0x%08x",
             cart->title, (int)cart->mbc, ct, cart->rom_size, cart->ram_size, cart->crc32);
    return APP_OK;
}

AppResult cart_load_file(GbCart *cart, const char *abs_path) {
    uint8_t *buf = NULL;
    size_t sz = 0;
    AppResult rc = hal_fs_read_file(abs_path, &buf, &sz);
    if (rc != APP_OK) {
        LOG_RC_ERROR("CART", "cart_load_file read failed", rc);
        return rc;
    }
    rc = cart_load_buffer(cart, buf, sz);
    free(buf);
    return rc;
}

void cart_unload(GbCart *cart) {
    if (!cart) return;
    free(cart->rom);
    free(cart->ram);
    memset(cart, 0, sizeof(*cart));
}

static uint32_t cart_eff_rom_bank0(const GbCart *cart) {
    if (cart->mbc == GB_MBC_1 && cart->mbc1_mode == 1) {
        uint32_t b = (uint32_t)(cart->mbc1_bank_hi << 5);
        return b % cart->rom_banks;
    }
    return 0;
}

static uint32_t cart_eff_rom_bankx(const GbCart *cart) {
    uint32_t b = cart->rom_bank;
    if (cart->mbc == GB_MBC_1) {
        b = ((uint32_t)cart->mbc1_bank_hi << 5) | (b & 0x1FU);
    }
    return b % cart->rom_banks;
}

uint8_t cart_read8(const GbCart *cart, uint16_t addr) {
    if (!cart || !cart->rom) {
        return 0xFF;
    }
    if (addr < 0x4000U) {
        uint32_t bank0 = cart_eff_rom_bank0(cart);
        return cart->rom[bank0 * 16384U + addr];
    }
    if (addr < 0x8000U) {
        uint32_t bankx = cart_eff_rom_bankx(cart);
        return cart->rom[bankx * 16384U + (addr - 0x4000U)];
    }
    if (addr >= 0xA000U && addr < 0xC000U) {
        if (!cart->ram_enabled) {
            return 0xFF;
        }
        if (cart->mbc == GB_MBC_2) {
            return (uint8_t)(cart->ram[(addr - 0xA000U) & 0x1FFU] | 0xF0U);
        }
        if (cart->mbc == GB_MBC_3 && cart->ram_bank >= 0x08U && cart->ram_bank <= 0x0CU) {
            return cart->rtc_regs[cart->ram_bank - 0x08U];
        }
        if (!cart->ram || cart->ram_size == 0) {
            return 0xFF;
        }
        uint32_t rb = (cart->mbc == GB_MBC_1 && cart->mbc1_mode == 0) ? 0U : (cart->ram_bank % cart->ram_banks);
        size_t off = (size_t)rb * 8192U + (size_t)(addr - 0xA000U);
        return (off < cart->ram_size) ? cart->ram[off] : 0xFF;
    }
    return 0xFF;
}

void cart_write8(GbCart *cart, uint16_t addr, uint8_t val) {
    if (!cart || !cart->rom) {
        return;
    }

    if (addr < 0x8000U) {
        switch (cart->mbc) {
            case GB_MBC_NONE:
                break;
            case GB_MBC_1:
                if (addr < 0x2000U) {
                    cart->ram_enabled = ((val & 0x0FU) == 0x0AU);
                } else if (addr < 0x4000U) {
                    uint8_t b = val & 0x1FU;
                    if (b == 0) b = 1;
                    cart->rom_bank = b;
                } else if (addr < 0x6000U) {
                    cart->mbc1_bank_hi = val & 0x03U;
                    cart->ram_bank = val & 0x03U;
                } else {
                    cart->mbc1_mode = val & 0x01U;
                }
                break;
            case GB_MBC_2:
                if (addr < 0x4000U) {
                    if ((addr & 0x0100U) == 0) {
                        cart->ram_enabled = ((val & 0x0FU) == 0x0AU);
                    } else {
                        uint8_t b = val & 0x0FU;
                        if (b == 0) b = 1;
                        cart->rom_bank = b;
                    }
                }
                break;
            case GB_MBC_3:
                if (addr < 0x2000U) {
                    cart->ram_enabled = ((val & 0x0FU) == 0x0AU);
                } else if (addr < 0x4000U) {
                    uint8_t b = val & 0x7FU;
                    if (b == 0) b = 1;
                    cart->rom_bank = b;
                } else if (addr < 0x6000U) {
                    cart->ram_bank = val;
                } else {
                    cart->rtc_latch = val;
                }
                break;
            case GB_MBC_5:
                if (addr < 0x2000U) {
                    cart->ram_enabled = ((val & 0x0FU) == 0x0AU);
                } else if (addr < 0x3000U) {
                    cart->rom_bank = (uint16_t)((cart->rom_bank & 0x0100U) | val);
                } else if (addr < 0x4000U) {
                    cart->rom_bank = (uint16_t)((cart->rom_bank & 0x00FFU) | ((uint16_t)(val & 1U) << 8));
                } else if (addr < 0x6000U) {
                    cart->ram_bank = val & 0x0FU;
                }
                break;
        }
        return;
    }

    if (addr >= 0xA000U && addr < 0xC000U) {
        if (!cart->ram_enabled) {
            return;
        }
        if (cart->mbc == GB_MBC_2 && cart->ram) {
            cart->ram[(addr - 0xA000U) & 0x1FFU] = (uint8_t)(val & 0x0FU);
            cart->ram_dirty = true;
            return;
        }
        if (cart->mbc == GB_MBC_3 && cart->ram_bank >= 0x08U && cart->ram_bank <= 0x0CU) {
            cart->rtc_regs[cart->ram_bank - 0x08U] = val;
            return;
        }
        if (!cart->ram || cart->ram_size == 0) {
            return;
        }
        uint32_t rb = (cart->mbc == GB_MBC_1 && cart->mbc1_mode == 0) ? 0U : (cart->ram_bank % cart->ram_banks);
        size_t off = (size_t)rb * 8192U + (size_t)(addr - 0xA000U);
        if (off < cart->ram_size) {
            cart->ram[off] = val;
            cart->ram_dirty = true;
        }
    }
}

AppResult cart_load_sram(GbCart *cart, const char *sav_path) {
    if (!cart || !cart->has_battery || !cart->ram || cart->ram_size == 0) {
        return APP_OK;
    }
    uint8_t *buf = NULL;
    size_t sz = 0;
    AppResult rc = hal_fs_read_file(sav_path, &buf, &sz);
    if (rc != APP_OK) {
        return rc;
    }
    size_t copy = sz < cart->ram_size ? sz : cart->ram_size;
    memcpy(cart->ram, buf, copy);
    free(buf);
    cart->ram_dirty = false;
    return APP_OK;
}

AppResult cart_save_sram(GbCart *cart, const char *sav_path) {
    if (!cart || !cart->has_battery || !cart->ram || cart->ram_size == 0 || !cart->ram_dirty) {
        return APP_OK;
    }
    AppResult rc = hal_fs_write_file(sav_path, cart->ram, cart->ram_size);
    if (rc == APP_OK) {
        cart->ram_dirty = false;
    }
    return rc;
}
