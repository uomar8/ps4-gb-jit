#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/gb_machine.h"
#include "core/cpu/sm83_interp.h"
#include "core/jit/jit_engine.h"
#include "debug/dbg_log.h"
#include "debug/dbg_stub.h"
#include "hal/hal.h"
#include "hal/mock/hal_mock_ctl.h"

/* Helper to emit SM83 bytes into a ROM buffer */
typedef struct RomAsm {
    uint8_t *buf;
    size_t   pos;
} RomAsm;

static void emit(RomAsm *a, uint8_t b) { a->buf[a->pos++] = b; }
static void emit16(RomAsm *a, uint16_t v) {
    a->buf[a->pos++] = (uint8_t)(v & 0xFFU);
    a->buf[a->pos++] = (uint8_t)((v >> 8) & 0xFFU);
}

static void assert_machines_identical(const GbMachine *a, const GbMachine *b, const char *label) {
    if (a->cpu.a != b->cpu.a || a->cpu.f != b->cpu.f ||
        a->cpu.b != b->cpu.b || a->cpu.c != b->cpu.c ||
        a->cpu.d != b->cpu.d || a->cpu.e != b->cpu.e ||
        a->cpu.h != b->cpu.h || a->cpu.l != b->cpu.l ||
        a->cpu.sp != b->cpu.sp || a->cpu.pc != b->cpu.pc ||
        a->cpu.ime != b->cpu.ime || a->if_reg != b->if_reg ||
        a->ie_reg != b->ie_reg || a->total_cycles != b->total_cycles ||
        memcmp(a->wram, b->wram, sizeof(a->wram)) != 0 ||
        memcmp(a->hram, b->hram, sizeof(a->hram)) != 0 ||
        a->serial_len != b->serial_len ||
        memcmp(a->serial_buf, b->serial_buf, a->serial_len) != 0) {
        fprintf(stderr,
                "[FAIL] Lockstep mismatch (%s):\n"
                "  INTERP: AF=%02X%02X BC=%02X%02X DE=%02X%02X HL=%02X%02X SP=%04X PC=%04X CYC=%llu\n"
                "  JIT   : AF=%02X%02X BC=%02X%02X DE=%02X%02X HL=%02X%02X SP=%04X PC=%04X CYC=%llu\n",
                label,
                a->cpu.a, a->cpu.f, a->cpu.b, a->cpu.c, a->cpu.d, a->cpu.e, a->cpu.h, a->cpu.l,
                a->cpu.sp, a->cpu.pc, (unsigned long long)a->total_cycles,
                b->cpu.a, b->cpu.f, b->cpu.b, b->cpu.c, b->cpu.d, b->cpu.e, b->cpu.h, b->cpu.l,
                b->cpu.sp, b->cpu.pc, (unsigned long long)b->total_cycles);
        abort();
    }
}

/*
 * Build a 64 KiB MBC1 test ROM that exercises:
 *  - ALU, DAA, CB bitops, stack PUSH/POP, CALL/RET, conditional JR/JP
 *  - Bank-switched ROMX code execution across banks 1, 2, 3
 *  - Self-modifying code (SMC) in WRAM (0xC100)
 *  - Blargg-style serial output ("OK") via 0xFF01/0xFF02
 */
