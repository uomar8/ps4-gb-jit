#ifndef PS4_GB_JIT_BUS_H
#define PS4_GB_JIT_BUS_H

#include <stdint.h>
#include "../gb_machine.h"

uint8_t  bus_read8(GbMachine *m, uint16_t addr);
void     bus_write8(GbMachine *m, uint16_t addr, uint8_t val);
uint16_t bus_read16(GbMachine *m, uint16_t addr);
void     bus_write16(GbMachine *m, uint16_t addr, uint16_t val);

#endif /* PS4_GB_JIT_BUS_H */
