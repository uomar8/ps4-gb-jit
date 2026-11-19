#include "../hal.h"
#include "../../debug/dbg_log.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <orbis/libkernel.h>
#include <orbis/AudioOut.h>

#define PS4_AUDIO_RING_FRAMES 4096U
#define PS4_AUDIO_VOL_0DB     32768 /* ORBIS_AUDIO_VOLUME_0DB undeclared in v0.5.4 */
#define PS4_AUDIO_STACK_SIZE  (1024U * 1024U) /* 1 MiB explicit worker stack */

static int32_t       g_audio_port = -1;
static OrbisPthread  g_audio_thread = 0;
static volatile int  g_audio_running = 0;

static int16_t       g_ring[PS4_AUDIO_RING_FRAMES * 2];
static volatile uint32_t g_ring_r = 0;
static volatile uint32_t g_ring_w = 0;

static void *ps4_audio_thread_fn(void *arg) {
    (void)arg;
    int16_t grain[APP_AUDIO_GRAIN * 2];

    while (__atomic_load_n(&g_audio_running, __ATOMIC_ACQUIRE)) {
        uint32_t r = __atomic_load_n(&g_ring_r, __ATOMIC_RELAXED);
        uint32_t w = __atomic_load_n(&g_ring_w, __ATOMIC_ACQUIRE);
        uint32_t avail = (w - r) & (PS4_AUDIO_RING_FRAMES - 1U);

        if (avail >= APP_AUDIO_GRAIN) {
            for (uint32_t i = 0; i < APP_AUDIO_GRAIN; i++) {
                uint32_t idx = (r + i) & (PS4_AUDIO_RING_FRAMES - 1U);
                grain[i * 2 + 0] = g_ring[idx * 2 + 0];
                grain[i * 2 + 1] = g_ring[idx * 2 + 1];
            }
            __atomic_store_n(&g_ring_r, (r + APP_AUDIO_GRAIN) & (PS4_AUDIO_RING_FRAMES - 1U),
                             __ATOMIC_RELEASE);
        } else {
            memset(grain, 0, sizeof(grain));
        }

        if (g_audio_port >= 0) {
            (void)sceAudioOutOutput(g_audio_port, grain);
        } else {
            (void)sceKernelUsleep(10000);
        }
    }
    return NULL;
}

AppResult hal_audio_init(void) {
    /* Open ONE process-lifetime port (closing leaks handles until PORT_FULL). */
    if (g_audio_port >= 0) {
        return APP_OK;
    }

    int32_t rc = sceAudioOutInit();
    if (rc < 0 && (uint32_t)rc != 0x80260002U /* ALREADY_INIT */) {
        LOG_RC_ERROR("AUD", "sceAudioOutInit failed", rc);
        return APP_ERR_AUDIO_INIT;
    }

    g_audio_port = sceAudioOutOpen(0xFF /* SYSTEM */,
                                   ORBIS_AUDIO_OUT_PORT_TYPE_MAIN,
                                   0,
                                   APP_AUDIO_GRAIN,
                                   APP_AUDIO_RATE,
                                   ORBIS_AUDIO_OUT_PARAM_FORMAT_S16_STEREO);
    if (g_audio_port < 0) {
        LOG_RC_ERROR("AUD", "sceAudioOutOpen failed", g_audio_port);
        return APP_ERR_AUDIO_INIT;
    }

    int32_t vols[8] = {
        PS4_AUDIO_VOL_0DB, PS4_AUDIO_VOL_0DB, PS4_AUDIO_VOL_0DB, PS4_AUDIO_VOL_0DB,
        PS4_AUDIO_VOL_0DB, PS4_AUDIO_VOL_0DB, PS4_AUDIO_VOL_0DB, PS4_AUDIO_VOL_0DB
    };
    (void)sceAudioOutSetVolume(g_audio_port, 0x03 /* L|R */, vols);

    g_ring_r = 0;
    g_ring_w = 0;
    __atomic_store_n(&g_audio_running, 1, __ATOMIC_RELEASE);

    OrbisPthreadAttr attr;
    (void)scePthreadAttrInit(&attr);
    (void)scePthreadAttrSetstacksize(&attr, PS4_AUDIO_STACK_SIZE);
    rc = scePthreadCreate(&g_audio_thread, &attr, ps4_audio_thread_fn, NULL, "gbjit_audio");
    (void)scePthreadAttrDestroy(&attr);
    if (rc != 0) {
        __atomic_store_n(&g_audio_running, 0, __ATOMIC_RELEASE);
        LOG_RC_ERROR("AUD", "scePthreadCreate failed", rc);
        return APP_ERR_AUDIO_INIT;
    }

    LOG_INFO("AUD", "sceAudioOut ready port=%d rate=%d grain=%d stack=1MiB",
             g_audio_port, APP_AUDIO_RATE, APP_AUDIO_GRAIN);
    return APP_OK;
}

void hal_audio_shutdown(void) {
    if (__atomic_load_n(&g_audio_running, __ATOMIC_ACQUIRE)) {
        __atomic_store_n(&g_audio_running, 0, __ATOMIC_RELEASE);
        if (g_audio_thread != 0) {
            (void)scePthreadJoin(g_audio_thread, NULL);
            g_audio_thread = 0;
        }
    }
    /* Intentionally keep g_audio_port open for process lifetime until LoadExec("exit"). */
}

void hal_audio_push_samples(const int16_t *stereo_frames, size_t frame_count) {
    if (!stereo_frames || frame_count == 0) {
        return;
    }
    uint32_t w = __atomic_load_n(&g_ring_w, __ATOMIC_RELAXED);
    uint32_t r = __atomic_load_n(&g_ring_r, __ATOMIC_ACQUIRE);

    for (size_t i = 0; i < frame_count; i++) {
        uint32_t next_w = (w + 1U) & (PS4_AUDIO_RING_FRAMES - 1U);
        if (next_w == r) {
            break; /* ring full */
        }
        g_ring[w * 2 + 0] = stereo_frames[i * 2 + 0];
        g_ring[w * 2 + 1] = stereo_frames[i * 2 + 1];
        w = next_w;
    }
    __atomic_store_n(&g_ring_w, w, __ATOMIC_RELEASE);
}

uint32_t hal_audio_get_fill_percent(void) {
    uint32_t r = __atomic_load_n(&g_ring_r, __ATOMIC_ACQUIRE);
    uint32_t w = __atomic_load_n(&g_ring_w, __ATOMIC_ACQUIRE);
    uint32_t used = (w - r) & (PS4_AUDIO_RING_FRAMES - 1U);
    return (used * 100U) / PS4_AUDIO_RING_FRAMES;
}
