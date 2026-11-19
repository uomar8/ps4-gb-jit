#ifndef PS4_GB_JIT_APU_H
#define PS4_GB_JIT_APU_H

#include <stdint.h>
#include <stdbool.h>
#include "../gb_types.h"

typedef struct GbApuPulse {
    bool     enabled;
    uint8_t  duty;
    uint8_t  duty_pos;
    uint8_t  length_cnt;
    bool     length_en;
    uint8_t  vol;
    uint8_t  vol_init;
    uint8_t  env_dir;
    uint8_t  env_period;
    uint8_t  env_timer;
    uint16_t freq;
    uint32_t timer;
    /* Ch1 sweep */
    uint8_t  sweep_period;
    uint8_t  sweep_neg;
    uint8_t  sweep_shift;
    uint8_t  sweep_timer;
    uint16_t sweep_shadow;
} GbApuPulse;

typedef struct GbApuWave {
    bool     enabled;
    bool     dac_on;
    uint16_t length_cnt;
    bool     length_en;
    uint8_t  vol_code;
    uint16_t freq;
    uint32_t timer;
    uint8_t  pos;
    uint8_t  ram[16];
} GbApuWave;

typedef struct GbApuNoise {
    bool     enabled;
    uint8_t  length_cnt;
    bool     length_en;
    uint8_t  vol;
    uint8_t  vol_init;
    uint8_t  env_dir;
    uint8_t  env_period;
    uint8_t  env_timer;
    uint8_t  shift;
    bool     width7;
    uint8_t  divisor_code;
    uint32_t timer;
    uint16_t lfsr;
} GbApuNoise;

typedef struct GbApu {
    bool       power;
    uint8_t    nr50;
    uint8_t    nr51;
    uint8_t    regs[0x30]; /* Raw register shadow FF10-FF3F */
    GbApuPulse ch1;
    GbApuPulse ch2;
    GbApuWave  ch3;
    GbApuNoise ch4;

    uint32_t   seq_cycles;
    uint8_t    seq_step;
    uint32_t   sample_accum;

    int16_t    out_buf[APP_AUDIO_GRAIN * 2];
    uint32_t   out_pos;
} GbApu;

void    hle_apu_reset(GbApu *a);
uint8_t hle_apu_read8(const GbApu *a, uint16_t addr);
void    hle_apu_write8(GbApu *a, uint16_t addr, uint8_t val);
void    hle_apu_step(GbApu *a, uint32_t t_cycles);

#endif /* PS4_GB_JIT_APU_H */
