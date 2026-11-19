#include "apu.h"
#include "../../hal/hal.h"

#include <string.h>

#define DMG_CLOCK_HZ 4194304U

static const uint8_t g_duty_table[4][8] = {
    {0, 0, 0, 0, 0, 0, 0, 1},
    {1, 0, 0, 0, 0, 0, 0, 1},
    {1, 0, 0, 0, 0, 1, 1, 1},
    {0, 1, 1, 1, 1, 1, 1, 0}
};

static const uint32_t g_noise_div[8] = { 8, 16, 32, 48, 64, 80, 96, 112 };

void hle_apu_reset(GbApu *a) {
    if (!a) return;
    memset(a, 0, sizeof(*a));
    a->power = true;
    a->nr50 = 0x77U;
    a->nr51 = 0xF3U;
    a->ch4.lfsr = 0x7FFFU;
}

uint8_t hle_apu_read8(const GbApu *a, uint16_t addr) {
    if (addr >= 0xFF30U && addr <= 0xFF3FU) {
        return a->ch3.ram[addr - 0xFF30U];
    }
    if (addr == 0xFF26U) {
        uint8_t s = a->power ? 0x80U : 0x00U;
        if (a->ch1.enabled) s |= 0x01U;
        if (a->ch2.enabled) s |= 0x02U;
        if (a->ch3.enabled) s |= 0x04U;
        if (a->ch4.enabled) s |= 0x08U;
        return (uint8_t)(s | 0x70U);
    }
    if (addr >= 0xFF10U && addr <= 0xFF25U) {
        return a->regs[addr - 0xFF10U];
    }
    return 0xFFU;
}

static void apu_trigger_pulse(GbApuPulse *c, bool has_sweep) {
    c->enabled = (c->vol_init > 0 || c->env_dir != 0);
    if (c->length_cnt == 0) c->length_cnt = 64;
    c->timer = (uint32_t)(2048U - c->freq) * 4U;
    c->vol = c->vol_init;
    c->env_timer = c->env_period ? c->env_period : 8;
    if (has_sweep) {
        c->sweep_shadow = c->freq;
        c->sweep_timer = c->sweep_period ? c->sweep_period : 8;
    }
}

