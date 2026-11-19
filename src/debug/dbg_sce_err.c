#include "dbg_sce_err.h"
#include "../core/result.h"

const char *dbg_sce_strerror(int32_t rc) {
    switch ((uint32_t)rc) {
        case 0x00000000U: return "OK";
        /* AppResult codes */
        case (uint32_t)APP_ERR_INVALID_ARG:   return "APP_ERR_INVALID_ARG";
        case (uint32_t)APP_ERR_NOMEM:         return "APP_ERR_NOMEM";
        case (uint32_t)APP_ERR_IO:            return "APP_ERR_IO";
        case (uint32_t)APP_ERR_NOT_FOUND:     return "APP_ERR_NOT_FOUND";
        case (uint32_t)APP_ERR_UNSUPPORTED:   return "APP_ERR_UNSUPPORTED";
        case (uint32_t)APP_ERR_BAD_FORMAT:    return "APP_ERR_BAD_FORMAT";
        case (uint32_t)APP_ERR_JIT_UNAVAIL:   return "APP_ERR_JIT_UNAVAIL";
        case (uint32_t)APP_ERR_VIDEO_INIT:    return "APP_ERR_VIDEO_INIT";
        case (uint32_t)APP_ERR_AUDIO_INIT:    return "APP_ERR_AUDIO_INIT";
        case (uint32_t)APP_ERR_INPUT_INIT:    return "APP_ERR_INPUT_INIT";
        case (uint32_t)APP_ERR_SELFTEST_FAIL: return "APP_ERR_SELFTEST_FAIL";
        case (uint32_t)APP_ERR_INTERNAL:      return "APP_ERR_INTERNAL";
        /* Orbis kernel errors (0x8002xxxx) */
        case 0x80020001U: return "SCE_KERNEL_ERROR_EPERM";
        case 0x80020002U: return "SCE_KERNEL_ERROR_ENOENT";
        case 0x80020009U: return "SCE_KERNEL_ERROR_EBADF";
        case 0x8002000CU: return "SCE_KERNEL_ERROR_ENOMEM";
        case 0x8002000DU: return "SCE_KERNEL_ERROR_EACCES";
        case 0x8002000EU: return "SCE_KERNEL_ERROR_EFAULT";
        case 0x80020010U: return "SCE_KERNEL_ERROR_EBUSY";
        case 0x80020011U: return "SCE_KERNEL_ERROR_EEXIST";
        case 0x80020016U: return "SCE_KERNEL_ERROR_EINVAL";
        case 0x80020023U: return "SCE_KERNEL_ERROR_EAGAIN";
        case 0x8002004EU: return "SCE_KERNEL_ERROR_ENOSYS";
        /* VideoOut errors (0x8029xxxx) */
        case 0x80290001U: return "SCE_VIDEO_OUT_ERROR_INVALID_VALUE";
        case 0x80290002U: return "SCE_VIDEO_OUT_ERROR_INVALID_ADDRESS";
        case 0x80290005U: return "SCE_VIDEO_OUT_ERROR_NOMEM";
        case 0x80290013U: return "SCE_VIDEO_OUT_ERROR_INVALID_MEMORY";
        /* AudioOut errors (0x8026xxxx) */
        case 0x80260004U: return "SCE_AUDIO_OUT_ERROR_PORT_FULL";
        case 0x80260008U: return "SCE_AUDIO_OUT_ERROR_INVALID_SAMPLE_RATE";
        case 0x80260009U: return "SCE_AUDIO_OUT_ERROR_INVALID_FORMAT";
        /* Pad / UserService errors */
        case 0x80920001U: return "SCE_PAD_ERROR_INVALID_ARG";
        case 0x80920004U: return "SCE_PAD_ERROR_NOT_INITIALIZED";
        case 0x80960001U: return "SCE_USER_SERVICE_ERROR_NOT_INITIALIZED";
        default:          return "UNKNOWN_ERROR";
    }
}
