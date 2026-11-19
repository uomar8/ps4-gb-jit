#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmsystem.h>
#include "../hal.h"

#include <string.h>

#define WIN_AUDIO_BUFS 4

static HWAVEOUT g_wave = NULL;
static WAVEHDR  g_hdrs[WIN_AUDIO_BUFS];
static int16_t  g_bufs[WIN_AUDIO_BUFS][APP_AUDIO_GRAIN * 2];
static int      g_cur_buf = 0;

AppResult hal_audio_init(void) {
    WAVEFORMATEX wf;
    memset(&wf, 0, sizeof(wf));
    wf.wFormatTag = WAVE_FORMAT_PCM;
    wf.nChannels = 2;
    wf.nSamplesPerSec = APP_AUDIO_RATE;
    wf.wBitsPerSample = 16;
    wf.nBlockAlign = 4;
    wf.nAvgBytesPerSec = APP_AUDIO_RATE * 4;

    if (waveOutOpen(&g_wave, WAVE_MAPPER, &wf, 0, 0, CALLBACK_NULL) != MMSYSERR_NOERROR) {
        g_wave = NULL;
        return APP_OK; /* Allow headless Windows CI/VMs without audio device */
    }
    memset(g_hdrs, 0, sizeof(g_hdrs));
    memset(g_bufs, 0, sizeof(g_bufs));
    for (int i = 0; i < WIN_AUDIO_BUFS; i++) {
        g_hdrs[i].lpData = (LPSTR)g_bufs[i];
        g_hdrs[i].dwBufferLength = (DWORD)sizeof(g_bufs[i]);
        waveOutPrepareHeader(g_wave, &g_hdrs[i], sizeof(WAVEHDR));
    }
    return APP_OK;
}

void hal_audio_shutdown(void) {
    if (g_wave) {
        waveOutReset(g_wave);
        for (int i = 0; i < WIN_AUDIO_BUFS; i++) {
            waveOutUnprepareHeader(g_wave, &g_hdrs[i], sizeof(WAVEHDR));
        }
        waveOutClose(g_wave);
        g_wave = NULL;
    }
}

void hal_audio_push_samples(const int16_t *stereo_frames, size_t frame_count) {
    if (!g_wave || !stereo_frames || frame_count == 0) {
        return;
    }
    WAVEHDR *h = &g_hdrs[g_cur_buf];
    if (h->dwFlags & WHDR_INQUEUE) {
        return;
    }
    size_t n = frame_count > APP_AUDIO_GRAIN ? APP_AUDIO_GRAIN : frame_count;
    memcpy(g_bufs[g_cur_buf], stereo_frames, n * 2 * sizeof(int16_t));
    h->dwBufferLength = (DWORD)(n * 2 * sizeof(int16_t));
    waveOutWrite(g_wave, h, sizeof(WAVEHDR));
    g_cur_buf = (g_cur_buf + 1) % WIN_AUDIO_BUFS;
}

uint32_t hal_audio_get_fill_percent(void) {
    if (!g_wave) return 0;
    uint32_t queued = 0;
    for (int i = 0; i < WIN_AUDIO_BUFS; i++) {
        if (g_hdrs[i].dwFlags & WHDR_INQUEUE) {
            queued++;
        }
    }
    return (queued * 100U) / WIN_AUDIO_BUFS;
}
