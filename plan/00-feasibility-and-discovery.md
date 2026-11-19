# 00 — Feasibility, Planning Decisions & Phase 0 Dependency Audit

## 1. Feasibility Gate Verdict
- **Verdict**: **100% Feasible — Full Speed (59.73 Hz / 60 FPS locked + 4×–8× fast-forward headroom)** on PS4 Slim (`CUH-20xx..22xx`, Orbis OS FW 13.52, GoldHEN).
- **Hardware Fit**:
  - **CPU**: AMD Jaguar 1.6 GHz x86_64 (`-march=btver2`, no AVX2/FMA3) vs 4.194304 MHz 8-bit Sharp SM83. Single Jaguar core easily executes JIT-compiled x86_64 blocks + scanline PPU + 48 kHz APU + 1080p integer blit.
  - **Memory**: 8 GB GDDR5 (~5–5.5 GB app-usable) vs 64 KiB DMG address space + up to 8 MiB ROM. Total app footprint < 64 MiB.
  - **Orbis W^X JIT**: Dual-mapped shared memory via `sceKernelJitCreateSharedMemory` (`0x07` RWX maxProt) + `sceKernelJitCreateAliasOfSharedMemory` (`0x03` RW) + `16 KiB` (`0x4000`) alignment + `--authinfo` with `0xE0` JIT bit.
  - **Video**: Native `sceVideoOut` 1080p ARGB8888 (`0x80000000`) on `2 MiB`-aligned DirectMemory (`prot = 0x33`). Zero Piglet EGL / `0x3800000000000035` AUTHINFO conflicts and zero shader stutter.
  - **Audio**: Single process-lifetime `sceAudioOut` port (`0xFF`, `48000` Hz stereo, `256`–`512` sample grain, `sceAudioOutSetVolume` = `32768`).
- **Paradigm Selected**: Native PS4 `.pkg` with custom baseline x86_64 Dynamic Recompiler (JIT) + HLE DMG subsystems + reference SM83 interpreter (for Tier 2 CI lockstep differential testing and automatic runtime fallback).

## 2. Planning Chat Q&A Decisions
| Topic | Locked Decision |
| :--- | :--- |
| **Target Console & Scope** | Original Game Boy (**DMG-01**) only: MBC0, MBC1, MBC2, MBC3 (with RTC stub/clock), MBC5. No GBC in Phase 1. |
| **CI Test Fixtures** | Public-domain DMG test ROMs & synthesized unit test ROMs: Blargg `cpu_instrs`, `instr_timing`, `dmg-acid2` PPU test, plus synthetic MBC/JIT/SMC unit ROMs generated in `tests/fixtures/`. |
| **1080p UI Frontend** | Lightweight native 1080p software UI rendered directly into the `sceVideoOut` ARGB8888 framebuffer (8×8 / 16×16 bitmap font scaled cleanly, zero Piglet/SDL2 dependencies, clean `0xE0` JIT authinfo). |
| **ROM & Save Paths** | `/data/gbjit/roms/` (FTP `:2121`) and `/data/gbjit/saves/` (`.sav` & `.state`) only — no USB or GoldHEN syscall 500 sandbox escape. |
| **Repository** | Private GitHub repo `uomar8/ps4-gb-jit`, build-only 3-job GitHub Actions CI (`archive: false` for `.pkg`). |

## 3. Phase 0 Target-to-Boot Dependency Audit

