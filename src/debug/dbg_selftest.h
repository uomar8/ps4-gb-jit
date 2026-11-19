#ifndef PS4_GB_JIT_DBG_SELFTEST_H
#define PS4_GB_JIT_DBG_SELFTEST_H

#include <stdint.h>
#include <stdbool.h>

typedef struct DbgSelfTestReport {
    bool abi_structs_ok;
    bool fs_data_write_ok;
    bool video_dmem_ok;
    bool audio_port_ok;
    bool pad_init_ok;
    bool jit_rwx_dualmap_ok;
    uint32_t passed;
    uint32_t failed;
} DbgSelfTestReport;

/*
 * Runs all 6 hardware/platform invariants without stopping on the first failure.
 * Returns true if all mandatory checks passed (JIT failure is logged as WARN
 * because interpreter fallback remains available).
 */
bool dbg_selftest_run(DbgSelfTestReport *out_report);

#endif /* PS4_GB_JIT_DBG_SELFTEST_H */
