#include "jit_engine.h"
#include "jit_x64_emit.h"
#include "../cpu/sm83_interp.h"
#include "../mem/bus.h"

#include <stddef.h>
#include <string.h>

/* C helper wrappers called from JIT-compiled x86_64 blocks */
static uint32_t jit_helper_interp_one(GbMachine *m) {
    return sm83_interp_step(m);
}

static uint32_t jit_helper_read8(GbMachine *m, uint16_t addr) {
    return (uint32_t)bus_read8(m, addr);
}

static void jit_helper_write8(GbMachine *m, uint16_t addr, uint8_t val) {
    bus_write8(m, addr, val);
}

static uint32_t reg8_offset(uint8_t idx) {
    switch (idx & 7U) {
        case 0: return (uint32_t)offsetof(GbMachine, cpu.b);
        case 1: return (uint32_t)offsetof(GbMachine, cpu.c);
        case 2: return (uint32_t)offsetof(GbMachine, cpu.d);
        case 3: return (uint32_t)offsetof(GbMachine, cpu.e);
        case 4: return (uint32_t)offsetof(GbMachine, cpu.h);
        case 5: return (uint32_t)offsetof(GbMachine, cpu.l);
        case 7: return (uint32_t)offsetof(GbMachine, cpu.a);
        default: return 0xFFFFFFFFU;
    }
}

/* Emit native x86_64 Z-flag update after AL logical op: f = (AL == 0 ? 0x80 : 0) | extra_flags */
static void emit_set_z_flag_from_al(JitEmitBuf *b, uint8_t extra_flags) {
    /* test al, al (84 C0) */
    jit_x64_u8(b, 0x84); jit_x64_u8(b, 0xC0);
    /* mov byte [rbx + off(f)], extra_flags */
    jit_x64_mov_m8_imm8(b, (uint32_t)offsetof(GbMachine, cpu.f), extra_flags);
    /* jnz +7 (skip setting Z) */
    size_t jnz_off = jit_x64_jnz_rel8_placeholder(b);
    jit_x64_mov_m8_imm8(b, (uint32_t)offsetof(GbMachine, cpu.f), (uint8_t)(extra_flags | SM83_FLAG_Z));
    jit_x64_patch_rel8(b, jnz_off);
}

