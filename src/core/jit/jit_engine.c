#include "jit_engine.h"
#include "../cpu/sm83_interp.h"
#include "../../debug/dbg_log.h"

#include <stdlib.h>
#include <string.h>

AppResult jit_engine_init(JitEngine *jit, bool enable) {
    if (!jit) return APP_ERR_INVALID_ARG;
    memset(jit, 0, sizeof(*jit));

    jit->rom0_blocks = (JitBlockFn *)calloc(16384U, sizeof(JitBlockFn));
    jit->romx_blocks = (JitRomxSlot *)malloc(JIT_ROMX_TABLE_SIZE * sizeof(JitRomxSlot));
    if (!jit->rom0_blocks || !jit->romx_blocks) {
        jit_engine_shutdown(jit);
        return APP_ERR_NOMEM;
    }

    AppResult rc = hal_jit_alloc(&jit->arena, JIT_CACHE_SIZE_BYTES);
    if (rc != APP_OK) {
        LOG_WARN("JIT", "hal_jit_alloc failed rc=0x%08x (%s) — falling back to SM83 interpreter",
                 (uint32_t)rc, dbg_sce_strerror(rc));
        jit->available = false;
        jit->enabled = false;
        return APP_OK; /* Non-fatal: interpreter fallback is active */
    }

    jit->available = true;
    jit->enabled = enable;
    jit_engine_flush(jit);
    LOG_INFO("JIT", "engine initialized cache=%u KiB enabled=%d",
             JIT_CACHE_SIZE_BYTES / 1024U, jit->enabled ? 1 : 0);
    return APP_OK;
}

void jit_engine_shutdown(JitEngine *jit) {
    if (!jit) return;
    if (jit->available) {
        hal_jit_free(&jit->arena);
    }
    free(jit->rom0_blocks);
    free(jit->romx_blocks);
    memset(jit, 0, sizeof(*jit));
}

void jit_engine_flush(JitEngine *jit) {
    if (!jit) return;
    jit->write_cursor = 0;
    if (jit->rom0_blocks) {
        memset(jit->rom0_blocks, 0, 16384U * sizeof(JitBlockFn));
    }
    if (jit->romx_blocks) {
        for (uint32_t i = 0; i < JIT_ROMX_TABLE_SIZE; i++) {
            jit->romx_blocks[i].key = 0xFFFFFFFFU;
            jit->romx_blocks[i].fn = NULL;
        }
    }
    if (jit->available) {
        hal_jit_flush_icache(jit->arena.rx_base, jit->arena.size);
    }
}

void jit_engine_set_enabled(JitEngine *jit, bool enable) {
    if (!jit) return;
    jit->enabled = jit->available && enable;
}

static JitBlockFn jit_lookup_or_compile(JitEngine *jit, GbMachine *m, uint16_t pc) {
    /* Flush cache if approaching capacity */
    if (jit->write_cursor + HAL_PAGE_SIZE > jit->arena.size) {
        jit->cache_flushes++;
        jit_engine_flush(jit);
    }

    if (pc < 0x4000U && !(m->cart.mbc == GB_MBC_1 && m->cart.mbc1_mode == 1)) {
        JitBlockFn fn = jit->rom0_blocks[pc];
        if (!fn) {
            fn = jit_compile_block(jit, m, pc);
            jit->rom0_blocks[pc] = fn;
        }
        return fn;
    }

    /* Banked ROMX (0x4000-0x7FFF) or MBC1 mode-1 ROM0: key by (bank << 16) | pc */
    uint32_t bank = (pc < 0x4000U) ? ((uint32_t)m->cart.mbc1_bank_hi << 5) : (uint32_t)m->cart.rom_bank;
    if (m->cart.mbc == GB_MBC_1 && pc >= 0x4000U) {
        bank = ((uint32_t)m->cart.mbc1_bank_hi << 5) | (bank & 0x1FU);
    }
    uint32_t key = (bank << 16) | (uint32_t)pc;
    uint32_t idx = (key * 2654435761U) & (JIT_ROMX_TABLE_SIZE - 1U);

    for (uint32_t probe = 0; probe < 16U; probe++) {
        uint32_t slot = (idx + probe) & (JIT_ROMX_TABLE_SIZE - 1U);
        if (jit->romx_blocks[slot].key == key) {
            return jit->romx_blocks[slot].fn;
        }
        if (jit->romx_blocks[slot].key == 0xFFFFFFFFU) {
            JitBlockFn fn = jit_compile_block(jit, m, pc);
            jit->romx_blocks[slot].key = key;
            jit->romx_blocks[slot].fn = fn;
            return fn;
        }
    }

    /* Hash bucket saturated: evict primary slot */
    JitBlockFn fn = jit_compile_block(jit, m, pc);
    jit->romx_blocks[idx].key = key;
    jit->romx_blocks[idx].fn = fn;
    return fn;
}

uint32_t jit_engine_step(JitEngine *jit, GbMachine *m) {
    /* Check interrupts first */
    uint32_t irq_cyc = sm83_check_interrupts(m);
    if (irq_cyc > 0) {
        return irq_cyc;
    }

    /* HLE HALT fast-forward: if halted with no pending IRQ, advance cleanly */
    if (m->cpu.halted) {
        return 4U;
    }

    /*
     * Use interpreter when:
     * - JIT disabled/unavailable
     * - EI 1-instruction delay or HALT bug active
     * - PC >= 0x8000 (WRAM/HRAM self-modifying code safe path)
     */
    if (!jit || !jit->enabled || !jit->available ||
        m->cpu.ime_delay || m->cpu.halt_bug || m->cpu.pc >= 0x8000U) {
        return sm83_interp_step(m);
    }

    JitBlockFn fn = jit_lookup_or_compile(jit, m, m->cpu.pc);
    if (!fn) {
        return sm83_interp_step(m);
    }
    return fn(m);
}

void jit_engine_run_frame(JitEngine *jit, GbMachine *m) {
    if (!m || !m->rom_loaded) {
        return;
    }
    m->ppu.frame_ready = false;
    uint32_t frame_cycles = 0;
    const uint32_t target = 70224U;

    while (frame_cycles < target) {
        uint32_t cyc = jit_engine_step(jit, m);
        gb_machine_step_subsystems(m, cyc);
        frame_cycles += cyc;
    }
}