### 3.1 DMG-01 Hardware & Subsystem Matrix
| Subsystem | Scope / Registers / Features | Phase 1–5 Status | Notes |
| :--- | :--- | :--- | :--- |
| **Boot ROM (`0x0000–0x00FF`)** | DMG-01 post-boot CPU + I/O register state (`AF=01B0`, `PC=0100`, `SP=FFFE`, `LCDC=91`) | **Implemented (HLE)** | No proprietary Nintendo ROM needed. |
| **SM83 CPU ISA** | All 245 primary opcodes + 256 `0xCB` prefixed opcodes, `IME`, `EI` 1-inst delay, `HALT`, `STOP`, `DAA`, flags `Z N H C` | **Implemented (Interp + JIT)** | Interpreter is lockstep oracle; JIT compiles basic blocks with interpreter fallback for rare/SMC cases. |
| **Memory Map** | ROM0 (`0000-3FFF`), ROMX (`4000-7FFF`), VRAM (`8000-9FFF`), ERAM (`A000-BFFF`), WRAM (`C000-DFFF`), Echo (`E000-FDFF`), OAM (`FE00-FE9F`), IO (`FF00-FF7F`), HRAM (`FF80-FFFE`), IE (`FFFF`) | **Implemented** | Echo RAM mirrors `C000-DDFF`; unused `FEA0-FEFF` returns `0xFF`. |
| **Cartridge Mappers** | `MBC0` (ROM only), `MBC1` (ROM/RAM banking), `MBC2` (512×4-bit RAM), `MBC3` (ROM/RAM + basic RTC), `MBC5` (up to 8 MiB ROM + 128 KiB RAM) | **Implemented (HLE)** | Unhandled mapper types logged via Stub-Discovery (`missing_report.txt`). |
| **Interrupts** | `IF` (`FF0F`), `IE` (`FFFF`): VBlank (`0x40`), LCD STAT (`0x48`), Timer (`0x50`), Serial (`0x58`), Joypad (`0x60`) | **Implemented** | Checked at block boundaries and `EI`/`HALT` transitions. |
| **Timer** | `DIV` (`FF04`), `TIMA` (`FF05`), `TMA` (`FF06`), `TAC` (`FF07`) | **Implemented (HLE)** | Accurate T-cycle counter with reload + Timer IRQ. |
| **Joypad** | `JOYP` (`FF00`) D-Pad / Action button matrix + Joypad IRQ | **Implemented (HLE)** | Mapped from DS4 (`scePad`) / Windows keyboard+gamepad / Mock scripts. |
| **PPU (LCD)** | `LCDC` (`FF40`), `STAT` (`FF41`), `SCY`/`SCX` (`FF42`/`FF43`), `LY` (`FF44`), `LYC` (`FF45`), `DMA` (`FF46`), `BGP`/`OBP0`/`OBP1` (`FF47`–`FF49`), `WY`/`WX` (`FF4A`/`FF4B`) | **Implemented (HLE Scanline)** | 160×144 4-shade (selectable palettes), BG + Window + 40 OAM sprites (8×8 & 8×16, priority, X/Y flip), OAM DMA instant copy + cycle accounting. |
| **APU (Audio)** | Ch1 (Pulse+Sweep), Ch2 (Pulse), Ch3 (Wave RAM `FF30-FF3F`), Ch4 (Noise LFSR), `NR10–NR52` (`FF10–FF26`) | **Implemented (HLE 48 kHz)** | High-level synthesis downsampled directly to 48000 Hz stereo `int16_t`. |
| **Serial Port** | `SB` (`FF01`), `SC` (`FF02`) | **Stubbed (HLE)** | Captures serial output bytes (needed for Blargg `cpu_instrs` test output!), returns `0xFF` for link cable peer. |
| **Link Cable / Printer / SGB / GBC** | Real multiplayer serial, Game Boy Printer, Super Game Boy borders, CGB double-speed/VRAM banks | **Deferred** | Out of DMG-01 scope; logged if accessed via Stub-Discovery. |

### 3.2 Orbis OS / PS4 Dependency Audit (`v0.5.4` OpenOrbis)
| Orbis Module / API | Purpose | Status | ABI / Packaging Rule |
| :--- | :--- | :--- | :--- |
| `sce_module/libc.prx`, `libSceFios2.prx` | Mandatory retail runtime PRXs | **Required in PKG** | Verified in Tier 3 CI via `PkgTool.Core pkg_listentries`. |
| `libkernel` (`sceKernelAllocateDirectMemory`, `MapDirectMemory`, `JitCreateSharedMemory`, `JitCreateAliasOfSharedMemory`, `Stat`, `Getdents`, `DebugOutText`, `GetProcessTime`) | Memory, JIT W^X, FS, Klog, timing | **Implemented (PS4 HAL)** | `DirectMemory` 2 MiB aligned `prot=0x33`; JIT `16 KiB` aligned `0x07`/`0x03`; `OrbisStatWire` 120B `st_size @72`; skip `d_fileno==0 || d_type==14`. |
| `libSceVideoOut` | 1080p ARGB8888 double/triple buffered flip | **Implemented (PS4 HAL)** | Pixel format `0x80000000U`, `sceVideoOutSubmitFlip`, `sceVideoOutWaitVblank`. |
| `libSceAudioOut` | 1 process-lifetime 48 kHz stereo port | **Implemented (PS4 HAL)** | User `0xFF`, `48000` Hz, grain `512`, `sceAudioOutSetVolume` `32768`, dedicated 1 MiB stack thread. |
| `libScePad` + `libSceUserService` | DS4 buttons, sticks, triggers, `L3+R3` HUD | **Implemented (PS4 HAL)** | `sceUserServiceInitialize` -> `sceUserServiceGetInitialUser` -> `scePadOpen`. |
| `libSceSystemService` | Hide splash screen, clean exit | **Implemented (PS4 HAL)** | `sceSystemServiceHideSplashScreen()`, exit via `sceSystemServiceLoadExec("exit", NULL)`. |
| `libScePigletv2VSH` / `libSceNet` / `libSceImeDialog` / GoldHEN Syscall `500` | Not used | **Omitted** | Keeps `eboot.bin` minimal, avoids FW 13.52 Piglet/IME authinfo conflicts and root-escape edge cases. |
