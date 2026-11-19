#include "../hal.h"
#include "../../debug/dbg_log.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <orbis/libkernel.h>
#include <orbis/VideoOut.h>

#define PS4_FB_COUNT       2
#define PS4_FB_SIZE_ALIGN  0xA00000ULL /* 10 MiB >= 1920*1080*4 (0x7E9000), multiple of 2 MiB */
#define PS4_DMEM_TOTAL     (PS4_FB_SIZE_ALIGN * PS4_FB_COUNT)
#define PS4_PIXEL_ARGB8888 0x80000000U /* ORBIS_VIDEO_OUT_PIXEL_FORMAT_A8R8G8B8_SRGB */

static int32_t  g_video_handle = -1;
static int64_t  g_dmem_phys = 0;
static void    *g_dmem_addr = NULL;
static void    *g_fb_ptrs[PS4_FB_COUNT] = { NULL, NULL };
static int      g_back_idx = 0;
static int64_t  g_flip_arg = 1;

AppResult hal_video_init(void) {
    LOG_INFO("VID", "step 1: sceVideoOutOpen");
    g_video_handle = sceVideoOutOpen(0xFF /* SYSTEM */, ORBIS_VIDEO_OUT_BUS_MAIN, 0, NULL);
    if (g_video_handle < 0) {
        LOG_RC_ERROR("VID", "sceVideoOutOpen failed", g_video_handle);
        return APP_ERR_VIDEO_INIT;
    }

    size_t dmem_max = sceKernelGetDirectMemorySize();
    LOG_INFO("VID", "step 2: alloc dmem handle=%d max=0x%zx req=0x%llx",
             g_video_handle, dmem_max, (unsigned long long)PS4_DMEM_TOTAL);
    int32_t rc = sceKernelAllocateDirectMemory(0, (off_t)dmem_max, PS4_DMEM_TOTAL,
                                               HAL_DMEM_ALIGN, 3 /* WB_ONION */, &g_dmem_phys);
    if (rc < 0) {
        LOG_RC_ERROR("VID", "sceKernelAllocateDirectMemory failed", rc);
        return APP_ERR_VIDEO_INIT;
    }

    LOG_INFO("VID", "step 3: map dmem phys=0x%llx prot=0x33", (unsigned long long)g_dmem_phys);
    /* prot = 0x33 (CPU R/W | GPU R/W) required by sceVideoOutRegisterBuffers */
    rc = sceKernelMapDirectMemory(&g_dmem_addr, PS4_DMEM_TOTAL, 0x33, 0, g_dmem_phys, HAL_DMEM_ALIGN);
    if (rc < 0 || !g_dmem_addr) {
        LOG_RC_ERROR("VID", "sceKernelMapDirectMemory failed", rc);
        return APP_ERR_VIDEO_INIT;
    }

    LOG_INFO("VID", "step 4: clear dmem=%p and register buffers", g_dmem_addr);
    memset(g_dmem_addr, 0, PS4_DMEM_TOTAL);
    for (int i = 0; i < PS4_FB_COUNT; i++) {
        g_fb_ptrs[i] = (uint8_t *)g_dmem_addr + (size_t)i * PS4_FB_SIZE_ALIGN;
    }

    OrbisVideoOutBufferAttribute attr;
    memset(&attr, 0, sizeof(attr));
    sceVideoOutSetBufferAttribute(&attr, PS4_PIXEL_ARGB8888,
                                  ORBIS_VIDEO_OUT_TILING_MODE_LINEAR,
                                  ORBIS_VIDEO_OUT_ASPECT_RATIO_16_9,
                                  APP_FB_W, APP_FB_H, APP_FB_W);

    rc = sceVideoOutRegisterBuffers(g_video_handle, 0, g_fb_ptrs, PS4_FB_COUNT, &attr);
    if (rc < 0) {
        LOG_RC_ERROR("VID", "sceVideoOutRegisterBuffers failed", rc);
        return APP_ERR_VIDEO_INIT;
    }

    (void)sceVideoOutSetFlipRate(g_video_handle, 0);
    g_back_idx = 0;
    LOG_INFO("VID", "sceVideoOut ready handle=%d dmem=%p total=0x%llx",
             g_video_handle, g_dmem_addr, (unsigned long long)PS4_DMEM_TOTAL);
    return APP_OK;
}

void hal_video_shutdown(void) {
    if (g_video_handle >= 0) {
        (void)sceVideoOutClose(g_video_handle);
        g_video_handle = -1;
    }
    if (g_dmem_addr) {
        (void)sceKernelMunmap(g_dmem_addr, PS4_DMEM_TOTAL);
        g_dmem_addr = NULL;
    }
    if (g_dmem_phys != 0) {
        (void)sceKernelReleaseDirectMemory(g_dmem_phys, PS4_DMEM_TOTAL);
        g_dmem_phys = 0;
    }
}

uint32_t *hal_video_get_backbuffer(int *out_width, int *out_height, int *out_pitch_px) {
    if (out_width) *out_width = APP_FB_W;
    if (out_height) *out_height = APP_FB_H;
    if (out_pitch_px) *out_pitch_px = APP_FB_W;
    return (uint32_t *)g_fb_ptrs[g_back_idx];
}

AppResult hal_video_present(void) {
    if (g_video_handle < 0) {
        return APP_ERR_VIDEO_INIT;
    }
    int32_t rc = sceVideoOutSubmitFlip(g_video_handle, g_back_idx,
                                       ORBIS_VIDEO_OUT_FLIP_VSYNC, g_flip_arg);
    if (rc < 0) {
        LOG_RC_ERROR("VID", "sceVideoOutSubmitFlip failed", rc);
        return APP_ERR_VIDEO_INIT;
    }

    OrbisVideoOutFlipStatus st;
    for (int spins = 0; spins < 1000; spins++) {
        memset(&st, 0, sizeof(st));
        if (sceVideoOutGetFlipStatus(g_video_handle, &st) == 0 && st.flipArg >= g_flip_arg) {
            break;
        }
        (void)sceKernelUsleep(500);
    }

    g_flip_arg++;
    g_back_idx = (g_back_idx + 1) % PS4_FB_COUNT;
    return APP_OK;
}
