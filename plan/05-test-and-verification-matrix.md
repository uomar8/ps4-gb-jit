# 05 — 5-Tier Verification Matrix & Pre-PS4 Sign-Off Checklist

## 1. Hard Verification Rules
1. **No Build-Only Sign-Off**: Never ask the user to install/test a `.pkg` on PS4 Slim FW 13.52 until **Tiers 1–3 pass in Cloud / GitHub Actions** and **Tier 4 is ready on Windows PC**.
2. **Regression Test on Every Bug**: Every bug discovered in CI, Windows, `shadPS4`, or real PS4 gets a permanent Tier 2 unit or E2E test in `tests/`.
3. **Batch Real PS4 Fixes**: Log every PS4 run in `plan/ps4-runs/run-NN.md` and fix the entire batch off-console before cutting the next `APP_VER` `.pkg`.

## 2. 5-Tier Verification Matrix

| Feature / Module | Tier 1 (CI Static/ABI) | Tier 2 (CI ASan/UBSan/TSan + Mock-Orbis) | Tier 3 (CI PS4 Binary/PKG Audit) | Tier 4 (Windows `.exe` + `shadPS4`) | Tier 5 (PS4 Slim FW 13.52) |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **Orbis Wire Structs & ABI** | `_Static_assert` + `lint.sh` bans (`getcwd`, `lstat`, `access`, `dup(`, `timer_create`) | `test_abi_structs` + `OrbisStatWire` offset checks | `llvm-nm` forbidden symbol check (`_exit`, `getcwd`, `posix_spawn`) | Boot-Time Self-Test in `shadPS4` | Real `sceKernelStat`/`Getdents` self-test |
| **Logger, Stub-Discovery & Crash Handler** | `-Wall -Wextra -Werror=vla -Werror=alloca` | `test_dbg_log`, `test_stub_report` sorted output | Stack frame `.su` <= 32 KiB | Verify `./data/log.txt` & `missing_report.txt` | FTP `:2121` pull `/data/gbjit/log.txt`, `klog.txt`, `missing_report.txt` |
| **Cartridge & MBC0/1/2/3/5** | Clean compile | `test_mbc` (bank switching, SRAM enable/save, RTC, invalid mapper stub) | No AVX2/FMA3 in `cart.o` | Load real `.gb` ROMs on Windows | Load `.gb` from `/data/gbjit/roms/` & `/mnt/usb0/` |
| **SM83 Reference Interpreter** | Clean compile | `test_sm83_unit` + Blargg `cpu_instrs` serial PASS | No AVX2/FMA3 | Run games in Interp mode | Fallback mode sanity check |
| **x86_64 Dynamic Recompiler (JIT)** | `lint.sh` + zero AVX2 emitter check | `test_jit_smoke` (`0x1337C0DE`), **JIT-vs-Interp lockstep differential test** on all opcode classes + Blargg `cpu_instrs` + SMC/bank-switch + JIT-alloc-fail fallback | `llvm-objdump` zero AVX2/FMA3 + `--authinfo` `0xE0` JIT check | Interactive Windows dual-view JIT (`VirtualAlloc`/`CreateFileMapping`) | Real `sceKernelJitCreateSharedMemory` `0x07`/`0x03` self-test + 60 FPS gameplay |
| **HLE PPU (Scanline 160×144)** | Clean compile | `test_ppu_acid2` (framebuffer checksum / reference match) | Stack <= 32 KiB | Visual check at 1080p on Windows | Real `sceVideoOut` 1080p ARGB8888 output |
| **HLE APU (48 kHz 4-Ch)** | Clean compile | `test_apu_ring` (SPSC lock-free under TSan, zero underruns) | Stack <= 32 KiB | Audible check via WASAPI on Windows | Real `sceAudioOut` 1-port 48 kHz pacing |
| **1080p UI & DS4 Controls** | Clean compile | **Scripted E2E Controller Walkthroughs**: Browser -> Load ROM -> In-Game -> Pause Modal (>500 ms open) -> Settings -> Palette -> JIT Toggle -> HUD (`L3+R3`) -> Exit; non-blank framebuffer & zero `ERROR` logs | `PkgTool.Core pkg_listentries` (`libc.prx`, `libSceFios2.prx`) + `pkg_validate` | Interactive DS4/Keyboard + `F12` HUD on Windows | Real DS4 buttons, Rest Mode enter/exit, clean `LoadExec("exit")` |
| **Mock-Orbis Fault Injection** | — | `DT_WHT=14` / `d_fileno==0` ghosts, `st_ino==0`, read-only `/app0/`, `16 KiB` alignment, JIT alloc `EACCES` fallback | — | — | Confirm real `/data/gbjit/` & `/mnt/usb0/` behavior |

## 3. Automated 3-Job GitHub Actions CI (`.github/workflows/build.yml`)
1. **`test-and-lint` (`ubuntu-latest`)**:
   - Tier 1: `scripts/lint.sh`, `-Wall -Wextra -Werror=vla -Werror=alloca -fstack-usage`.
   - Tier 2: Build & run all unit, JIT-vs-Interpreter lockstep, Mock-Orbis fault-injection, and scripted E2E walkthrough tests under **ASan + UBSan** (and audio ring-buffer under **TSan**).
2. **`build-windows` (`windows-latest`)**:
   - Tier 4: Build interactive Windows x64 `ps4-gb-jit-win64.exe` and upload artifact.
   - *(Note: `DOTNET_SYSTEM_GLOBALIZATION_INVARIANT` is NEVER set at workflow level so PowerShell `Expand-Archive` works cleanly.)*
3. **`build-ps4` (`ubuntu-22.04`)**:
   - Scoped `env:` with `DOTNET_SYSTEM_GLOBALIZATION_INVARIANT: "1"` and `DOTNET_BUNDLE_EXTRACT_BASE_DIR: "/tmp/dotnet_extract"`.
   - Cached OpenOrbis `v0.5.4` (`actions/cache@v6`), dynamic root discovery.
   - Cross-compile `-march=btver2 -mtune=btver2 -O3`, link `eboot.elf`, `create-fself` with `--authinfo` (`0xE0` JIT bit) and `-out=build/eboot.oelf --eboot=eboot.bin`, build `PS4-GB-JIT-v01.01.pkg`.
   - Tier 3: `scripts/audit_ps4.sh` fails job if AVX2/FMA3 found, forbidden symbols found, stack frame > 32 KiB, `sce_module/libc.prx` or `libSceFios2.prx` missing, or `pkg_validate` fails.
   - Upload single `.pkg` via `actions/upload-artifact@v7` with **`archive: false`**.

## 4. Mandatory Pre-PS4 Active Sign-Off Checklist
Before asking the user to install any `.pkg` on PS4 Slim FW 13.52, verify and report:
- [ ] Tier 1 static lint & `_Static_assert` ABI checks pass with 0 warnings/errors.
- [ ] Tier 2 ASan/UBSan unit tests, JIT-vs-Interpreter lockstep tests, Mock-Orbis fault tests, and scripted E2E controller walkthroughs pass with 0 `ERROR`/`FATAL` logs.
- [ ] Tier 3 PS4 binary/ISA/symbol/stack/PRX/PKG audit passes (`0` AVX2/FMA3, `libc.prx` + `libSceFios2.prx` present, `pkg_validate` OK).
- [ ] Tier 4 Windows HAL `.exe` artifact is built and ready for interactive PC verification.
- [ ] `APP_VER` incremented and reflected in uploaded `.pkg` filename.
- [ ] Explicit list of remaining Tier 5 hardware-only checks provided.
