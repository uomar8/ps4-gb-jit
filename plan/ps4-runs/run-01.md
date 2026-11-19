# PS4 Run 01 — `PS4-GB-JIT-v01.01.pkg` (PS4 Slim FW 13.52, GoldHEN)

## Observed Result
- `eboot.bin` booted, created `/data/gbjit/`, `/data/gbjit/roms/`, `/data/gbjit/saves/`, `/data/gbjit/config.ini`, and `/data/gbjit/log.txt`.
- `log.txt` contained 1 line:
  `[1142633] [SYS] [INFO] [app.c:72 app_run()] ps4-gb-jit booting (DMG-01 x86_64 JIT + HLE)`
- App then crashed with **`CE-34878-0`** before `hal_video_init()` logged completion, with no `klog.txt`, no `missing_report.txt`, and no `[CRASH]` line in `log.txt`.

## Root Causes Identified & Batched Fixes for `v01.02`
1. **Orbis Kernel `sceKernelOpen()` Flags Mismatch (`src/hal/ps4/hal_ps4_fs.c`)**:
   - Passed OpenOrbis `musl` Linux `<fcntl.h>` flags (`O_CREAT=0x40`, `O_TRUNC=0x200`, `O_DIRECTORY=0x10000`) directly to `sceKernelOpen()`.
   - On FreeBSD/Orbis kernel, `0x0040` is **`O_ASYNC`**, while `ORBIS_KERNEL_O_CREAT=0x0200`, `ORBIS_KERNEL_O_TRUNC=0x0400`, `ORBIS_KERNEL_O_DIRECTORY=0x20000`.
   - **Fix**: Replaced with explicit `PS4_O_*` Orbis kernel constants.
2. **Orbis Service Init Order (`src/core/app.c`)**:
   - `hal_video_init()` (`sceVideoOutOpen`) was called before `hal_input_init()` (`sceUserServiceInitialize`).
   - **Fix**: Initialize `hal_input_init()` (`sceUserServiceInitialize` + `scePadInit`) first, then `hal_video_init()`, then `hal_audio_init()`.
3. **Crash Handler `SA_SIGINFO` Flag Mismatch (`src/debug/dbg_crash.c`)**:
   - Passed `musl` Linux `SA_SIGINFO` (`4`) instead of FreeBSD/Orbis `SA_SIGINFO` (`0x0040`).
   - **Fix**: Use `0x0040` under `#if defined(__ORBIS__) || defined(__PS4__)`.
4. **Granular Init Breadcrumbs (`src/core/app.c`, `src/hal/ps4/hal_ps4_video.c`)**:
   - Added flushed `LOG_INFO` breadcrumbs after `app_load_config()` and before every `sceVideoOut` / DirectMemory call so any remaining hardware issue is pinpointed to the exact line on Run 2.
