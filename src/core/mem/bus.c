#include "bus.h"
#include "../../debug/dbg_stub.h"

static uint8_t bus_read_joyp(const GbMachine *m) {
    uint8_t res = (uint8_t)(0xC0U | (m->joyp_sel & 0x30U) | 0x0FU);
    /* Bit 4 == 0 selects D-Pad */
    if ((m->joyp_sel & 0x10U) == 0) {
        if (m->joyp_keys & GB_KEY_RIGHT) res &= (uint8_t)~0x01U;
        if (m->joyp_keys & GB_KEY_LEFT)  res &= (uint8_t)~0x02U;
        if (m->joyp_keys & GB_KEY_UP)    res &= (uint8_t)~0x04U;
        if (m->joyp_keys & GB_KEY_DOWN)  res &= (uint8_t)~0x08U;
    }
    /* Bit 5 == 0 selects Action buttons */
    if ((m->joyp_sel & 0x20U) == 0) {
        if (m->joyp_keys & GB_KEY_A)      res &= (uint8_t)~0x01U;
        if (m->joyp_keys & GB_KEY_B)      res &= (uint8_t)~0x02U;
        if (m->joyp_keys & GB_KEY_SELECT) res &= (uint8_t)~0x04U;
        if (m->joyp_keys & GB_KEY_START)  res &= (uint8_t)~0x08U;
    }
    return res;
}

static void bus_do_oam_dma(GbMachine *m, uint8_t page) {
    uint16_t src = (uint16_t)((uint16_t)page << 8);
    for (uint16_t i = 0; i < 160U; i++) {
        m->ppu.oam[i] = bus_read8(m, (uint16_t)(src + i));
    }
}

uint8_t bus_read8(GbMachine *m, uint16_t addr) {
    if (addr < 0x8000U) {
        return cart_read8(&m->cart, addr);
    }
    if (addr < 0xA000U) {
        return m->ppu.vram[addr - 0x8000U];
    }
    if (addr < 0xC000U) {
        return cart_read8(&m->cart, addr);
    }
    if (addr < 0xE000U) {
        return m->wram[addr - 0xC000U];
    }
    if (addr < 0xFE00U) {
        /* Echo RAM mirrors C000-DDFF */
        return m->wram[addr - 0xE000U];
    }
    if (addr < 0xFEA0U) {
        return m->ppu.oam[addr - 0xFE00U];
    }
    if (addr < 0xFF00U) {
        return 0xFFU;
    }
    if (addr < 0xFF80U) {
        if (addr == 0xFF00U) return bus_read_joyp(m);
        if (addr == 0xFF01U) return m->sb;
        if (addr == 0xFF02U) return (uint8_t)(m->sc | 0x7EU);
        if (addr >= 0xFF04U && addr <= 0xFF07U) return hle_timer_read8(&m->timer, addr);
        if (addr == 0xFF0FU) return (uint8_t)(m->if_reg | 0xE0U);
        if (addr >= 0xFF10U && addr <= 0xFF3FU) return hle_apu_read8(&m->apu, addr);
        if (addr >= 0xFF40U && addr <= 0xFF4BU) return hle_ppu_read8(&m->ppu, addr);
        /* Record GBC/unimplemented MMIO accesses in Stub-Discovery */
        if (addr == 0xFF4DU || addr == 0xFF4FU || addr == 0xFF70U) {
            dbg_stub_record("MMIO_RD", addr, "GBC_ONLY_REGISTER");
        }
        return 0xFFU;
    }
    if (addr < 0xFFFFU) {
        return m->hram[addr - 0xFF80U];
    }
    return m->ie_reg;
}

void bus_write8(GbMachine *m, uint16_t addr, uint8_t val) {
    if (addr < 0x8000U) {
        cart_write8(&m->cart, addr, val);
        return;
    }
    if (addr < 0xA000U) {
        m->ppu.vram[addr - 0x8000U] = val;
        return;
    }
    if (addr < 0xC000U) {
        cart_write8(&m->cart, addr, val);
        return;
    }
    if (addr < 0xE000U) {
        m->wram[addr - 0xC000U] = val;
        return;
    }
    if (addr < 0xFE00U) {
        m->wram[addr - 0xE000U] = val;
        return;
    }
    if (addr < 0xFEA0U) {
        m->ppu.oam[addr - 0xFE00U] = val;
        return;
    }
    if (addr < 0xFF00U) {
        return;
    }
    if (addr < 0xFF80U) {
        if (addr == 0xFF00U) {
            m->joyp_sel = (uint8_t)(val & 0x30U);
            return;
        }
        if (addr == 0xFF01U) {
            m->sb = val;
            return;
        }
        if (addr == 0xFF02U) {
            m->sc = val;
            if ((val & 0x81U) == 0x81U) {
                /* Capture serial output byte (used by Blargg cpu_instrs) */
                if (m->serial_len + 1 < sizeof(m->serial_buf)) {
                    m->serial_buf[m->serial_len++] = (char)m->sb;
                    m->serial_buf[m->serial_len] = '\0';
                }
                m->sc &= (uint8_t)~0x80U;
                m->sb = 0xFFU;
                m->if_reg |= 0x08U; /* Serial IRQ */
            }
            return;
        }
        if (addr >= 0xFF04U && addr <= 0xFF07U) {
            hle_timer_write8(&m->timer, addr, val, &m->if_reg);
            return;
        }
        if (addr == 0xFF0FU) {
            m->if_reg = (uint8_t)(val & 0x1FU);
            return;
        }
        if (addr >= 0xFF10U && addr <= 0xFF3FU) {
            hle_apu_write8(&m->apu, addr, val);
            return;
        }
        if (addr == 0xFF46U) {
            bus_do_oam_dma(m, val);
            return;
        }
        if (addr >= 0xFF40U && addr <= 0xFF4BU) {
            hle_ppu_write8(&m->ppu, addr, val, &m->if_reg);
            return;
        }
        if (addr == 0xFF4DU || addr == 0xFF4FU || addr == 0xFF70U) {
            dbg_stub_record("MMIO_WR", addr, "GBC_ONLY_REGISTER");
        }
        return;
    }
    if (addr < 0xFFFFU) {
        m->hram[addr - 0xFF80U] = val;
        return;
    }
    m->ie_reg = val;
}

uint16_t bus_read16(GbMachine *m, uint16_t addr) {
    uint16_t lo = bus_read8(m, addr);
    uint16_t hi = bus_read8(m, (uint16_t)(addr + 1U));
    return (uint16_t)(lo | (hi << 8));
}

void bus_write16(GbMachine *m, uint16_t addr, uint16_t val) {
    bus_write8(m, addr, (uint8_t)(val & 0xFFU));
    bus_write8(m, (uint16_t)(addr + 1U), (uint8_t)(val >> 8));
}
