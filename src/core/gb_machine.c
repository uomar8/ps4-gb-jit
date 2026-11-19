#include "gb_machine.h"
#include "../hal/hal.h"
#include "../debug/dbg_log.h"

#include <stdlib.h>
#include <string.h>

void gb_machine_init(GbMachine *m) {
    if (!m) return;
    memset(m, 0, sizeof(*m));
    gb_machine_reset(m);
}

void gb_machine_destroy(GbMachine *m) {
    if (!m) return;
    cart_unload(&m->cart);
    m->rom_loaded = false;
}

void gb_machine_reset(GbMachine *m) {
    if (!m) return;

    /* DMG-01 Boot ROM HLE post-boot register state */
    memset(&m->cpu, 0, sizeof(m->cpu));
    m->cpu.a  = 0x01U;
    m->cpu.f  = 0xB0U;
    m->cpu.b  = 0x00U;
    m->cpu.c  = 0x13U;
    m->cpu.d  = 0x00U;
    m->cpu.e  = 0xD8U;
    m->cpu.h  = 0x01U;
    m->cpu.l  = 0x4DU;
    m->cpu.sp = 0xFFFEU;
    m->cpu.pc = 0x0100U;

    memset(m->wram, 0, sizeof(m->wram));
    memset(m->hram, 0, sizeof(m->hram));
    m->if_reg = 0xE1U;
    m->ie_reg = 0x00U;
    m->joyp_sel = 0x30U;
    m->joyp_keys = 0x00U;
    m->sb = 0x00U;
    m->sc = 0x7EU;

    cart_reset(&m->cart);
    hle_ppu_reset(&m->ppu);
    hle_apu_reset(&m->apu);
    hle_timer_reset(&m->timer);

    m->serial_buf[0] = '\0';
    m->serial_len = 0;
    m->total_cycles = 0;
}

AppResult gb_machine_load_rom_buffer(GbMachine *m, const uint8_t *data, size_t size) {
    AppResult rc = cart_load_buffer(&m->cart, data, size);
    if (rc != APP_OK) {
        m->rom_loaded = false;
        return rc;
    }
    m->rom_loaded = true;
    gb_machine_reset(m);
    return APP_OK;
}

AppResult gb_machine_load_rom_file(GbMachine *m, const char *abs_path) {
    AppResult rc = cart_load_file(&m->cart, abs_path);
    if (rc != APP_OK) {
        m->rom_loaded = false;
        return rc;
    }
    m->rom_loaded = true;
    gb_machine_reset(m);
    return APP_OK;
}

void gb_machine_set_keys(GbMachine *m, uint8_t keys) {
    uint8_t newly_pressed = (uint8_t)(keys & (uint8_t)~m->joyp_keys);
    m->joyp_keys = keys;
    if (newly_pressed != 0) {
        m->if_reg |= 0x10U; /* Joypad IRQ */
    }
}

void gb_machine_step_subsystems(GbMachine *m, uint32_t t_cycles) {
    m->total_cycles += t_cycles;
    hle_timer_step(&m->timer, t_cycles, &m->if_reg);
    hle_ppu_step(&m->ppu, t_cycles, &m->if_reg);
    hle_apu_step(&m->apu, t_cycles);
}

typedef struct GbStateCoreBlob {
    GbCpu   cpu;
    uint8_t wram[8192];
    uint8_t hram[127];
    uint8_t if_reg;
    uint8_t ie_reg;
    uint8_t joyp_sel;
    uint8_t sb;
    uint8_t sc;
    GbTimer timer;
    GbPpu   ppu;
    GbApu   apu;
    bool     ram_enabled;
    uint16_t rom_bank;
    uint8_t  ram_bank;
    uint8_t  mbc1_mode;
    uint8_t  mbc1_bank_hi;
    uint8_t  rtc_regs[5];
    uint8_t  rtc_latch;
} GbStateCoreBlob;

