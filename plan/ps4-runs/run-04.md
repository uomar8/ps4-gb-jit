# PS4 Run 04 — `PS4-GB-JIT-v01.04.pkg` (PS4 Slim FW 13.52, GoldHEN)

## Observed Result — REAL x86_64 JIT 6/6 SELF-TEST PASS ON PS4 SLIM!
- **Boot-Time Self-Test: 6/6 PASS** (`JIT_RWX_DUALMAP status=PASS result=0x1337c0de` via GoldHEN RWX `mmap`!).
- **4 MiB x86_64 JIT Engine active** (`ptr=0x201600000 size=0x400000`, `enabled=1`) running *Super Mario Land* (`MBC1`) smoothly with Save/Load states, `L3+R3` HUD, and `Square` = GB B button.
- **Remaining Issue Reported**: Pause/Settings menu was still laggy.

## Root Cause & Fix for `v01.05`
- **Uncached DirectMemory Framebuffer Reads (`src/ui/ui_state.c`, `src/ui/ui_hud.c`)**:
  - `ui_render_rect_alpha` was reading back all 2,073,600 pixels (`8.3 MB`) from PS4 `sceVideoOut` DirectMemory on every Pause/Settings modal frame (and for the `L3+R3` HUD & toast), which stalls the Jaguar CPU (~100× slower than write-only `hle_ppu_blit_1080p`).
  - **Fix**: Eliminated **100% of CPU framebuffer reads** across all modals, HUD, and toasts (replaced with pure write-only `ui_render_rect` panels + crisp accent borders) and skipped the redundant software `hal_sys_sleep_us` on PS4 since `sceVideoOutSubmitFlip(..., ORBIS_VIDEO_OUT_FLIP_VSYNC)` already hardware-VSyncs at 60 Hz.
