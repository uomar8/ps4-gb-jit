#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/gb_machine.h"
#include "core/mem/bus.h"
#include "core/mem/cart.h"
#include "core/hle/timer.h"
#include "core/hle/ppu.h"
#include "core/hle/apu.h"
#include "debug/dbg_log.h"
#include "debug/dbg_stub.h"
#include "hal/hal.h"
#include "hal/mock/hal_mock_ctl.h"

static uint8_t *make_test_rom(size_t size, uint8_t cart_type, uint8_t rom_size_code, uint8_t ram_size_code) {
    uint8_t *rom = (uint8_t *)calloc(1, size);
    assert(rom != NULL);
    /* Fill each 16 KiB bank with a unique signature byte at offset 0x0150 */
    size_t banks = size / 0x4000U;
    for (size_t b = 0; b < banks; ++b) {
        rom[b * 0x4000U + 0x0150U] = (uint8_t)(b & 0xFFU);
        rom[b * 0x4000U + 0x0151U] = (uint8_t)((b >> 8) & 0xFFU);
    }
    memcpy(rom + 0x0134, "TESTROM", 7);
    rom[0x0147] = cart_type;
    rom[0x0148] = rom_size_code;
    rom[0x0149] = ram_size_code;
    return rom;
}

static void test_mbc_mappers(void) {
    /* 1. MBC0 (32 KiB ROM-only) */
    {
        uint8_t *rom = make_test_rom(32768, 0x00, 0x00, 0x00);
        GbCart cart;
        memset(&cart, 0, sizeof(cart));
        assert(cart_load_buffer(&cart, rom, 32768) == APP_OK);
        assert(cart.mbc == GB_MBC_NONE);
        assert(cart_read8(&cart, 0x0150) == 0);
        assert(cart_read8(&cart, 0x4150) == 1);
        cart_unload(&cart);
        free(rom);
    }

    /* 2. MBC1 (128 KiB = 8 banks, 32 KiB RAM = 4 banks) */
    {
        uint8_t *rom = make_test_rom(128 * 1024, 0x03, 0x02, 0x03);
        GbCart cart;
        memset(&cart, 0, sizeof(cart));
        assert(cart_load_buffer(&cart, rom, 128 * 1024) == APP_OK);
        assert(cart.mbc == GB_MBC_1);
        assert(cart.has_battery == true);
        assert(cart_read8(&cart, 0x4150) == 1);

        /* Bank 0 write remaps to bank 1 */
        cart_write8(&cart, 0x2000, 0x00);
        assert(cart_read8(&cart, 0x4150) == 1);

        /* Switch to bank 5 */
        cart_write8(&cart, 0x2000, 0x05);
        assert(cart_read8(&cart, 0x4150) == 5);

        /* RAM disabled by default -> reads 0xFF */
        assert(cart_read8(&cart, 0xA000) == 0xFF);
        /* Enable RAM + bank mode 1 */
        cart_write8(&cart, 0x0000, 0x0A);
        cart_write8(&cart, 0x6000, 0x01);
        cart_write8(&cart, 0x4000, 0x02);
        cart_write8(&cart, 0xA010, 0x5A);
        assert(cart_read8(&cart, 0xA010) == 0x5A);
        assert(cart.ram_dirty == true);

        /* SRAM save/load round-trip */
        char sav_path[256];
        snprintf(sav_path, sizeof(sav_path), "%s/saves/test_mbc1.sav", hal_fs_get_data_dir());
        assert(cart_save_sram(&cart, sav_path) == APP_OK);
        memset(cart.ram, 0, cart.ram_size);
        assert(cart_read8(&cart, 0xA010) == 0x00);
        assert(cart_load_sram(&cart, sav_path) == APP_OK);
        assert(cart_read8(&cart, 0xA010) == 0x5A);

        cart_unload(&cart);
        free(rom);
    }

    /* 3. MBC2 (64 KiB ROM, 512x4-bit built-in RAM) */
    {
        uint8_t *rom = make_test_rom(64 * 1024, 0x06, 0x01, 0x00);
        GbCart cart;
        memset(&cart, 0, sizeof(cart));
        assert(cart_load_buffer(&cart, rom, 64 * 1024) == APP_OK);
        assert(cart.mbc == GB_MBC_2);
        cart_write8(&cart, 0x0000, 0x0A); /* bit 8 == 0 -> RAM enable */
        cart_write8(&cart, 0x0100, 0x03); /* bit 8 == 1 -> ROM bank 3 */
        assert(cart_read8(&cart, 0x4150) == 3);
        cart_write8(&cart, 0xA005, 0x0C);
        assert((cart_read8(&cart, 0xA005) & 0x0F) == 0x0C);
        assert((cart_read8(&cart, 0xA205) & 0x0F) == 0x0C); /* 512B mirror */
        cart_unload(&cart);
        free(rom);
    }

    /* 4. MBC3 (128 KiB ROM, 32 KiB RAM + RTC) */
    {
        uint8_t *rom = make_test_rom(128 * 1024, 0x10, 0x02, 0x03);
        GbCart cart;
        memset(&cart, 0, sizeof(cart));
        assert(cart_load_buffer(&cart, rom, 128 * 1024) == APP_OK);
        assert(cart.mbc == GB_MBC_3);
        assert(cart.has_rtc == true);
        cart_write8(&cart, 0x2000, 0x07);
        assert(cart_read8(&cart, 0x4150) == 7);
        cart_write8(&cart, 0x0000, 0x0A);
        cart_write8(&cart, 0x4000, 0x08); /* RTC seconds */
        cart_write8(&cart, 0xA000, 42);
        assert(cart_read8(&cart, 0xA000) == 42);
        cart_unload(&cart);
        free(rom);
    }

    /* 5. MBC5 (512 KiB = 32 banks, bank 0 allowed in 0x4000) */
    {
        uint8_t *rom = make_test_rom(512 * 1024, 0x1B, 0x04, 0x03);
        GbCart cart;
        memset(&cart, 0, sizeof(cart));
        assert(cart_load_buffer(&cart, rom, 512 * 1024) == APP_OK);
        assert(cart.mbc == GB_MBC_5);
        cart_write8(&cart, 0x2000, 0x00);
        cart_write8(&cart, 0x3000, 0x00);
        assert(cart_read8(&cart, 0x4150) == 0); /* MBC5 allows bank 0 in ROMX */
        cart_write8(&cart, 0x2000, 0x19);
        assert(cart_read8(&cart, 0x4150) == 25);
        cart_unload(&cart);
        free(rom);
    }
}

