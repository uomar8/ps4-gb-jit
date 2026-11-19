#ifndef PS4_GB_JIT_SM83_INTERP_H
#define PS4_GB_JIT_SM83_INTERP_H

#include <stdint.h>
#include "../gb_machine.h"

/*
 * Execute a single SM83 instruction (or handle pending interrupt / HALT).
 * Returns T-cycles consumed (always a multiple of 4).
 */
uint32_t sm83_interp_step(GbMachine *m);

/*
 * Check and service pending DMG interrupt if IME=1 and (IF & IE) != 0.
 * Returns 20 T-cycles if an interrupt was serviced, else 0.
 */
uint32_t sm83_check_interrupts(GbMachine *m);

#endif /* PS4_GB_JIT_SM83_INTERP_H */
