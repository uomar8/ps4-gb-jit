#include "timer.h"

static uint16_t timer_bit_mask(uint8_t tac) {
    switch (tac & 0x03U) {
        case 0: return (uint16_t)(1U << 9); /* 4096 Hz */
        case 1: return (uint16_t)(1U << 3); /* 262144 Hz */
        case 2: return (uint16_t)(1U << 5); /* 65536 Hz */
        case 3: return (uint16_t)(1U << 7); /* 16384 Hz */
        default: return (uint16_t)(1U << 9);
    }
}

static bool timer_signal(const GbTimer *t) {
    return ((t->tac & 0x04U) != 0) && ((t->div_counter & timer_bit_mask(t->tac)) != 0);
}

static void timer_inc_tima(GbTimer *t, uint8_t *if_reg) {
    if (t->tima == 0xFFU) {
        t->tima = t->tma;
        if (if_reg) {
            *if_reg |= 0x04U; /* Timer IRQ */
        }
    } else {
        t->tima++;
    }
}

void hle_timer_reset(GbTimer *t) {
    if (!t) return;
    /* DMG-01 post-boot state */
    t->div_counter = 0xABCCU;
    t->tima = 0x00U;
    t->tma  = 0x00U;
    t->tac  = 0xF8U;
}

uint8_t hle_timer_read8(const GbTimer *t, uint16_t addr) {
    switch (addr) {
        case 0xFF04U: return (uint8_t)(t->div_counter >> 8);
        case 0xFF05U: return t->tima;
        case 0xFF06U: return t->tma;
        case 0xFF07U: return (uint8_t)(t->tac | 0xF8U);
        default:      return 0xFFU;
    }
}

void hle_timer_write8(GbTimer *t, uint16_t addr, uint8_t val, uint8_t *if_reg) {
    bool old_sig = timer_signal(t);
    switch (addr) {
        case 0xFF04U:
            t->div_counter = 0;
            if (old_sig && !timer_signal(t)) {
                timer_inc_tima(t, if_reg);
            }
            break;
        case 0xFF05U:
            t->tima = val;
            break;
        case 0xFF06U:
            t->tma = val;
            break;
        case 0xFF07U:
            t->tac = (uint8_t)((val & 0x07U) | 0xF8U);
            if (old_sig && !timer_signal(t)) {
                timer_inc_tima(t, if_reg);
            }
            break;
        default:
            break;
    }
}

void hle_timer_step(GbTimer *t, uint32_t t_cycles, uint8_t *if_reg) {
    if ((t->tac & 0x04U) == 0) {
        t->div_counter = (uint16_t)(t->div_counter + (uint16_t)t_cycles);
        return;
    }

    uint16_t mask = timer_bit_mask(t->tac);
    for (uint32_t i = 0; i < t_cycles; i++) {
        bool old_bit = (t->div_counter & mask) != 0;
        t->div_counter++;
        bool new_bit = (t->div_counter & mask) != 0;
        if (old_bit && !new_bit) {
            timer_inc_tima(t, if_reg);
        }
    }
}