JitBlockFn jit_compile_block(JitEngine *jit, GbMachine *m, uint16_t start_pc) {
    if (!jit || !jit->available || !m || start_pc >= 0x8000U) {
        return NULL;
    }

    size_t start_off = (jit->write_cursor + 15U) & ~15U;
    if (start_off + 2048U > jit->arena.size) {
        return NULL;
    }

    JitEmitBuf eb;
    eb.buf = (uint8_t *)jit->arena.rw_base + start_off;
    eb.pos = 0;
    eb.cap = jit->arena.size - start_off;
    eb.oom = false;

    jit_x64_prologue(&eb);

    uint16_t pc = start_pc;
    uint32_t cycles = 0;
    bool terminated = false;
    const uint32_t max_insns = 32U;

    for (uint32_t n = 0; n < max_insns && !terminated && !eb.oom; n++) {
        /* Never cross 0x4000 or 0x8000 bank boundaries mid-block */
        if ((start_pc < 0x4000U && pc >= 0x4000U) || pc >= 0x8000U) {
            break;
        }

        uint8_t op = cart_read8(&m->cart, pc);

        /* 0x00: NOP */
        if (op == 0x00U) {
            pc++;
            cycles += 4U;
            continue;
        }

        /* LD r8, d8 (excluding (HL) 0x36) */
        if ((op & 0xC7U) == 0x06U && op != 0x36U) {
            uint8_t dst = (op >> 3) & 7U;
            uint8_t imm = cart_read8(&m->cart, (uint16_t)(pc + 1U));
            jit_x64_mov_m8_imm8(&eb, reg8_offset(dst), imm);
            pc = (uint16_t)(pc + 2U);
            cycles += 8U;
            continue;
        }

        /* LD r8, r8 (0x40..0x7F excluding (HL) and HALT 0x76) */
        if (op >= 0x40U && op <= 0x7FU && op != 0x76U) {
            uint8_t dst = (op >> 3) & 7U;
            uint8_t src = op & 7U;
            if (dst != 6U && src != 6U) {
                if (dst != src) {
                    jit_x64_mov_al_m8(&eb, reg8_offset(src));
                    jit_x64_mov_m8_al(&eb, reg8_offset(dst));
                }
                pc++;
                cycles += 4U;
                continue;
            }
        }

        /* LD rr, d16 (BC, DE, HL, SP) */
        if (op == 0x01U || op == 0x11U || op == 0x21U || op == 0x31U) {
            uint8_t lo = cart_read8(&m->cart, (uint16_t)(pc + 1U));
            uint8_t hi = cart_read8(&m->cart, (uint16_t)(pc + 2U));
            uint16_t imm16 = (uint16_t)(lo | ((uint16_t)hi << 8));
            if (op == 0x01U) {
                jit_x64_mov_m8_imm8(&eb, (uint32_t)offsetof(GbMachine, cpu.b), hi);
                jit_x64_mov_m8_imm8(&eb, (uint32_t)offsetof(GbMachine, cpu.c), lo);
            } else if (op == 0x11U) {
                jit_x64_mov_m8_imm8(&eb, (uint32_t)offsetof(GbMachine, cpu.d), hi);
                jit_x64_mov_m8_imm8(&eb, (uint32_t)offsetof(GbMachine, cpu.e), lo);
            } else if (op == 0x21U) {
                jit_x64_mov_m8_imm8(&eb, (uint32_t)offsetof(GbMachine, cpu.h), hi);
                jit_x64_mov_m8_imm8(&eb, (uint32_t)offsetof(GbMachine, cpu.l), lo);
            } else {
                jit_x64_mov_m16_imm16(&eb, (uint32_t)offsetof(GbMachine, cpu.sp), imm16);
            }
            pc = (uint16_t)(pc + 3U);
            cycles += 12U;
            continue;
        }

        /* INC/DEC SP */
        if (op == 0x33U || op == 0x3BU) {
            jit_x64_add_m16_imm8(&eb, (uint32_t)offsetof(GbMachine, cpu.sp), (op == 0x33U) ? 1 : -1);
            pc++;
            cycles += 8U;
            continue;
        }

        /* Native AND / XOR / OR A, r8 (excluding (HL)) */
        if ((op >= 0xA0U && op <= 0xB7U) && (op & 7U) != 6U) {
            uint8_t src = op & 7U;
            jit_x64_mov_al_m8(&eb, (uint32_t)offsetof(GbMachine, cpu.a));
            jit_x64_mov_cl_m8(&eb, reg8_offset(src));
            uint8_t grp = (op >> 3) & 7U;
            if (grp == 4U) {
                jit_x64_and_al_cl(&eb);
                jit_x64_mov_m8_al(&eb, (uint32_t)offsetof(GbMachine, cpu.a));
                emit_set_z_flag_from_al(&eb, SM83_FLAG_H);
            } else if (grp == 5U) {
                jit_x64_xor_al_cl(&eb);
                jit_x64_mov_m8_al(&eb, (uint32_t)offsetof(GbMachine, cpu.a));
                emit_set_z_flag_from_al(&eb, 0U);
            } else {
                jit_x64_or_al_cl(&eb);
                jit_x64_mov_m8_al(&eb, (uint32_t)offsetof(GbMachine, cpu.a));
                emit_set_z_flag_from_al(&eb, 0U);
            }
            pc++;
            cycles += 4U;
            continue;
        }

        /* Native AND / XOR / OR A, d8 */
        if (op == 0xE6U || op == 0xEEU || op == 0xF6U) {
            uint8_t imm = cart_read8(&m->cart, (uint16_t)(pc + 1U));
            jit_x64_mov_al_m8(&eb, (uint32_t)offsetof(GbMachine, cpu.a));
            if (op == 0xE6U) {
                jit_x64_and_al_imm8(&eb, imm);
                jit_x64_mov_m8_al(&eb, (uint32_t)offsetof(GbMachine, cpu.a));
                emit_set_z_flag_from_al(&eb, SM83_FLAG_H);
            } else if (op == 0xEEU) {
                jit_x64_xor_al_imm8(&eb, imm);
                jit_x64_mov_m8_al(&eb, (uint32_t)offsetof(GbMachine, cpu.a));
                emit_set_z_flag_from_al(&eb, 0U);
            } else {
                jit_x64_or_al_imm8(&eb, imm);
                jit_x64_mov_m8_al(&eb, (uint32_t)offsetof(GbMachine, cpu.a));
                emit_set_z_flag_from_al(&eb, 0U);
            }
            pc = (uint16_t)(pc + 2U);
            cycles += 8U;
            continue;
        }

        /* LDH (a8), A (0xE0) and LDH A, (a8) (0xF0) — terminate block after MMIO! */
        if (op == 0xE0U || op == 0xF0U) {
            uint16_t io_addr = (uint16_t)(0xFF00U + cart_read8(&m->cart, (uint16_t)(pc + 1U)));
            if (op == 0xE0U) {
                jit_x64_mov_al_m8(&eb, (uint32_t)offsetof(GbMachine, cpu.a));
                jit_x64_call_c3_rbx_u16_al(&eb, (uintptr_t)jit_helper_write8, io_addr);
            } else {
                jit_x64_call_c2_rbx_u16(&eb, (uintptr_t)jit_helper_read8, io_addr);
                jit_x64_mov_m8_al(&eb, (uint32_t)offsetof(GbMachine, cpu.a));
            }
            pc = (uint16_t)(pc + 2U);
            cycles += 12U;
            jit_x64_mov_m16_imm16(&eb, (uint32_t)offsetof(GbMachine, cpu.pc), pc);
            jit_x64_epilogue_ret_imm32(&eb, cycles);
            terminated = true;
            continue;
        }

        /* JP a16 (0xC3) */
        if (op == 0xC3U) {
            uint16_t lo = cart_read8(&m->cart, (uint16_t)(pc + 1U));
            uint16_t hi = cart_read8(&m->cart, (uint16_t)(pc + 2U));
            uint16_t target = (uint16_t)(lo | (hi << 8));
            cycles += 16U;
            jit_x64_mov_m16_imm16(&eb, (uint32_t)offsetof(GbMachine, cpu.pc), target);
            jit_x64_epilogue_ret_imm32(&eb, cycles);
            terminated = true;
            continue;
        }

        /* JR r8 (0x18) */
        if (op == 0x18U) {
            int8_t rel = (int8_t)cart_read8(&m->cart, (uint16_t)(pc + 1U));
            uint16_t target = (uint16_t)(pc + 2U + rel);
            cycles += 12U;
            jit_x64_mov_m16_imm16(&eb, (uint32_t)offsetof(GbMachine, cpu.pc), target);
            jit_x64_epilogue_ret_imm32(&eb, cycles);
            terminated = true;
            continue;
        }

        /* Conditional JR cc, r8 (0x20 NZ, 0x28 Z, 0x30 NC, 0x38 C) */
        if (op == 0x20U || op == 0x28U || op == 0x30U || op == 0x38U) {
            int8_t rel = (int8_t)cart_read8(&m->cart, (uint16_t)(pc + 1U));
            uint16_t fallthrough = (uint16_t)(pc + 2U);
            uint16_t target = (uint16_t)(fallthrough + rel);
            uint8_t mask = (op == 0x20U || op == 0x28U) ? SM83_FLAG_Z : SM83_FLAG_C;
            bool branch_if_set = (op == 0x28U || op == 0x38U);

            jit_x64_test_m8_imm8(&eb, (uint32_t)offsetof(GbMachine, cpu.f), mask);
            size_t not_taken_jmp = branch_if_set ? jit_x64_jz_rel8_placeholder(&eb)
                                                 : jit_x64_jnz_rel8_placeholder(&eb);
            /* Taken path */
            jit_x64_mov_m16_imm16(&eb, (uint32_t)offsetof(GbMachine, cpu.pc), target);
            jit_x64_epilogue_ret_imm32(&eb, cycles + 12U);
            /* Not-taken path */
            jit_x64_patch_rel8(&eb, not_taken_jmp);
            jit_x64_mov_m16_imm16(&eb, (uint32_t)offsetof(GbMachine, cpu.pc), fallthrough);
            jit_x64_epilogue_ret_imm32(&eb, cycles + 8U);
            terminated = true;
            continue;
        }

        /* DI (0xF3) */
        if (op == 0xF3U) {
            jit_x64_mov_m8_imm8(&eb, (uint32_t)offsetof(GbMachine, cpu.ime), 0);
            jit_x64_mov_m8_imm8(&eb, (uint32_t)offsetof(GbMachine, cpu.ime_delay), 0);
            pc++;
            cycles += 4U;
            continue;
        }

        /*
         * Fallback for complex/memory/branch opcodes (DAA, CB, CALL, RET, ADD/SUB, (HL), etc.):
         * Flush PC, call reference SM83 interpreter for this instruction, add its T-cycles, exit block.
         */
        jit_x64_mov_m16_imm16(&eb, (uint32_t)offsetof(GbMachine, cpu.pc), pc);
        jit_x64_call_c1_rbx(&eb, (uintptr_t)jit_helper_interp_one);
        jit_x64_epilogue_ret_eax_add(&eb, cycles);
        terminated = true;
    }

    if (!terminated && !eb.oom) {
        jit_x64_mov_m16_imm16(&eb, (uint32_t)offsetof(GbMachine, cpu.pc), pc);
        jit_x64_epilogue_ret_imm32(&eb, cycles);
    }

    if (eb.oom || eb.pos == 0) {
        return NULL;
    }

    jit->write_cursor = start_off + eb.pos;
    jit->blocks_compiled++;

    void *rx_entry = (uint8_t *)jit->arena.rx_base + start_off;
    hal_jit_flush_icache(rx_entry, eb.pos);

    JitBlockFn fn = NULL;
    memcpy(&fn, &rx_entry, sizeof(fn));
    return fn;
}
