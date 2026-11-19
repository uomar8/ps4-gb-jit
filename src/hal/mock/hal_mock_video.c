#include "../hal.h"
#include "hal_mock_ctl.h"

#include <stdlib.h>
#include <string.h>

static uint32_t *g_fb = NULL;
static uint32_t  g_present_count = 0;
static uint64_t  g_last_checksum = 0;

static uint64_t compute_fb_checksum(const uint32_t *fb) {
    if (!fb) return 0;
    uint64_t hash = 1469598103934665603ULL;
    size_t count = (size_t)APP_FB_W * (size_t)APP_FB_H;
    for (size_t i = 0; i < count; i += 16) {
        hash ^= (uint64_t)fb[i];
        hash *= 1099511628211ULL;
    }
    return hash;
}

AppResult hal_video_init(void) {
    if (!g_fb) {
        g_fb = (uint32_t *)calloc((size_t)APP_FB_W * (size_t)APP_FB_H, sizeof(uint32_t));
        if (!g_fb) {
            return APP_ERR_NOMEM;
        }
    }
    g_present_count = 0;
    g_last_checksum = 0;
    return APP_OK;
}

void hal_video_shutdown(void) {
    free(g_fb);
    g_fb = NULL;
}

uint32_t *hal_video_get_backbuffer(int *out_width, int *out_height, int *out_pitch_px) {
    if (out_width) *out_width = APP_FB_W;
    if (out_height) *out_height = APP_FB_H;
    if (out_pitch_px) *out_pitch_px = APP_FB_W;
    return g_fb;
}

AppResult hal_video_present(void) {
    g_present_count++;
    g_last_checksum = compute_fb_checksum(g_fb);
    return APP_OK;
}

uint64_t hal_mock_video_checksum(void) {
    if (g_fb) {
        return compute_fb_checksum(g_fb);
    }
    return g_last_checksum;
}

uint32_t hal_mock_video_present_count(void) {
    return g_present_count;
}
