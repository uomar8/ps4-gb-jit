# PS4 Game Boy (DMG-01) Dynamic Recompiler (JIT) + HLE (`ps4-gb-jit`)

- **Target Hardware**: PlayStation 4 Slim (`CUH-20xx..22xx`), Orbis OS FW 13.52 (GoldHEN `2.4b18.12+`)
- **Primary Output**: Native PS4 `.pkg` (`GBJT00001`, starting `APP_VER=01.01`, OpenOrbis `v0.5.4`, `-march=btver2`, `--authinfo` with `0xE0` JIT bit)
- **Off-Console Targets**: Linux x64 `host_run` + ASan/UBSan/TSan test suite (GitHub Actions CI), Windows x64 interactive HAL `.exe` (Win32 + WASAPI + XInput/DS4, built in CI)
- **Core Approach**: Custom baseline x86_64 block-based Dynamic Recompiler (JIT) paired with High-Level Emulation (HLE) of DMG Boot ROM, scanline PPU, 48 kHz APU, OAM DMA, and MBC0/1/2/3/5 mappers — backed by a reference SM83 interpreter for Tier 2 CI lockstep verification and automatic runtime fallback.

## Modular Blueprint Index
1. [00-feasibility-and-discovery.md](plan/00-feasibility-and-discovery.md) — Feasibility verdict, planning Q&A decisions, and Phase 0 DMG/Orbis Dependency Audit.
2. [01-architecture-and-flow.md](plan/01-architecture-and-flow.md) — System architecture, data flow, memory budgets (`2 MiB` DirectMemory, `16 KiB` dual-mapped JIT), threading, and Inverted Phase Order.
3. [02-folder-and-naming-spec.md](plan/02-folder-and-naming-spec.md) — Directory tree, strict HAL boundary rules, naming conventions, and `AppResult` error handling.
4. [03-algorithms-and-apis.md](plan/03-algorithms-and-apis.md) — x86_64 JIT codegen/cache/SMC design, HLE subsystems, UI state machines & DS4 controls, wire/kernel structs (`_Static_assert`), and Orbis ABI rules.
5. [04-debug-and-telemetry-spec.md](plan/04-debug-and-telemetry-spec.md) — Unified structured logs, `dbg_sce_strerror()`, `/data/gbjit/` FTP diagnostics (`log.txt`, `klog.txt`, `missing_report.txt`), `L3+R3`/`F12` Live Debug HUD, Boot-Time Self-Test, and Crash Handler.
6. [05-test-and-verification-matrix.md](plan/05-test-and-verification-matrix.md) — 5-Tier Verification Matrix, scripted E2E walkthroughs, JIT-vs-Interpreter lockstep tests, Mock-Orbis fault injection, and Pre-PS4 Active Sign-Off Checklist.

## Milestones (Inverted Phase Order)
- [x] **Phase 0 — Feasibility Gate, Planning Chat & Target Dependency Audit** (`plan/00`–`05` approved by user)
- [x] **Phase 1 — Test & Debug Harness FIRST** (HAL `win`/`ps4`/`mock`, structured logger + `dbg_sce_strerror()`, Stub-Discovery `missing_report.txt`, Crash Handler with Orbis `mc_rip`/`mc_rsp` offsets, 6-point Boot-Time Self-Test, 3-job GitHub Actions CI + Tier 1 static lint + Tier 3 binary/PKG audit, `host_run` & Windows HAL `.exe`)
- [x] **Phase 2 — DMG Memory Bus, Cart/MBC0-5 & HLE Subsystems** (DMG Boot ROM HLE, Timer, Joypad, Scanline PPU + 7x 1080p blit, OAM DMA, 48 kHz 4-ch APU, Battery SRAM `.sav` & Save States)
- [x] **Phase 3 — Reference SM83 Interpreter** (100% SM83 primary + `0xCB` ISA, `DAA`, `EI` delay, `HALT`/`STOP`, interrupts, serial capture)
- [x] **Phase 4 — Baseline x86_64 Dynamic Recompiler (JIT) + Lockstep Differential Verification** (Dual-mapped RW/RX emitter, zero AVX2/FMA3, `(bank, pc)` block cache, WRAM/HRAM SMC safety, automatic EACCES interpreter fallback, 100% bit-exact JIT-vs-Interpreter lockstep pass in ASan/UBSan CI)
- [x] **Phase 5 — Lightweight Native 1080p UI + DS4 Controls + Active E2E Walkthroughs** (ROM browser for `/data/gbjit/roms/`, Pause/Settings/Error modals, Touchpad=SELECT / R1=START / Options=Pause / L3+R3=HUD, scripted E2E walkthrough in CI)
- [ ] **Phase 6 — Tier 4 Windows PC Interactive Testing** *(ACTIVE NOW)*: Verify `ps4-gb-jit-win64.exe` on Windows 10/11 (Win64 JIT lockstep CI + interactive ROM gameplay, audio, save states, settings, `F12` HUD)
- [ ] **Phase 7 — Tier 5 Real PS4 Slim FW 13.52 Batched Hardware Runs** *(DEFERRED — no PS4 available right now; `PS4-GB-JIT-v01.01.pkg` already built & Tier 3 audited, ready when console is available)*

## Current Blockers / Decisions Pending
- PS4 console temporarily unavailable — focusing on **Tier 4 Windows PC verification** (`ps4-gb-jit-win64.exe`) first; PS4 hardware runs (`plan/ps4-runs/`) will resume once console is available.