static uint8_t *build_comprehensive_test_rom(size_t *out_size) {
    size_t sz = 64 * 1024;
    uint8_t *rom = (uint8_t *)calloc(1, sz);
    assert(rom != NULL);
    memcpy(rom + 0x0134, "LOCKSTEP", 8);
    rom[0x0147] = 0x01; /* MBC1 */
    rom[0x0148] = 0x01; /* 64 KiB (4 banks) */
    rom[0x0149] = 0x00;

    /* Helper in Bank 1 @ 0x4000: A = A + 0x11; RET */
    {
        RomAsm b1 = { rom, 0x4000 };
        emit(&b1, 0xC6); emit(&b1, 0x11); /* ADD A, 0x11 */
        emit(&b1, 0xC9);                  /* RET */
    }
    /* Helper in Bank 2 @ 0x4000: A = A ^ 0x55; RET */
    {
        RomAsm b2 = { rom, 0x8000 };
        emit(&b2, 0xEE); emit(&b2, 0x55); /* XOR 0x55 */
        emit(&b2, 0xC9);                  /* RET */
    }
    /* Helper in Bank 3 @ 0x4000: RLCA; DAA; RET */
    {
        RomAsm b3 = { rom, 0xC000 };
        emit(&b3, 0x07); /* RLCA */
        emit(&b3, 0x27); /* DAA */
        emit(&b3, 0xC9); /* RET */
    }

    /* Main program at 0x0100 */
    RomAsm a = { rom, 0x0100 };
    emit(&a, 0x00);                  /* NOP */
    emit(&a, 0xC3); emit16(&a, 0x0150); /* JP 0x0150 */

    a.pos = 0x0150;
    /* 1. ALU + DAA + CB bitops loop (16 iterations) */
    emit(&a, 0x3E); emit(&a, 0x19);  /* LD A, 0x19 */
    emit(&a, 0xC6); emit(&a, 0x28);  /* ADD A, 0x28 -> 0x41 */
    emit(&a, 0x27);                  /* DAA -> 0x47 */
    emit(&a, 0x06); emit(&a, 0x10);  /* LD B, 16 */
    emit(&a, 0x21); emit16(&a, 0xC000); /* LD HL, 0xC000 */
    uint16_t loop_pc = (uint16_t)a.pos;
    emit(&a, 0xCB); emit(&a, 0x37);  /* SWAP A */
    emit(&a, 0xCB); emit(&a, 0xC7);  /* SET 0, A */
    emit(&a, 0x22);                  /* LD (HL+), A */
    emit(&a, 0x80);                  /* ADD A, B */
    emit(&a, 0x05);                  /* DEC B */
    emit(&a, 0x20); emit(&a, (uint8_t)((int8_t)(loop_pc - (a.pos + 1)))); /* JR NZ, loop */

    /* 2. Bank-switched calls to 0x4000 in Bank 1, 2, 3 */
    emit(&a, 0x3E); emit(&a, 0x01);  /* LD A, 1 */
    emit(&a, 0xEA); emit16(&a, 0x2000); /* LD (0x2000), A -> Bank 1 */
    emit(&a, 0xCD); emit16(&a, 0x4000); /* CALL 0x4000 */

    emit(&a, 0xF5);                  /* PUSH AF */
    emit(&a, 0x3E); emit(&a, 0x02);  /* LD A, 2 */
    emit(&a, 0xEA); emit16(&a, 0x2000); /* Bank 2 */
    emit(&a, 0xF1);                  /* POP AF */
    emit(&a, 0xCD); emit16(&a, 0x4000); /* CALL 0x4000 */

    emit(&a, 0xF5);
    emit(&a, 0x3E); emit(&a, 0x03);  /* LD A, 3 */
    emit(&a, 0xEA); emit16(&a, 0x2000); /* Bank 3 */
    emit(&a, 0xF1);
    emit(&a, 0xCD); emit16(&a, 0x4000); /* CALL 0x4000 */
    emit(&a, 0xEA); emit16(&a, 0xC040); /* Store result */

    /* 3. Self-Modifying Code (SMC) in WRAM at 0xC100 */
    /* Write: [3E 42 C9] (LD A, 0x42; RET), call it, patch 0x42 -> 0x99, call again */
    emit(&a, 0x21); emit16(&a, 0xC100);
    emit(&a, 0x36); emit(&a, 0x3E); emit(&a, 0x23); /* LD (HL), 0x3E; INC HL */
    emit(&a, 0x36); emit(&a, 0x42); emit(&a, 0x23); /* LD (HL), 0x42; INC HL */
    emit(&a, 0x36); emit(&a, 0xC9);                 /* LD (HL), 0xC9 */
    emit(&a, 0xCD); emit16(&a, 0xC100);             /* CALL 0xC100 -> A=0x42 */
    emit(&a, 0xEA); emit16(&a, 0xC041);
    emit(&a, 0x3E); emit(&a, 0x99);
    emit(&a, 0xEA); emit16(&a, 0xC101);             /* Patch immediate to 0x99 */
    emit(&a, 0xCD); emit16(&a, 0xC100);             /* CALL 0xC100 -> A=0x99 */
    emit(&a, 0xEA); emit16(&a, 0xC042);

    /* 4. Serial output "OK" via 0xFF01 / 0xFF02 */
    emit(&a, 0x3E); emit(&a, 'O');
    emit(&a, 0xE0); emit(&a, 0x01); /* LDH (0x01), A */
    emit(&a, 0x3E); emit(&a, 0x81);
    emit(&a, 0xE0); emit(&a, 0x02); /* LDH (0x02), A */
    emit(&a, 0x3E); emit(&a, 'K');
    emit(&a, 0xE0); emit(&a, 0x01);
    emit(&a, 0x3E); emit(&a, 0x81);
    emit(&a, 0xE0); emit(&a, 0x02);

    /* 5. Infinite idle loop (JR -2) */
    emit(&a, 0x18); emit(&a, 0xFE);

    *out_size = sz;
    return rom;
}

