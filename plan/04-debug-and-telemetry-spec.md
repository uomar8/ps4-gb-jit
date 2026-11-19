# 04 — Debug, Logging, Telemetry & Crash Diagnostics Specification

## 1. Unified Structured Log Format
Every log line across PS4, Windows, and Linux CI follows one strict format:
```text
[timestamp_us] [SUBSYSTEM] [LEVEL] [file:line func()] message key=val rc=0x80xxxxxx (DECODED_NAME)
```
- **Subsystems**: `SYS`, `HAL`, `VID`, `AUD`, `INP`, `FS`, `JIT`, `CPU`, `PPU`, `APU`, `CART`, `UI`, `TEST`, `STUB`, `CRASH`.
- **Levels**: `DEBUG`, `INFO`, `WARN`, `ERROR`, `FATAL`.
- **`dbg_sce_strerror(int32_t rc)`**: Decodes common Orbis error codes (`0x8002000D EACCES`, `0x80020016 EINVAL`, `0x8002000C ENOMEM`, `0x80020002 ENOENT`, `0x80290013 VIDEO_OUT_INVALID_MEMORY`, `0x80260008 AUDIO_OUT_INVALID_SAMPLE_RATE`, `0x80260004 AUDIO_OUT_PORT_FULL`, etc.) plus `AppResult` codes.
- **Immediate Persistence**: Every `WARN`/`ERROR`/`FATAL` and milestone `INFO` calls `fflush()` + `fsync()` on `/data/gbjit/log.txt` and tees to GoldHEN Klog via `sceKernelDebugOutText(0, line)`.

## 2. Per-App FTP Diagnostic Directory (`/data/gbjit/`)
All outputs are isolated inside `/data/gbjit/` so a single Android/PC FTP (`:2121`) download reveals the full state of a PS4 run:
| File | Purpose |
| :--- | :--- |
| `/data/gbjit/log.txt` | Full timestamped structured log from boot through exit/crash. |
| `/data/gbjit/klog.txt` | Non-blocking `/dev/klog` snapshot captured at startup and clean exit. |
| `/data/gbjit/missing_report.txt` | **Stub-Discovery Report**: every unimplemented MMIO register, mapper type, or opcode encountered, sorted by hit count descending. |
| `/data/gbjit/config.ini` | User/debug overrides (`jit_enabled=1`, `palette=0`, `log_level=INFO`, `usb_scan=1`, `force_interp=0`) editable over FTP without rebuilding a `.pkg`. |
| `/data/gbjit/roms/` | User `.gb` ROMs uploaded via FTP `:2121`. |
| `/data/gbjit/saves/` | Battery `.sav` files and `.state` save states. |

## 3. Stub-Discovery Mode
- Whenever an unimplemented DMG hardware feature is hit (e.g., unknown MBC type, GBC-only MMIO register `0xFF4D`/`0xFF4F`, link cable transfer, or unimplemented opcode), `dbg_stub_record(category, id, detail)` increments a deduplicated counter table and logs `[STUB]` on first hit.
- On clean exit or pause menu dump, `dbg_stub_write_report("/data/gbjit/missing_report.txt")` writes a sorted summary (`count | category | id | detail`).

## 4. Boot-Time Hardware Self-Test (One-Shot Diagnostic Pass)
Runs automatically at startup **without stopping on the first failure**, logging `PASS` or `FAIL` with decoded `rc` for every invariant:
1. `ABI_STRUCTS`: Verify `sizeof(OrbisStatWire) == 120`, `offsetof(st_size) == 72`, `HAL_PAGE_SIZE == 16384`.
2. `FS_DATA_WRITE`: Create/write/stat/unlink probe file in `/data/gbjit/` and verify `st_size` matches bytes written.
3. `VIDEO_DMEM`: Verify `2 MiB`-aligned DirectMemory (`prot = 0x33`) allocation and `sceVideoOut` buffer registration.
4. `AUDIO_PORT`: Verify single `48000` Hz stereo port open + `sceAudioOutSetVolume(32768)` + 1 silent buffer submit.
5. `PAD_INIT`: Verify `sceUserService` + `scePadOpen` handle.
6. `JIT_RWX_DUALMAP`: Allocate dual-mapped RW/RX JIT page, emit `MOV EAX, 0x1337C0DE; RET` into `rw_base`, flush icache, execute `rx_base()`, and assert return value `== 0x1337C0DE`!
7. `GOLDHEN_USB`: Test GoldHEN syscall `500` (with `SIGSYS` ignored + immediate `setuid(backup.cr_uid)`) and list mounted `/mnt/usb0..7/` drives.

## 5. Live Debug HUD (`L3 + R3` on PS4 / `F12` on Windows)
Semi-transparent overlay rendered cleanly in the top-left corner of the 1080p framebuffer:
- Current UI Focus Zone & Active Modal (`ROM_BROWSER`, `IN_GAME`, `PAUSE_MODAL`, etc.)
- Last DS4 / Keyboard button pressed + hold duration
- Execution Mode (`JIT` vs `INTERP`), JIT blocks compiled, cache usage (`KB / 4096 KB`), cache flushes
- Frame time (`ms`), FPS, Audio ring-buffer fill (`%`), Free RAM / DirectMemory
- Last 6 structured log lines (color-coded by level)

## 6. Crash Signal Handler
- Hooks `SIGSEGV`, `SIGBUS`, `SIGILL`, `SIGABRT`, `SIGFPE` (ignores `SIGSYS`).
- Records `LOG_STEP("...")` breadcrumbs in a lock-free ring buffer.
- On signal: extracts `mc_rip` / `mc_rsp` / faulting address from `ucontext_t`, writes signal number + `mc_rip` + last 8 `LOG_STEP()` entries directly via POSIX `write()` + `fsync()` to `/data/gbjit/log.txt` and `sceKernelDebugOutText()`, then exits cleanly.
