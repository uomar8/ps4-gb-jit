#ifndef PS4_GB_JIT_X64_EMIT_H
#define PS4_GB_JIT_X64_EMIT_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

/* Baseline x86_64 register encoding (zero AVX2/FMA3) */
typedef enum X64Reg {
    X64_RAX = 0,
    X64_RCX = 1,
    X64_RDX = 2,
    X64_RBX = 3,
    X64_RSP = 4,
    X64_RBP = 5,
    X64_RSI = 6,
    X64_RDI = 7
} X64Reg;

typedef struct JitEmitBuf {
    uint8_t *buf;
    size_t   pos;
    size_t   cap;
    bool     oom;
} JitEmitBuf;

void jit_x64_u8(JitEmitBuf *b, uint8_t v);
void jit_x64_u16(JitEmitBuf *b, uint16_t v);
void jit_x64_u32(JitEmitBuf *b, uint32_t v);
void jit_x64_u64(JitEmitBuf *b, uint64_t v);

void jit_x64_prologue(JitEmitBuf *b);
void jit_x64_epilogue_ret_imm32(JitEmitBuf *b, uint32_t cycles);
void jit_x64_epilogue_ret_eax_add(JitEmitBuf *b, uint32_t extra_cycles);

/* [RBX + disp32] memory ops */
void jit_x64_mov_m8_imm8(JitEmitBuf *b, uint32_t disp, uint8_t imm);
void jit_x64_mov_m16_imm16(JitEmitBuf *b, uint32_t disp, uint16_t imm);
void jit_x64_mov_al_m8(JitEmitBuf *b, uint32_t disp);
void jit_x64_mov_cl_m8(JitEmitBuf *b, uint32_t disp);
void jit_x64_mov_m8_al(JitEmitBuf *b, uint32_t disp);
void jit_x64_mov_ax_m16(JitEmitBuf *b, uint32_t disp);
void jit_x64_mov_m16_ax(JitEmitBuf *b, uint32_t disp);
void jit_x64_add_m16_imm8(JitEmitBuf *b, uint32_t disp, int8_t imm);

/* Native 8-bit ALU on AL / CL */
void jit_x64_and_al_cl(JitEmitBuf *b);
void jit_x64_xor_al_cl(JitEmitBuf *b);
void jit_x64_or_al_cl(JitEmitBuf *b);
void jit_x64_and_al_imm8(JitEmitBuf *b, uint8_t imm);
void jit_x64_xor_al_imm8(JitEmitBuf *b, uint8_t imm);
void jit_x64_or_al_imm8(JitEmitBuf *b, uint8_t imm);

/* Test [RBX + disp32] with imm8 and conditional short jump (returns patch offset) */
void   jit_x64_test_m8_imm8(JitEmitBuf *b, uint32_t disp, uint8_t mask);
size_t jit_x64_jz_rel8_placeholder(JitEmitBuf *b);
size_t jit_x64_jnz_rel8_placeholder(JitEmitBuf *b);
void   jit_x64_patch_rel8(JitEmitBuf *b, size_t patch_off);

/* Position-independent C helper calls (SysV & Win64 ABI safe) */
void jit_x64_call_c1_rbx(JitEmitBuf *b, uintptr_t fn_addr);
void jit_x64_call_c2_rbx_u16(JitEmitBuf *b, uintptr_t fn_addr, uint16_t arg1);
void jit_x64_call_c3_rbx_u16_al(JitEmitBuf *b, uintptr_t fn_addr, uint16_t arg1);

#endif /* PS4_GB_JIT_X64_EMIT_H */