static void test_timer_ppu_apu_savestate(void) {
    uint8_t *rom = make_test_rom(32768, 0x00, 0x00, 0x00);
    GbMachine *m = (GbMachine *)calloc(1, sizeof(GbMachine));
    assert(m != NULL);
    gb_machine_init(m);
    assert(gb_machine_load_rom_buffer(m, rom, 32768) == APP_OK);

    /* Timer overflow -> IF bit 2 (0x04) */
    m->if_reg = 0;
    bus_write8(m, 0xFF06, 0x80); /* TMA = 0x80 */
    bus_write8(m, 0xFF05, 0xFF); /* TIMA = 0xFF */
    bus_write8(m, 0xFF07, 0x05); /* TAC enable, 16 T-cycles/tick */
    hle_timer_step(&m->timer, 32, &m->if_reg);
    assert((m->if_reg & 0x04U) != 0);
    assert(m->timer.tima >= 0x80U);

    /* OAM DMA transfer */
    for (int i = 0; i < 160; ++i) m->wram[i] = (uint8_t)(i ^ 0xA5);
    bus_write8(m, 0xFF46, 0xC0);
    for (int i = 0; i < 160; ++i) assert(m->ppu.oam[i] == (uint8_t)(i ^ 0xA5));

    /* PPU full frame (70224 T-cycles) triggers VBlank IRQ + renders BG */
    m->if_reg = 0;
    /* Put a non-empty tile 1 in VRAM (solid color 3) and map it at (0,0) */
    for (int b = 0; b < 16; ++b) m->ppu.vram[16 + b] = 0xFF;
    m->ppu.vram[0x1800] = 1;
    bus_write8(m, 0xFF47, 0xE4); /* BGP */
    bus_write8(m, 0xFF40, 0x91); /* LCDC on, BG on, 0x8000 tile data */
    hle_ppu_step(&m->ppu, 70224, &m->if_reg);
    assert((m->if_reg & 0x01U) != 0);
    assert(m->ppu.frame_ready == true);
    assert(m->ppu.fb_shades[0] == 3);

    /* Test 1080p 7x integer blit */
    uint32_t *fb = (uint32_t *)calloc((size_t)APP_FB_W * APP_FB_H, sizeof(uint32_t));
    assert(fb != NULL);
    hle_ppu_blit_1080p(&m->ppu, 0, fb, APP_FB_W, APP_FB_H, APP_FB_W);
    /* Top-left of 7x centered viewport: off_x=400, off_y=36 */
    assert(fb[36 * APP_FB_W + 400] == 0xFF0F380FU);
    free(fb);

    /* APU generates 48 kHz samples when Ch1 triggered */
    bus_write8(m, 0xFF26, 0x80); /* Master power on */
    bus_write8(m, 0xFF24, 0x77); /* Max vol L/R */
    bus_write8(m, 0xFF25, 0xFF); /* All channels L/R */
    bus_write8(m, 0xFF11, 0x80); /* 50% duty */
    bus_write8(m, 0xFF12, 0xF0); /* Vol 15 */
    bus_write8(m, 0xFF13, 0x00);
    bus_write8(m, 0xFF14, 0x87); /* Trigger */
    m->apu.out_pos = 0;
    hle_apu_step(&m->apu, 70224);
    assert(m->apu.out_pos > 0);
    bool nonzero = false;
    for (uint32_t i = 0; i < m->apu.out_pos * 2U; ++i) {
        if (m->apu.out_buf[i] != 0) { nonzero = true; break; }
    }
    assert(nonzero);

    /* Save State round-trip */
    m->cpu.a = 0x77;
    m->cpu.pc = 0x1234;
    m->wram[42] = 0xBE;
    char st_path[256];
    snprintf(st_path, sizeof(st_path), "%s/saves/test_state.st0", hal_fs_get_data_dir());
    assert(gb_machine_save_state(m, st_path) == APP_OK);

    m->cpu.a = 0x00;
    m->cpu.pc = 0x0100;
    m->wram[42] = 0x00;
    assert(gb_machine_load_state(m, st_path) == APP_OK);
    assert(m->cpu.a == 0x77);
    assert(m->cpu.pc == 0x1234);
    assert(m->wram[42] == 0xBE);

    gb_machine_destroy(m);
    free(m);
    free(rom);
}

int main(void) {
    assert(hal_sys_init() == APP_OK);
    hal_mock_fs_set_app_dir("/tmp/ps4_gb_jit_test_mbc_hle");
    char saves_dir[256], log_path[256];
    snprintf(saves_dir, sizeof(saves_dir), "%s/saves", hal_fs_get_data_dir());
    snprintf(log_path, sizeof(log_path), "%s/log.txt", hal_fs_get_data_dir());
    assert(hal_fs_mkdir_p(saves_dir) == APP_OK);
    dbg_log_init(log_path, DBG_LVL_WARN);
    dbg_stub_reset();

    test_mbc_mappers();
    test_timer_ppu_apu_savestate();

    dbg_log_shutdown();
    hal_sys_shutdown();
    printf("[PASS] test_mbc_hle: MBC0/1/2/3/5, SRAM, Timer, PPU 7x blit, APU 48kHz, and SaveState passed!\n");
    return 0;
}
