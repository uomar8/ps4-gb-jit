# PS4 Run 02 — `PS4-GB-JIT-v01.02.pkg` (PS4 Slim FW 13.52, GoldHEN)

## Observed Result
- `log.txt` reached line 77:
  `[1140025] [SYS] [INFO] [app.c:72 app_run()] ps4-gb-jit booting (DMG-01 x86_64 JIT + HLE)`
  `[1149298] [SYS] [INFO] [app.c:77 app_run()] config loaded jit=1 palette=0`
- Crashed immediately inside `hal_input_init()` (`sceUserServiceInitialize`) — proving the exact same root cause as Run 01 (`sceVideoOutOpen`): **internal firmware sysmodules were not loaded via `sceSysmoduleLoadModuleInternal()`**, and `sceUserServiceInitialize(NULL)` passed `NULL` instead of a valid `OrbisUserServiceInitializeParams`.
- `klog.txt` was not yet created because `hal_sys_snapshot_klog()` ran at line 91 (after subsystem init).

## Batched Fixes for `v01.03`
1. **Load Internal Sysmodules (`src/hal/ps4/hal_ps4_sys.c` + `Makefile`)**:
   - Link `-lSceSysmodule` and call `sceSysmoduleLoadModuleInternal()` for `SYSTEM_SERVICE`, `USER_SERVICE`, `PAD`, `VIDEO_OUT`, and `AUDIO_OUT` inside `hal_sys_init()`.
2. **Non-NULL `OrbisUserServiceInitializeParams` (`src/hal/ps4/hal_ps4_input.c`)**:
   - Pass `{ .priority = 700 }` to `sceUserServiceInitialize(&params)` and add step-by-step flushed `LOG_INFO` lines.
3. **Early `klog.txt` Snapshot (`src/core/app.c`)**:
   - Snapshot `/dev/klog` to `/data/gbjit/klog.txt` right after `dbg_log_init()` (in addition to post-selftest and shutdown).
