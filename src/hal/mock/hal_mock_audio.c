#include "../hal.h"
#include "hal_mock_ctl.h"

static uint64_t g_total_frames = 0;

AppResult hal_audio_init(void) {
    g_total_frames = 0;
    return APP_OK;
}

void hal_audio_shutdown(void) {
}

void hal_audio_push_samples(const int16_t *stereo_frames, size_t frame_count) {
    (void)stereo_frames;
    g_total_frames += frame_count;
}

uint32_t hal_audio_get_fill_percent(void) {
    return 50;
}

uint64_t hal_mock_audio_total_frames(void) {
    return g_total_frames;
}
