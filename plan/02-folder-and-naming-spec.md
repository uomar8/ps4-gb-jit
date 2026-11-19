# 02 — Folder Structure, HAL Boundary & Naming Specification

## 1. Directory Tree
```text
ps4-gb-jit/
├── PLAN.md
├── Makefile                          # Builds host_run, tests, and PS4 eboot.bin / .pkg
├── CMakeLists.txt                    # Used by build-windows (MSVC/MinGW) & host builds
├── .github/
│   └── workflows/
│       └── build.yml                 # 3-job CI: test-and-lint, build-windows, build-ps4
├── plan/
│   ├── 00-feasibility-and-discovery.md
│   ├── 01-architecture-and-flow.md
│   ├── 02-folder-and-naming-spec.md
│   ├── 03-algorithms-and-apis.md
│   ├── 04-debug-and-telemetry-spec.md
│   ├── 05-test-and-verification-matrix.md
│   └── ps4-runs/                     # Post-mortem logs for batched Tier 5 PS4 runs
├── assets/
│   ├── icon0.png                     # 512x512 PS4 home screen icon
│   ├── pic0.png                      # 1920x1080 splash background
│   └── sce_module/                   # Bundled libc.prx & libSceFios2.prx (required for PKG)
├── scripts/
│   ├── lint.sh                       # Tier 1 static trap scanner
│   ├── audit_ps4.sh                  # Tier 3 ISA/symbol/stack/PRX/PKG audit
│   └── make_gp4.py                   # Deterministic pkg.gp4 generator
├── src/
│   ├── main.c                        # Shared entry point calling app_run()
│   ├── core/
│   │   ├── app.h / app.c             # Top-level app loop, config.ini, frame pacing
│   │   ├── result.h                  # AppResult enum & CHECK_RC macros
│   │   ├── gb_types.h                # Shared DMG structs & _Static_asserts
│   │   ├── gb_machine.h / .c         # Machine lifecycle, reset, HLE boot state, savestate
│   │   ├── mem/
│   │   │   ├── bus.h / bus.c         # 64 KiB memory map, MMIO router, OAM DMA
│   │   │   └── cart.h / cart.c       # ROM loader, header parser, MBC0/1/2/3/5 + SRAM
│   │   ├── cpu/
│   │   │   ├── sm83_interp.h / .c    # Reference SM83 interpreter (oracle + fallback)
│   │   │   └── sm83_tables.h / .c    # Opcode cycle & metadata tables
│   │   ├── jit/
│   │   │   ├── jit_engine.h / .c     # Block cache, (bank,pc) table, SMC invalidation
│   │   │   ├── jit_x64_emit.h / .c   # Baseline x86_64 encoder (zero AVX2/FMA3)
│   │   │   └── jit_compile.c         # SM83 basic-block -> x86_64 translator + HLE fast-paths
│   │   └── hle/
│   │       ├── ppu.h / ppu.c         # HLE scanline PPU (160x144, palettes, sprites, window)
│   │       ├── apu.h / apu.c         # HLE 4-ch APU (48000 Hz stereo + SPSC ring buffer)
│   │       └── timer.h / timer.c     # HLE DIV/TIMA/TMA/TAC & interrupt controller
│   ├── ui/
│   │   ├── ui_state.h / ui_state.c   # Deterministic focus zones & modal state machine
│   │   ├── ui_render.h / ui_render.c # 1080p software renderer, 8x8/16x16 bitmap font, blit
│   │   └── ui_hud.h / ui_hud.c       # L3+R3 / F12 Live Debug HUD overlay
│   ├── debug/
│   │   ├── dbg_log.h / dbg_log.c     # Structured logger, LOG_STEP(), Klog tee, file flush
│   │   ├── dbg_sce_err.h / .c        # dbg_sce_strerror() hex decoder
│   │   ├── dbg_stub.h / dbg_stub.c   # Stub-Discovery tracker -> missing_report.txt
│   │   ├── dbg_selftest.h / .c       # Boot-Time Hardware Self-Test (all subsystems)
│   │   └── dbg_crash.h / dbg_crash.c # Crash signal handler (mc_rip + last LOG_STEP)
│   └── hal/
│       ├── hal.h                     # Pure C HAL interface (video, audio, input, fs, jit, sys)
│       ├── ps4/                      # Orbis OS implementation (ONLY place libSce* is included)
│       │   ├── hal_ps4_sys.c         # Init, splash hide, GoldHEN syscall 500, clean exit, klog
│       │   ├── hal_ps4_video.c       # sceVideoOut 1080p DirectMemory 2 MiB prot=0x33
│       │   ├── hal_ps4_audio.c       # sceAudioOut 1-port 48 kHz + 1 MiB thread
│       │   ├── hal_ps4_input.c       # scePad DS4 polling
│       │   ├── hal_ps4_fs.c          # sceKernelStat (120B wire), sceKernelGetdents (DT_WHT filter)
│       │   └── hal_ps4_jit.c         # sceKernelJitCreateSharedMemory (0x07) + Alias (0x03)
│       ├── win/                      # Windows x64 interactive HAL (Tier 4)
│       │   ├── hal_win_sys.c
│       │   ├── hal_win_video.c       # Win32 GDI/DIB 1080p window
│       │   ├── hal_win_audio.c       # WASAPI / waveOut 48 kHz
│       │   ├── hal_win_input.c       # Keyboard + XInput/DS4
│       │   ├── hal_win_fs.c          # Win32 FindFirstFile / absolute paths
│       │   └── hal_win_jit.c         # CreateFileMapping dual RW/RX views
│       └── mock/                     # Headless Linux CI HAL (Tier 2)
│           ├── hal_mock_sys.c
│           ├── hal_mock_video.c      # Offscreen 1920x1080 buffer + checksum/PNG dump
│           ├── hal_mock_audio.c      # Ring-buffer sink + sample counter
│           ├── hal_mock_input.c      # Scripted DS4 button sequence player
│           ├── hal_mock_fs.c         # Fault-injectable FS (DT_WHT=14 ghosts, st_ino=0, RO /app0)
│           └── hal_mock_jit.c        # memfd_create dual RW/RX views + fault injection
└── tests/
    ├── unit/                         # Tier 2 unit & differential JIT-vs-Interp tests
    ├── e2e/                          # Tier 2 scripted controller walkthrough tests
    └── fixtures/                     # Synthetic test ROMs & public-domain DMG test suites
```