void hle_apu_write8(GbApu *a, uint16_t addr, uint8_t val) {
    if (addr >= 0xFF30U && addr <= 0xFF3FU) {
        a->ch3.ram[addr - 0xFF30U] = val;
        return;
    }
    if (addr == 0xFF26U) {
        bool on = (val & 0x80U) != 0;
        if (!on) {
            uint8_t wave_backup[16];
            memcpy(wave_backup, a->ch3.ram, 16);
            memset(a, 0, sizeof(*a));
            memcpy(a->ch3.ram, wave_backup, 16);
            a->ch4.lfsr = 0x7FFFU;
        }
        a->power = on;
        return;
    }
    if (!a->power || addr < 0xFF10U || addr > 0xFF25U) {
        return;
    }
    a->regs[addr - 0xFF10U] = val;

    switch (addr) {
        /* Ch1 */
        case 0xFF10U:
            a->ch1.sweep_period = (val >> 4) & 7U;
            a->ch1.sweep_neg    = (val >> 3) & 1U;
            a->ch1.sweep_shift  = val & 7U;
            break;
        case 0xFF11U:
            a->ch1.duty = (val >> 6) & 3U;
            a->ch1.length_cnt = (uint8_t)(64U - (val & 0x3FU));
            break;
        case 0xFF12U:
            a->ch1.vol_init   = (val >> 4) & 0x0FU;
            a->ch1.env_dir    = (val >> 3) & 1U;
            a->ch1.env_period = val & 7U;
            if ((val & 0xF8U) == 0) a->ch1.enabled = false;
            break;
        case 0xFF13U:
            a->ch1.freq = (uint16_t)((a->ch1.freq & 0x0700U) | val);
            break;
        case 0xFF14U:
            a->ch1.freq = (uint16_t)((a->ch1.freq & 0x00FFU) | ((uint16_t)(val & 7U) << 8));
            a->ch1.length_en = (val & 0x40U) != 0;
            if (val & 0x80U) apu_trigger_pulse(&a->ch1, true);
            break;

        /* Ch2 */
        case 0xFF16U:
            a->ch2.duty = (val >> 6) & 3U;
            a->ch2.length_cnt = (uint8_t)(64U - (val & 0x3FU));
            break;
        case 0xFF17U:
            a->ch2.vol_init   = (val >> 4) & 0x0FU;
            a->ch2.env_dir    = (val >> 3) & 1U;
            a->ch2.env_period = val & 7U;
            if ((val & 0xF8U) == 0) a->ch2.enabled = false;
            break;
        case 0xFF18U:
            a->ch2.freq = (uint16_t)((a->ch2.freq & 0x0700U) | val);
            break;
        case 0xFF19U:
            a->ch2.freq = (uint16_t)((a->ch2.freq & 0x00FFU) | ((uint16_t)(val & 7U) << 8));
            a->ch2.length_en = (val & 0x40U) != 0;
            if (val & 0x80U) apu_trigger_pulse(&a->ch2, false);
            break;

        /* Ch3 */
        case 0xFF1AU:
            a->ch3.dac_on = (val & 0x80U) != 0;
            if (!a->ch3.dac_on) a->ch3.enabled = false;
            break;
        case 0xFF1BU:
            a->ch3.length_cnt = (uint16_t)(256U - val);
            break;
        case 0xFF1CU:
            a->ch3.vol_code = (val >> 5) & 3U;
            break;
        case 0xFF1DU:
            a->ch3.freq = (uint16_t)((a->ch3.freq & 0x0700U) | val);
            break;
        case 0xFF1EU:
            a->ch3.freq = (uint16_t)((a->ch3.freq & 0x00FFU) | ((uint16_t)(val & 7U) << 8));
            a->ch3.length_en = (val & 0x40U) != 0;
            if (val & 0x80U) {
                a->ch3.enabled = a->ch3.dac_on;
                if (a->ch3.length_cnt == 0) a->ch3.length_cnt = 256;
                a->ch3.pos = 0;
                a->ch3.timer = (uint32_t)(2048U - a->ch3.freq) * 2U;
            }
            break;

        /* Ch4 */
        case 0xFF20U:
            a->ch4.length_cnt = (uint8_t)(64U - (val & 0x3FU));
            break;
        case 0xFF21U:
            a->ch4.vol_init   = (val >> 4) & 0x0FU;
            a->ch4.env_dir    = (val >> 3) & 1U;
            a->ch4.env_period = val & 7U;
            if ((val & 0xF8U) == 0) a->ch4.enabled = false;
            break;
        case 0xFF22U:
            a->ch4.shift        = (val >> 4) & 0x0FU;
            a->ch4.width7       = (val & 0x08U) != 0;
            a->ch4.divisor_code = val & 7U;
            break;
        case 0xFF23U:
            a->ch4.length_en = (val & 0x40U) != 0;
            if (val & 0x80U) {
                a->ch4.enabled = (a->ch4.vol_init > 0 || a->ch4.env_dir != 0);
                if (a->ch4.length_cnt == 0) a->ch4.length_cnt = 64;
                a->ch4.vol = a->ch4.vol_init;
                a->ch4.env_timer = a->ch4.env_period ? a->ch4.env_period : 8;
                a->ch4.lfsr = 0x7FFFU;
                a->ch4.timer = g_noise_div[a->ch4.divisor_code] << a->ch4.shift;
            }
            break;

        case 0xFF24U: a->nr50 = val; break;
        case 0xFF25U: a->nr51 = val; break;
        default: break;
    }
}

static void apu_clock_envelope(uint8_t *vol, uint8_t dir, uint8_t period, uint8_t *timer) {
    if (period == 0) return;
    if (*timer > 0) (*timer)--;
    if (*timer == 0) {
        *timer = period;
        if (dir && *vol < 15) (*vol)++;
        else if (!dir && *vol > 0) (*vol)--;
    }
}

