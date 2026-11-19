#ifndef PS4_GB_JIT_HAL_MOCK_CTL_H
#define PS4_GB_JIT_HAL_MOCK_CTL_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

/* Mock-Orbis test control hooks (used by Tier 2 unit & E2E tests) */
void     hal_mock_fs_set_data_dir(const char *path);
void     hal_mock_fs_set_app_dir(const char *path);
void     hal_mock_fs_set_inject_ghosts(bool enable);
void     hal_mock_jit_set_fail_eacces(bool enable);

void     hal_mock_input_reset(void);
void     hal_mock_input_push(uint32_t buttons, uint32_t repeat_frames);
void     hal_mock_input_push_quit(void);

uint64_t hal_mock_video_checksum(void);
uint32_t hal_mock_video_present_count(void);
uint64_t hal_mock_audio_total_frames(void);

#endif /* PS4_GB_JIT_HAL_MOCK_CTL_H */
