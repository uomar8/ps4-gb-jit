#include "jit_x64_emit.h"

void jit_x64_u8(JitEmitBuf *b, uint8_t v) {
    if (b->pos + 1 > b->cap) { b->oom = true; return; }
    b->buf[b->pos++] = v;
}

void jit_x64_u16(JitEmitBuf *b, uint16_t v) {
    jit_x64_u8(b, (uint8_t)(v & 0xFFU));
    jit_x64_u8(b, (uint8_t)(v >> 8));
}

void jit_x64_u32(JitEmitBuf *b, uint32_t v) {
    jit_x64_u8(b, (uint8_t)(v & 0xFFU));
    jit_x64_u8(b, (uint8_t)((v >> 8) & 0xFFU));
    jit_x64_u8(b, (uint8_t)((v >> 16) & 0xFFU));
    jit_x64_u8(b, (uint8_t)((v >> 24) & 0xFFU));
}

void jit_x64_u64(JitEmitBuf *b, uint64_t v) {
    jit_x64_u32(b, (uint32_t)(v & 0xFFFFFFFFULL));
    jit_x64_u32(b, (uint32_t)(v >> 32));
}

void jit_x64_prologue(JitEmitBuf *b) {
    /* push rbx (aligns RSP to 16B after CALL pushed 8B return address) */
    jit_x64_u8(b, 0x53);
#if defined(_WIN32)
    /* mov rbx, rcx (48 89 CB) */
    jit_x64_u8(b, 0x48); jit_x64_u8(b, 0x89); jit_x64_u8(b, 0xCB);
    /* sub rsp, 32 (48 83 EC 20) — Win64 shadow space, keeps 16B alignment */
    jit_x64_u8(b, 0x48); jit_x64_u8(b, 0x83); jit_x64_u8(b, 0xEC); jit_x64_u8(b, 0x20);
#else
    /* mov rbx, rdi (48 89 FB) — SysV AMD64 (Linux x64 & PS4 Orbis) */
    jit_x64_u8(b, 0x48); jit_x64_u8(b, 0x89); jit_x64_u8(b, 0xFB);
#endif
}

void jit_x64_epilogue_ret_imm32(JitEmitBuf *b, uint32_t cycles) {
    /* mov eax, imm32 (B8 imm32) */
    jit_x64_u8(b, 0xB8);
    jit_x64_u32(b, cycles);
#if defined(_WIN32)
    /* add rsp, 32 (48 83 C4 20) */
    jit_x64_u8(b, 0x48); jit_x64_u8(b, 0x83); jit_x64_u8(b, 0xC4); jit_x64_u8(b, 0x20);
#endif
    /* pop rbx; ret */
    jit_x64_u8(b, 0x5B);
    jit_x64_u8(b, 0xC3);
}

void jit_x64_epilogue_ret_eax_add(JitEmitBuf *b, uint32_t extra_cycles) {
    if (extra_cycles != 0) {
        /* add eax, imm32 (05 imm32) */
        jit_x64_u8(b, 0x05);
        jit_x64_u32(b, extra_cycles);
    }
#if defined(_WIN32)
    jit_x64_u8(b, 0x48); jit_x64_u8(b, 0x83); jit_x64_u8(b, 0xC4); jit_x64_u8(b, 0x20);
#endif
    jit_x64_u8(b, 0x5B);
    jit_x64_u8(b, 0xC3);
}

void jit_x64_mov_m8_imm8(JitEmitBuf *b, uint32_t disp, uint8_t imm) {
    /* mov byte [rbx + disp32], imm8 (C6 83 disp32 imm8) */
    jit_x64_u8(b, 0xC6);
    jit_x64_u8(b, 0x83);
    jit_x64_u32(b, disp);
    jit_x64_u8(b, imm);
}

void jit_x64_mov_m16_imm16(JitEmitBuf *b, uint32_t disp, uint16_t imm) {
    /* mov word [rbx + disp32], imm16 (66 C7 83 disp32 imm16) */
    jit_x64_u8(b, 0x66);
    jit_x64_u8(b, 0xC7);
    jit_x64_u8(b, 0x83);
    jit_x64_u32(b, disp);
    jit_x64_u16(b, imm);
}

void jit_x64_mov_al_m8(JitEmitBuf *b, uint32_t disp) {
    /* mov al, byte [rbx + disp32] (8A 83 disp32) */
    jit_x64_u8(b, 0x8A);
    jit_x64_u8(b, 0x83);
    jit_x64_u32(b, disp);
}

void jit_x64_mov_cl_m8(JitEmitBuf *b, uint32_t disp) {
    /* mov cl, byte [rbx + disp32] (8A 8B disp32) */
    jit_x64_u8(b, 0x8A);
    jit_x64_u8(b, 0x8B);
    jit_x64_u32(b, disp);
}

void jit_x64_mov_m8_al(JitEmitBuf *b, uint32_t disp) {
    /* mov byte [rbx + disp32], al (88 83 disp32) */
    jit_x64_u8(b, 0x88);
    jit_x64_u8(b, 0x83);
    jit_x64_u32(b, disp);
}

void jit_x64_mov_ax_m16(JitEmitBuf *b, uint32_t disp) {
    /* mov ax, word [rbx + disp32] (66 8B 83 disp32) */
    jit_x64_u8(b, 0x66);
    jit_x64_u8(b, 0x8B);
    jit_x64_u8(b, 0x83);
    jit_x64_u32(b, disp);
}

void jit_x64_mov_m16_ax(JitEmitBuf *b, uint32_t disp) {
    /* mov word [rbx + disp32], ax (66 89 83 disp32) */
    jit_x64_u8(b, 0x66);
    jit_x64_u8(b, 0x89);
    jit_x64_u8(b, 0x83);
    jit_x64_u32(b, disp);
}