static void apu_clock_sequencer(GbApu *a) {
    /* 256 Hz length */
    if ((a->seq_step & 1U) == 0) {
        if (a->ch1.length_en && a->ch1.length_cnt > 0 && --a->ch1.length_cnt == 0) a->ch1.enabled = false;
        if (a->ch2.length_en && a->ch2.length_cnt > 0 && --a->ch2.length_cnt == 0) a->ch2.enabled = false;
        if (a->ch3.length_en && a->ch3.length_cnt > 0 && --a->ch3.length_cnt == 0) a->ch3.enabled = false;
        if (a->ch4.length_en && a->ch4.length_cnt > 0 && --a->ch4.length_cnt == 0) a->ch4.enabled = false;
    }
    /* 128 Hz sweep */
    if (a->seq_step == 2U || a->seq_step == 6U) {
        if (a->ch1.sweep_period > 0 && a->ch1.sweep_shift > 0) {
            if (a->ch1.sweep_timer > 0) a->ch1.sweep_timer--;
            if (a->ch1.sweep_timer == 0) {
                a->ch1.sweep_timer = a->ch1.sweep_period;
                uint16_t delta = (uint16_t)(a->ch1.sweep_shadow >> a->ch1.sweep_shift);
                uint16_t next = a->ch1.sweep_neg ? (uint16_t)(a->ch1.sweep_shadow - delta)
                                                 : (uint16_t)(a->ch1.sweep_shadow + delta);
                if (next > 2047U) {
                    a->ch1.enabled = false;
                } else {
                    a->ch1.sweep_shadow = next;
                    a->ch1.freq = next;
                }
            }
        }
    }
    /* 64 Hz envelope */
    if (a->seq_step == 7U) {
        apu_clock_envelope(&a->ch1.vol, a->ch1.env_dir, a->ch1.env_period, &a->ch1.env_timer);
        apu_clock_envelope(&a->ch2.vol, a->ch2.env_dir, a->ch2.env_period, &a->ch2.env_timer);
        apu_clock_envelope(&a->ch4.vol, a->ch4.env_dir, a->ch4.env_period, &a->ch4.env_timer);
    }
    a->seq_step = (uint8_t)((a->seq_step + 1U) & 7U);
}

static void apu_emit_sample(GbApu *a) {
    int32_t s1 = 0, s2 = 0, s3 = 0, s4 = 0;
    if (a->power) {
        if (a->ch1.enabled) s1 = g_duty_table[a->ch1.duty][a->ch1.duty_pos] ? (int32_t)a->ch1.vol : -(int32_t)a->ch1.vol;
        if (a->ch2.enabled) s2 = g_duty_table[a->ch2.duty][a->ch2.duty_pos] ? (int32_t)a->ch2.vol : -(int32_t)a->ch2.vol;
        if (a->ch3.enabled && a->ch3.vol_code > 0) {
            uint8_t b = a->ch3.ram[a->ch3.pos >> 1];
            uint8_t nib = (a->ch3.pos & 1U) ? (b & 0x0FU) : (b >> 4);
            uint8_t shift = (uint8_t)(a->ch3.vol_code - 1U);
            s3 = ((int32_t)(nib >> shift) - 8) * 2;
        }
        if (a->ch4.enabled) s4 = ((a->ch4.lfsr & 1U) == 0) ? (int32_t)a->ch4.vol : -(int32_t)a->ch4.vol;
    }

    int32_t left = 0, right = 0;
    if (a->nr51 & 0x10U) left  += s1;
    if (a->nr51 & 0x20U) left  += s2;
    if (a->nr51 & 0x40U) left  += s3;
    if (a->nr51 & 0x80U) left  += s4;
    if (a->nr51 & 0x01U) right += s1;
    if (a->nr51 & 0x02U) right += s2;
    if (a->nr51 & 0x04U) right += s3;
    if (a->nr51 & 0x08U) right += s4;

    int32_t vol_l = ((a->nr50 >> 4) & 7U) + 1;
    int32_t vol_r = (a->nr50 & 7U) + 1;
    left  = (left  * vol_l * 32);
    right = (right * vol_r * 32);
    if (left  >  32767) left  =  32767;
    if (left  < -32768) left  = -32768;
    if (right >  32767) right =  32767;
    if (right < -32768) right = -32768;

    a->out_buf[a->out_pos * 2 + 0] = (int16_t)left;
    a->out_buf[a->out_pos * 2 + 1] = (int16_t)right;
    a->out_pos++;
    if (a->out_pos >= APP_AUDIO_GRAIN) {
        hal_audio_push_samples(a->out_buf, APP_AUDIO_GRAIN);
        a->out_pos = 0;
    }
}

