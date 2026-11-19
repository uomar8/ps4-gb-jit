#ifndef PS4_GB_JIT_HAL_H
#define PS4_GB_JIT_HAL_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "../core/result.h"
#include "../core/gb_types.h"

/* Logical button bitmask (platform-independent; never binds DS4 Share) */
typedef enum HalButtonMask {
    HAL_BTN_UP       = (1U << 0),
    HAL_BTN_DOWN     = (1U << 1),
    HAL_BTN_LEFT     = (1U << 2),
    HAL_BTN_RIGHT    = (1U << 3),
    HAL_BTN_CROSS    = (1U << 4),
    HAL_BTN_CIRCLE   = (1U << 5),
    HAL_BTN_SQUARE   = (1U << 6),
    HAL_BTN_TRIANGLE = (1U << 7),
    HAL_BTN_L1       = (1U << 8),
    HAL_BTN_R1       = (1U << 9),
    HAL_BTN_L2       = (1U << 10),
    HAL_BTN_R2       = (1U << 11),
    HAL_BTN_L3       = (1U << 12),
    HAL_BTN_R3       = (1U << 13),
    HAL_BTN_OPTIONS  = (1U << 14),
    HAL_BTN_TOUCHPAD = (1U << 15),
    HAL_BTN_HUD_KEY  = (1U << 16) /* F12 on PC; PS4 uses L3+R3 */
} HalButtonMask;

typedef struct HalInputState {
    uint32_t buttons;
    uint32_t prev_buttons;
    int16_t  lx;
    int16_t  ly;
    bool     quit_requested;
} HalInputState;

typedef struct HalJitArena {
    void  *rx_base;
    void  *rw_base;
    size_t size;
    int    rx_fd;
    int    rw_fd;
    void  *platform_handle;
} HalJitArena;

typedef struct HalDirEntry {
    char     name[256];
    bool     is_dir;
    uint64_t size;
} HalDirEntry;

/* System */
AppResult hal_sys_init(void);
void      hal_sys_shutdown(void);
void      hal_sys_hide_splash(void);
void      hal_sys_exit_clean(int code);
uint64_t  hal_sys_get_time_us(void);
void      hal_sys_sleep_us(uint32_t us);
void      hal_sys_klog_write(const char *line);
AppResult hal_sys_snapshot_klog(const char *dest_path);

/* Video (1920x1080 ARGB8888) */
AppResult hal_video_init(void);
void      hal_video_shutdown(void);
uint32_t *hal_video_get_backbuffer(int *out_width, int *out_height, int *out_pitch_px);
AppResult hal_video_present(void);

/* Audio (48000 Hz stereo S16, single process-lifetime port) */
AppResult hal_audio_init(void);
void      hal_audio_shutdown(void);
void      hal_audio_push_samples(const int16_t *stereo_frames, size_t frame_count);
uint32_t  hal_audio_get_fill_percent(void);

/* Input */
AppResult hal_input_init(void);
void      hal_input_shutdown(void);
void      hal_input_poll(HalInputState *state);

/* Filesystem (no getcwd/lstat/access; filters DT_WHT=14 & d_fileno==0) */
const char *hal_fs_get_data_dir(void);
const char *hal_fs_get_app_dir(void);
AppResult   hal_fs_mkdir_p(const char *abs_path);
AppResult   hal_fs_stat_size(const char *abs_path, int64_t *out_size, bool *out_is_dir);
AppResult   hal_fs_read_file(const char *abs_path, uint8_t **out_buf, size_t *out_size);
AppResult   hal_fs_write_file(const char *abs_path, const uint8_t *buf, size_t size);
AppResult   hal_fs_unlink(const char *abs_path);
AppResult   hal_fs_list_dir(const char *abs_dir, HalDirEntry *out_entries,
                            size_t max_entries, size_t *out_count);

/* JIT Dual-Mapped RW/RX Memory (16 KiB aligned) */
AppResult hal_jit_alloc(HalJitArena *arena, size_t size);
void      hal_jit_free(HalJitArena *arena);
void      hal_jit_flush_icache(void *rx_addr, size_t size);

#endif /* PS4_GB_JIT_HAL_H */