AppResult gb_machine_save_state(const GbMachine *m, const char *abs_path) {
    if (!m || !m->rom_loaded || !abs_path) {
        return APP_ERR_INVALID_ARG;
    }
    size_t total = sizeof(GbSaveStateHeader) + sizeof(GbStateCoreBlob) + m->cart.ram_size;
    uint8_t *buf = (uint8_t *)malloc(total);
    if (!buf) {
        return APP_ERR_NOMEM;
    }

    GbSaveStateHeader hdr;
    memset(&hdr, 0, sizeof(hdr));
    memcpy(hdr.magic, "GBJT", 4);
    hdr.version   = 1;
    hdr.rom_crc32 = m->cart.crc32;
    hdr.cpu_size  = (uint32_t)sizeof(GbStateCoreBlob);
    hdr.mem_size  = (uint32_t)m->cart.ram_size;

    GbStateCoreBlob *blob = (GbStateCoreBlob *)(buf + sizeof(GbSaveStateHeader));
    memset(blob, 0, sizeof(*blob));
    blob->cpu          = m->cpu;
    memcpy(blob->wram, m->wram, sizeof(m->wram));
    memcpy(blob->hram, m->hram, sizeof(m->hram));
    blob->if_reg       = m->if_reg;
    blob->ie_reg       = m->ie_reg;
    blob->joyp_sel     = m->joyp_sel;
    blob->sb           = m->sb;
    blob->sc           = m->sc;
    blob->timer        = m->timer;
    blob->ppu          = m->ppu;
    blob->apu          = m->apu;
    blob->ram_enabled  = m->cart.ram_enabled;
    blob->rom_bank     = m->cart.rom_bank;
    blob->ram_bank     = m->cart.ram_bank;
    blob->mbc1_mode    = m->cart.mbc1_mode;
    blob->mbc1_bank_hi = m->cart.mbc1_bank_hi;
    memcpy(blob->rtc_regs, m->cart.rtc_regs, sizeof(blob->rtc_regs));
    blob->rtc_latch    = m->cart.rtc_latch;

    memcpy(buf, &hdr, sizeof(hdr));
    if (m->cart.ram && m->cart.ram_size > 0) {
        memcpy(buf + sizeof(GbSaveStateHeader) + sizeof(GbStateCoreBlob),
               m->cart.ram, m->cart.ram_size);
    }

    AppResult rc = hal_fs_write_file(abs_path, buf, total);
    free(buf);
    if (rc == APP_OK) {
        LOG_INFO("SYS", "saved state path=%s bytes=%zu", abs_path, total);
    }
    return rc;
}

AppResult gb_machine_load_state(GbMachine *m, const char *abs_path) {
    if (!m || !m->rom_loaded || !abs_path) {
        return APP_ERR_INVALID_ARG;
    }
    uint8_t *buf = NULL;
    size_t sz = 0;
    AppResult rc = hal_fs_read_file(abs_path, &buf, &sz);
    if (rc != APP_OK) {
        return rc;
    }
    if (sz < sizeof(GbSaveStateHeader) + sizeof(GbStateCoreBlob)) {
        free(buf);
        return APP_ERR_BAD_FORMAT;
    }

    GbSaveStateHeader hdr;
    memcpy(&hdr, buf, sizeof(hdr));
    if (memcmp(hdr.magic, "GBJT", 4) != 0 || hdr.version != 1 ||
        hdr.rom_crc32 != m->cart.crc32 || hdr.cpu_size != sizeof(GbStateCoreBlob)) {
        free(buf);
        return APP_ERR_BAD_FORMAT;
    }

    const GbStateCoreBlob *blob = (const GbStateCoreBlob *)(buf + sizeof(GbSaveStateHeader));
    m->cpu          = blob->cpu;
    memcpy(m->wram, blob->wram, sizeof(m->wram));
    memcpy(m->hram, blob->hram, sizeof(m->hram));
    m->if_reg       = blob->if_reg;
    m->ie_reg       = blob->ie_reg;
    m->joyp_sel     = blob->joyp_sel;
    m->sb           = blob->sb;
    m->sc           = blob->sc;
    m->timer        = blob->timer;
    m->ppu          = blob->ppu;
    m->apu          = blob->apu;
    m->cart.ram_enabled  = blob->ram_enabled;
    m->cart.rom_bank     = blob->rom_bank;
    m->cart.ram_bank     = blob->ram_bank;
    m->cart.mbc1_mode    = blob->mbc1_mode;
    m->cart.mbc1_bank_hi = blob->mbc1_bank_hi;
    memcpy(m->cart.rtc_regs, blob->rtc_regs, sizeof(m->cart.rtc_regs));
    m->cart.rtc_latch    = blob->rtc_latch;

    if (m->cart.ram && hdr.mem_size == m->cart.ram_size &&
        sz >= sizeof(GbSaveStateHeader) + sizeof(GbStateCoreBlob) + m->cart.ram_size) {
        memcpy(m->cart.ram,
               buf + sizeof(GbSaveStateHeader) + sizeof(GbStateCoreBlob),
               m->cart.ram_size);
    }

    free(buf);
    LOG_INFO("SYS", "loaded state path=%s", abs_path);
    return APP_OK;
}
