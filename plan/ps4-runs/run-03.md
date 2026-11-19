# PS4 Run 03 — `PS4-GB-JIT-v01.03.pkg` (PS4 Slim FW 13.52, GoldHEN)

## Observed Result — PLAYABLE MILESTONE ACHIEVED!
- Booted cleanly on PS4 Slim FW 13.52 with GoldHEN:
  - `sceUserServiceInitialize`, `scePadOpen`, `sceVideoOut` (`20 MiB` DirectMemory `prot=0x33`), `sceAudioOut` (`48000 Hz`, grain `512`, `1 MiB` stack thread) all initialized with **0 errors**.
  - Boot-Time Self-Test: **5/6 PASS** (`ABI_STRUCTS`, `FS_DATA_WRITE`, `VIDEO_DMEM`, `AUDIO_PORT`, `PAD_INIT` all PASS; `JIT_RWX_DUALMAP` gracefully fell back to interpreter because `sceKernelJitCreateSharedMemory` returned `0x80020001` `EPERM` under PAID `0x3800000000000011`).
  - Scanned `/data/gbjit/roms/` → launched **`Super Mario Land (World).gb`** (`MBC1`, 64 KiB).
  - Played for ~3 minutes, tested Pause Modal, multiple Save States (`42056` bytes) & Load States, exited to ROM Browser, relaunched, **0 crashes**.

## User Feedback & Batched Fixes for `v01.04`
1. **Pause/Settings Menu Lag (`src/ui/ui_render.c`)**:
   - `ui_render_rect_alpha` ran 3 integer divisions (`/ 255U`) per pixel across all `1920×1080` pixels (~6.2M divisions/frame on 1.6 GHz Jaguar).
   - **Fix**: Fast shift-mask dim (`(px >> 2) & 0x003F3F3F`) for dark modal backdrops + `>> 8` alpha blend (zero divisions per pixel).
2. **Game Boy Button Mapping (`src/ui/ui_state.c`)**:
   - User requested **Square** as the second Game Boy button (`B`) instead of Circle.
   - **Fix**: Default **`Cross` = A**, **`Square` = B** (`SWAP A/B` swaps `Square` ↔ `Cross`).
3. **Real x86_64 JIT on GoldHEN PS4 (`src/hal/ps4/hal_ps4_jit.c`)**:
   - When `sceKernelJitCreateSharedMemory` returns `EPERM` (`0x80020001`), automatically try GoldHEN's patched `mmap(NULL, size, PROT_READ|PROT_WRITE|PROT_EXEC, MAP_PRIVATE|MAP_ANON, -1, 0)` before falling back to interpreter.