void jit_x64_add_m16_imm8(JitEmitBuf *b, uint32_t disp, int8_t imm) {
    /* add word [rbx + disp32], imm8 (66 83 83 disp32 imm8) */
    jit_x64_u8(b, 0x66);
    jit_x64_u8(b, 0x83);
    jit_x64_u8(b, 0x83);
    jit_x64_u32(b, disp);
    jit_x64_u8(b, (uint8_t)imm);
}

void jit_x64_and_al_cl(JitEmitBuf *b) { jit_x64_u8(b, 0x20); jit_x64_u8(b, 0xC8); }
void jit_x64_xor_al_cl(JitEmitBuf *b) { jit_x64_u8(b, 0x30); jit_x64_u8(b, 0xC8); }
void jit_x64_or_al_cl(JitEmitBuf *b)  { jit_x64_u8(b, 0x08); jit_x64_u8(b, 0xC8); }

void jit_x64_and_al_imm8(JitEmitBuf *b, uint8_t imm) { jit_x64_u8(b, 0x24); jit_x64_u8(b, imm); }
void jit_x64_xor_al_imm8(JitEmitBuf *b, uint8_t imm) { jit_x64_u8(b, 0x34); jit_x64_u8(b, imm); }
void jit_x64_or_al_imm8(JitEmitBuf *b, uint8_t imm)  { jit_x64_u8(b, 0x0C); jit_x64_u8(b, imm); }

void jit_x64_test_m8_imm8(JitEmitBuf *b, uint32_t disp, uint8_t mask) {
    /* test byte [rbx + disp32], imm8 (F6 83 disp32 imm8) */
    jit_x64_u8(b, 0xF6);
    jit_x64_u8(b, 0x83);
    jit_x64_u32(b, disp);
    jit_x64_u8(b, mask);
}

size_t jit_x64_jz_rel8_placeholder(JitEmitBuf *b) {
    jit_x64_u8(b, 0x74);
    size_t off = b->pos;
    jit_x64_u8(b, 0x00);
    return off;
}

size_t jit_x64_jnz_rel8_placeholder(JitEmitBuf *b) {
    jit_x64_u8(b, 0x75);
    size_t off = b->pos;
    jit_x64_u8(b, 0x00);
    return off;
}

void jit_x64_patch_rel8(JitEmitBuf *b, size_t patch_off) {
    if (b->oom || patch_off >= b->pos) return;
    intptr_t rel = (intptr_t)b->pos - (intptr_t)(patch_off + 1);
    if (rel < -128 || rel > 127) {
        b->oom = true;
        return;
    }
    b->buf[patch_off] = (uint8_t)(int8_t)rel;
}

static void jit_x64_call_rax_imm64(JitEmitBuf *b, uintptr_t fn_addr) {
    /* mov rax, imm64 (48 B8 imm64); call rax (FF D0) */
    jit_x64_u8(b, 0x48);
    jit_x64_u8(b, 0xB8);
    jit_x64_u64(b, (uint64_t)fn_addr);
    jit_x64_u8(b, 0xFF);
    jit_x64_u8(b, 0xD0);
}

void jit_x64_call_c1_rbx(JitEmitBuf *b, uintptr_t fn_addr) {
#if defined(_WIN32)
    /* mov rcx, rbx (48 89 D9) */
    jit_x64_u8(b, 0x48); jit_x64_u8(b, 0x89); jit_x64_u8(b, 0xD9);
#else
    /* mov rdi, rbx (48 89 DF) */
    jit_x64_u8(b, 0x48); jit_x64_u8(b, 0x89); jit_x64_u8(b, 0xDF);
#endif
    jit_x64_call_rax_imm64(b, fn_addr);
}

void jit_x64_call_c2_rbx_u16(JitEmitBuf *b, uintptr_t fn_addr, uint16_t arg1) {
#if defined(_WIN32)
    /* mov rcx, rbx; mov edx, imm32 */
    jit_x64_u8(b, 0x48); jit_x64_u8(b, 0x89); jit_x64_u8(b, 0xD9);
    jit_x64_u8(b, 0xBA); jit_x64_u32(b, (uint32_t)arg1);
#else
    /* mov rdi, rbx; mov esi, imm32 */
    jit_x64_u8(b, 0x48); jit_x64_u8(b, 0x89); jit_x64_u8(b, 0xDF);
    jit_x64_u8(b, 0xBE); jit_x64_u32(b, (uint32_t)arg1);
#endif
    jit_x64_call_rax_imm64(b, fn_addr);
}

void jit_x64_call_c3_rbx_u16_al(JitEmitBuf *b, uintptr_t fn_addr, uint16_t arg1) {
#if defined(_WIN32)
    /* movzx r8d, al (44 0F B6 C0); mov rcx, rbx; mov edx, imm32 */
    jit_x64_u8(b, 0x44); jit_x64_u8(b, 0x0F); jit_x64_u8(b, 0xB6); jit_x64_u8(b, 0xC0);
    jit_x64_u8(b, 0x48); jit_x64_u8(b, 0x89); jit_x64_u8(b, 0xD9);
    jit_x64_u8(b, 0xBA); jit_x64_u32(b, (uint32_t)arg1);
#else
    /* movzx edx, al (0F B6 D0); mov rdi, rbx; mov esi, imm32 */
    jit_x64_u8(b, 0x0F); jit_x64_u8(b, 0xB6); jit_x64_u8(b, 0xD0);
    jit_x64_u8(b, 0x48); jit_x64_u8(b, 0x89); jit_x64_u8(b, 0xDF);
    jit_x64_u8(b, 0xBE); jit_x64_u32(b, (uint32_t)arg1);
#endif
    jit_x64_call_rax_imm64(b, fn_addr);
}