void hle_apu_step(GbApu *a, uint32_t t_cycles) {
    if (a->power) {
        /* Advance channel oscillators */
        uint32_t p1 = (uint32_t)(2048U - a->ch1.freq) * 4U;
        if (p1 == 0) p1 = 4;
        if (a->ch1.timer <= t_cycles) {
            uint32_t steps = (t_cycles - a->ch1.timer) / p1 + 1U;
            a->ch1.duty_pos = (uint8_t)((a->ch1.duty_pos + steps) & 7U);
            a->ch1.timer = p1 - ((t_cycles - a->ch1.timer) % p1);
        } else {
            a->ch1.timer -= t_cycles;
        }

        uint32_t p2 = (uint32_t)(2048U - a->ch2.freq) * 4U;
        if (p2 == 0) p2 = 4;
        if (a->ch2.timer <= t_cycles) {
            uint32_t steps = (t_cycles - a->ch2.timer) / p2 + 1U;
            a->ch2.duty_pos = (uint8_t)((a->ch2.duty_pos + steps) & 7U);
            a->ch2.timer = p2 - ((t_cycles - a->ch2.timer) % p2);
        } else {
            a->ch2.timer -= t_cycles;
        }

        uint32_t p3 = (uint32_t)(2048U - a->ch3.freq) * 2U;
        if (p3 == 0) p3 = 2;
        if (a->ch3.timer <= t_cycles) {
            uint32_t steps = (t_cycles - a->ch3.timer) / p3 + 1U;
            a->ch3.pos = (uint8_t)((a->ch3.pos + steps) & 31U);
            a->ch3.timer = p3 - ((t_cycles - a->ch3.timer) % p3);
        } else {
            a->ch3.timer -= t_cycles;
        }

        uint32_t p4 = g_noise_div[a->ch4.divisor_code] << a->ch4.shift;
        if (p4 == 0) p4 = 8;
        if (a->ch4.timer <= t_cycles) {
            uint32_t steps = (t_cycles - a->ch4.timer) / p4 + 1U;
            if (steps > 8U) steps = 8U;
            for (uint32_t i = 0; i < steps; i++) {
                uint16_t xor_bit = (uint16_t)((a->ch4.lfsr & 1U) ^ ((a->ch4.lfsr >> 1) & 1U));
                a->ch4.lfsr = (uint16_t)((a->ch4.lfsr >> 1) | (xor_bit << 14));
                if (a->ch4.width7) {
                    a->ch4.lfsr = (uint16_t)((a->ch4.lfsr & ~(1U << 6)) | (xor_bit << 6));
                }
            }
            a->ch4.timer = p4 - ((t_cycles - a->ch4.timer) % p4);
        } else {
            a->ch4.timer -= t_cycles;
        }

        a->seq_cycles += t_cycles;
        while (a->seq_cycles >= 8192U) {
            a->seq_cycles -= 8192U;
            apu_clock_sequencer(a);
        }
    }

    a->sample_accum += t_cycles * APP_AUDIO_RATE;
    while (a->sample_accum >= DMG_CLOCK_HZ) {
        a->sample_accum -= DMG_CLOCK_HZ;
        apu_emit_sample(a);
    }
}