static void run_lockstep_test(bool inject_jit_fail) {
    size_t rom_size = 0;
    uint8_t *rom = build_comprehensive_test_rom(&rom_size);

    GbMachine *m_interp = (GbMachine *)calloc(1, sizeof(GbMachine));
    GbMachine *m_jit    = (GbMachine *)calloc(1, sizeof(GbMachine));
    assert(m_interp != NULL && m_jit != NULL);
    gb_machine_init(m_interp);
    gb_machine_init(m_jit);
    assert(gb_machine_load_rom_buffer(m_interp, rom, rom_size) == APP_OK);
    assert(gb_machine_load_rom_buffer(m_jit, rom, rom_size) == APP_OK);

    hal_mock_jit_set_fail_eacces(inject_jit_fail);
    JitEngine jit;
    AppResult jrc = jit_engine_init(&jit, true);
    assert(jrc == APP_OK);
    if (inject_jit_fail) {
        assert(!jit.available && !jit.enabled);
    } else {
        assert(jit.available && jit.enabled);
    }

    /* Run 4 full DMG frames in lockstep */
    for (int frame = 0; frame < 4; ++frame) {
        uint64_t target = m_jit->total_cycles + 70224ULL;
        while (m_jit->total_cycles < target) {
            uint32_t cyc = jit_engine_step(&jit, m_jit);
            m_jit->total_cycles += cyc;
            gb_machine_step_subsystems(m_jit, cyc);

            while (m_interp->total_cycles < m_jit->total_cycles) {
                uint32_t ic = sm83_interp_step(m_interp);
                m_interp->total_cycles += ic;
                gb_machine_step_subsystems(m_interp, ic);
            }
            assert_machines_identical(m_interp, m_jit, inject_jit_fail ? "fallback" : "jit");
        }
    }

    /* Verify SMC results and Serial output */
    assert(m_jit->wram[0x41] == 0x42);
    assert(m_jit->wram[0x42] == 0x99);
    assert(m_jit->serial_len == 2);
    assert(strcmp(m_jit->serial_buf, "OK") == 0);
    if (!inject_jit_fail) {
        assert(jit.blocks_compiled >= 5);
    }

    jit_engine_shutdown(&jit);
    hal_mock_jit_set_fail_eacces(false);
    gb_machine_destroy(m_interp);
    gb_machine_destroy(m_jit);
    free(m_interp);
    free(m_jit);
    free(rom);
}

int main(void) {
    assert(hal_sys_init() == APP_OK);
    hal_mock_fs_set_app_dir("/tmp/ps4_gb_jit_test_lockstep");
    assert(hal_fs_mkdir_p(hal_fs_get_data_dir()) == APP_OK);
    char log_path[256];
    snprintf(log_path, sizeof(log_path), "%s/log.txt", hal_fs_get_data_dir());
    dbg_log_init(log_path, DBG_LVL_WARN);
    dbg_stub_reset();

    run_lockstep_test(false); /* Real x86_64 JIT vs SM83 Interpreter */
    run_lockstep_test(true);  /* Simulated EACCES -> Interpreter fallback */

    dbg_log_shutdown();
    hal_sys_shutdown();
    printf("[PASS] test_sm83_jit_lockstep: bit-exact JIT vs Interpreter lockstep (ALU/DAA/CB/MBC1/SMC/Serial) + EACCES fallback passed!\n");
    return 0;
}
