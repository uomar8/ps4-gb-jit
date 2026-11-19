#ifndef PS4_GB_JIT_RESULT_H
#define PS4_GB_JIT_RESULT_H

#include <stdint.h>

#define HAL_PAGE_SIZE 16384U
#define HAL_DMEM_ALIGN 0x200000ULL /* 2 MiB DirectMemory alignment */

typedef enum AppResult {
    APP_OK                 =  0,
    APP_ERR_INVALID_ARG    = -1,
    APP_ERR_NOMEM          = -2,
    APP_ERR_IO             = -3,
    APP_ERR_NOT_FOUND      = -4,
    APP_ERR_UNSUPPORTED    = -5,
    APP_ERR_BAD_FORMAT     = -6,
    APP_ERR_JIT_UNAVAIL    = -7,
    APP_ERR_VIDEO_INIT     = -8,
    APP_ERR_AUDIO_INIT     = -9,
    APP_ERR_INPUT_INIT     = -10,
    APP_ERR_SELFTEST_FAIL  = -11,
    APP_ERR_INTERNAL       = -12
} AppResult;

#endif /* PS4_GB_JIT_RESULT_H */
