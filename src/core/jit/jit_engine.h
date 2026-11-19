#ifndef PS4_GB_JIT_ENGINE_H
#define PS4_GB_JIT_ENGINE_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "../result.h"
#include "../gb_machine.h"
#include "../../hal/hal.h"

#define JIT_CACHE_SIZE_BYTES (4U * 1024U * 1024U) /* 4 MiB, multiple of 16 KiB */
#define JIT_ROMX_TABLE_SIZE  8192U                /* Power of 2 hash table for banked ROMX */

typedef uint32_t (*JitBlockFn)(GbMachine *m);

typedef struct JitRomxSlot {
    uint32_t   key; /* (rom_bank << 16) | pc; 0xFFFFFFFF == empty */
    JitBlockFn fn;
} JitRomxSlot;

typedef struct JitEngine {
    HalJitArena  arena;
    bool         available;
    bool         enabled;
    size_t       write_cursor;

    JitBlockFn  *rom0_blocks; /* 16384 entries for 0x0000-0x3FFF */
    JitRomxSlot *romx_blocks; /* JIT_ROMX_TABLE_SIZE entries for 0x4000-0x7FFF */

    uint32_t     blocks_compiled;
    uint32_t     cache_flushes;
} JitEngine;

AppResult jit_engine_init(JitEngine *jit, bool enable);
void      jit_engine_shutdown(JitEngine *jit);
void      jit_engine_flush(JitEngine *jit);
void      jit_engine_set_enabled(JitEngine *jit, bool enable);

/* Run one emulated DMG frame (70224 T-cycles) using JIT (or interpreter fallback). */
void      jit_engine_run_frame(JitEngine *jit, GbMachine *m);

/* Execute a single JIT block or interpreter step (used by differential lockstep tests). */
uint32_t  jit_engine_step(JitEngine *jit, GbMachine *m);

/* Internal compiler entry point (implemented in jit_compile.c) */
JitBlockFn jit_compile_block(JitEngine *jit, GbMachine *m, uint16_t start_pc);

#endif /* PS4_GB_JIT_ENGINE_H */
