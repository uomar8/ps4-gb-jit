#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/gb_machine.h"
#include "core/cpu/sm83_interp.h"
#include "core/jit/jit_engine.h"
#include "debug/dbg_log.h"
#include "debug/dbg_stub.h"
#include "hal/hal.h"

#define CHECK(cond) do { \
    if (!(cond)) { \
        fprintf(stderr, "[FAIL] %s:%d: check failed: %s\n", __FILE__, __LINE__, #cond); \
        return 1; \
    } \
} while (0)

static uint8_t *build_win_lockstep_rom(size_t *out_size) {
    size_t sz = 32768;
    uint8_t *rom = (uint8_t *)calloc(1, sz);
    if (!rom) return NULL;
    memcpy(rom + 0x0134, "WINJITTEST", 10);
    rom[0x0147] = 0x00;
    rom[0x0148] = 0x00;
    rom[0x0149] = 0x00;

    size_t p = 0x0100;
    rom[p++] = 0x00;
    rom[p++] = 0xC3; rom[p++] = 0x50; rom[p++] = 0x01;

    p = 0x0150;
    rom[p++] = 0x3E; rom[p++] = 0x19; /* LD A, 0x19 */
    rom[p++] = 0xC6; rom[p++] = 0x28; /* ADD A, 0x28 */
    rom[p++] = 0x27;                  /* DAA */
    rom[p++] = 0x06; rom[p++] = 0x10; /* LD B, 16 */
    rom[p++] = 0x21; rom[p++] = 0x00; rom[p++] = 0xC0; /* LD HL, 0xC000 */
    uint16_t lp = (uint16_t)p;
    rom[p++] = 0xCB; rom[p++] = 0x37; /* SWAP A */
    rom[p++] = 0x22;                  /* LD (HL+), A */
    rom[p++] = 0x80;                  /* ADD A, B */
    rom[p++] = 0x05;                  /* DEC B */
    rom[p++] = 0x20;
    { int8_t rel = (int8_t)(lp - (p + 1)); rom[p++] = (uint8_t)rel; }

    /* Serial "OK" */
    rom[p++] = 0x3E; rom[p++] = 'O'; rom[p++] = 0xE0; rom[p++] = 0x01;
    rom[p++] = 0x3E; rom[p++] = 0x81; rom[p++] = 0xE0; rom[p++] = 0x02;
    rom[p++] = 0x3E; rom[p++] = 'K'; rom[p++] = 0xE0; rom[p++] = 0x01;
    rom[p++] = 0x3E; rom[p++] = 0x81; rom[p++] = 0xE0; rom[p++] = 0x02;
    rom[p++] = 0x18; rom[p++] = 0xFE; /* JR -2 */

    *out_size = sz;
    return rom;
}

int main(void) {
    CHECK(hal_sys_init() == APP_OK);
    CHECK(hal_fs_mkdir_p(hal_fs_get_data_dir()) == APP_OK);
    dbg_log_init("./data/win_jit_test.log", DBG_LVL_WARN);
    dbg_stub_reset();

    size_t sz = 0;
    uint8_t *rom = build_win_lockstep_rom(&sz);
    CHECK(rom != NULL);

    GbMachine *m_interp = (GbMachine *)calloc(1, sizeof(GbMachine));
    GbMachine *m_jit    = (GbMachine *)calloc(1, sizeof(GbMachine));
    CHECK(m_interp != NULL && m_jit != NULL);
    gb_machine_init(m_interp);
    gb_machine_init(m_jit);
    CHECK(gb_machine_load_rom_buffer(m_interp, rom, sz) == APP_OK);
    CHECK(gb_machine_load_rom_buffer(m_jit, rom, sz) == APP_OK);

    JitEngine jit;
    memset(&jit, 0, sizeof(jit));
    CHECK(jit_engine_init(&jit, true) == APP_OK);
    CHECK(jit.available && jit.enabled);

    for (int f = 0; f < 4; ++f) {
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
            CHECK(m_interp->cpu.a == m_jit->cpu.a);
            CHECK(m_interp->cpu.f == m_jit->cpu.f);
            CHECK(m_interp->cpu.pc == m_jit->cpu.pc);
            CHECK(m_interp->cpu.sp == m_jit->cpu.sp);
            CHECK(m_interp->total_cycles == m_jit->total_cycles);
            CHECK(memcmp(m_interp->wram, m_jit->wram, sizeof(m_interp->wram)) == 0);
        }
    }

    CHECK(strcmp(m_jit->serial_buf, "OK") == 0);
    CHECK(jit.blocks_compiled >= 3);

    uint32_t compiled = jit.blocks_compiled;
    jit_engine_shutdown(&jit);
    gb_machine_destroy(m_interp);
    gb_machine_destroy(m_jit);
    free(m_interp);
    free(m_jit);
    free(rom);
    dbg_log_shutdown();
    hal_sys_shutdown();

    printf("[PASS] test_win_jit_selftest: Win64 CreateFileMapping dual RW/RX JIT + ABI lockstep passed (%u blocks)!\n",
           compiled);
    return 0;
}