## 2. Strict HAL Boundary Rules
- **Zero `libSce*`, `<orbis/*>`, or `<windows.h>` leaks** into `src/core/`, `src/ui/`, or `src/debug/` (except optional platform crash-context helper behind `hal_sys_*`). Enforced automatically by `scripts/lint.sh` in Tier 1 CI.
- **Language Standard**: Clean **C11** (`-std=c11`) for the entire codebase — zero `libc++` / `__cxa_guard_acquire` / `std::regex` hazards, fast compile times, predictable stack frames, and trivial FFI with OpenOrbis `v0.5.4`, Clang/GCC on Linux, and MSVC/Clang on Windows.
- **Paths**: HAL provides `hal_fs_get_data_dir()` (`/data/gbjit` on PS4, `./data` on PC), `hal_fs_get_app_dir()` (`/app0` on PS4, `./assets` on PC), and `hal_fs_list_usb_mounts()` (`/mnt/usb0..7` on PS4). Never call `getcwd()` or `realpath(".")`.

## 3. Naming & Error-Handling Conventions
- **Functions**: `<subsystem>_<module>_<action>()` in `snake_case` (e.g., `jit_x64_emit_mov_r64_imm32()`, `hle_ppu_render_scanline()`, `hal_ps4_jit_alloc()`, `dbg_log_write()`).
- **Types**: `PascalCase` structs/enums (`GbMachine`, `JitBlock`, `AppResult`, `HalInputState`, `OrbisStatWire`).
- **Constants / Macros**: `UPPER_SNAKE_CASE` (`GB_SCREEN_W`, `JIT_CACHE_SIZE_BYTES`, `HAL_PAGE_SIZE`).
- **Return Codes**: Every fallible function returns `AppResult` (typed `int32_t` enum: `APP_OK = 0`, negative app errors, or wrapped Orbis `rc`) and **logs immediately at the failure site** with `rc=0x%08x (%s)` via `dbg_sce_strerror()`.
