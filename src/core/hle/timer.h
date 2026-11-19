#ifndef PS4_GB_JIT_TIMER_H
#define PS4_GB_JIT_TIMER_H

#include <stdint.h>
#include <stdbool.h>

typedef struct GbTimer {
    uint16_t div_counter; /* Upper 8 bits == DIV (0xFF04) */
    uint8_t  tima;        /* 0xFF05 */
    uint8_t  tma;         /* 0xFF06 */
    uint8_t  tac;         /* 0xFF07 */
} GbTimer;

void    hle_timer_reset(GbTimer *t);
uint8_t hle_timer_read8(const GbTimer *t, uint16_t addr);
void    hle_timer_write8(GbTimer *t, uint16_t addr, uint8_t val, uint8_t *if_reg);
void    hle_timer_step(GbTimer *t, uint32_t t_cycles, uint8_t *if_reg);

#endif /* PS4_GB_JIT_TIMER_H */
