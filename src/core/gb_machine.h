#ifndef PS4_GB_JIT_GB_MACHINE_H
#define PS4_GB_JIT_GB_MACHINE_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "result.h"
#include "gb_types.h"
#include "mem/cart.h"
#include "hle/ppu.h"
#include "hle/apu.h"
#include "hle/timer.h"

/* DMG Joypad button bitmask (active high inside emulator; inverted on JOYP read) */
typedef enum GbKeyMask {
    GB_KEY_RIGHT  = (1U << 0),
    GB_KEY_LEFT   = (1U << 1),
    GB_KEY_UP     = (1U << 2),
    GB_KEY_DOWN   = (1U << 3),
    GB_KEY_A      = (1U << 4),
    GB_KEY_B      = (1U << 5),
    GB_KEY_SELECT = (1U << 6),
    GB_KEY_START  = (1U << 7)
} GbKeyMask;

/* SM83 Flag bits in F register */
#define SM83_FLAG_Z 0x80U
#define SM83_FLAG_N 0x40U
#define SM83_FLAG_H 0x20U
#define SM83_FLAG_C 0x10U

typedef struct GbCpu {
    uint8_t  a;
    uint8_t  f;
    uint8_t  b;
    uint8_t  c;
    uint8_t  d;
    uint8_t  e;
    uint8_t  h;
    uint8_t  l;
    uint16_t sp;
    uint16_t pc;
    uint8_t  ime;       /* Interrupt Master Enable (0 or 1) */
    uint8_t  ime_delay; /* EI takes effect after the next instruction */
    uint8_t  halted;
    uint8_t  stopped;
    uint8_t  halt_bug;
    uint8_t  pad0;
} GbCpu;

typedef struct GbMachine {
    /* Kept at top of struct for compact x86_64 JIT [RBX + off] addressing */
    GbCpu    cpu;
    uint8_t  wram[8192];
    uint8_t  hram[127];
    uint8_t  if_reg;    /* 0xFF0F */
    uint8_t  ie_reg;    /* 0xFFFF */
    uint8_t  joyp_sel;  /* 0xFF00 bits 4-5 */
    uint8_t  joyp_keys; /* Active-high GbKeyMask */
    uint8_t  sb;        /* 0xFF01 Serial transfer data */
    uint8_t  sc;        /* 0xFF02 Serial transfer control */

    /* Subsystems */
    GbCart   cart;
    GbPpu    ppu;
    GbApu    apu;
    GbTimer  timer;

    /* Serial capture buffer (used by Blargg cpu_instrs test ROMs) */
    char     serial_buf[256];
    size_t   serial_len;

    /* Execution stats */
    uint64_t total_cycles;
    bool     rom_loaded;
} GbMachine;

void      gb_machine_init(GbMachine *m);
void      gb_machine_destroy(GbMachine *m);
void      gb_machine_reset(GbMachine *m);
AppResult gb_machine_load_rom_buffer(GbMachine *m, const uint8_t *data, size_t size);
AppResult gb_machine_load_rom_file(GbMachine *m, const char *abs_path);
void      gb_machine_set_keys(GbMachine *m, uint8_t keys);
void      gb_machine_step_subsystems(GbMachine *m, uint32_t t_cycles);
AppResult gb_machine_save_state(const GbMachine *m, const char *abs_path);
AppResult gb_machine_load_state(GbMachine *m, const char *abs_path);

#endif /* PS4_GB_JIT_GB_MACHINE_H */
