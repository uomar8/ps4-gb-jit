#include "../hal.h"
#include "../../debug/dbg_log.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <orbis/UserService.h>
#include <orbis/Pad.h>

static int32_t g_user_id = -1;
static int32_t g_pad_handle = -1;

AppResult hal_input_init(void) {
    LOG_INFO("INP", "step 1: sceUserServiceInitialize");
    OrbisUserServiceInitializeParams params;
    memset(&params, 0, sizeof(params));
    params.priority = 700; /* ORBIS_KERNEL_PRIO_FIFO_DEFAULT */
    int32_t rc = sceUserServiceInitialize(&params);
    if (rc < 0 && (uint32_t)rc != 0x80960003U /* ALREADY_INITIALIZED */) {
        LOG_RC_ERROR("INP", "sceUserServiceInitialize failed", rc);
        return APP_ERR_INPUT_INIT;
    }

    LOG_INFO("INP", "step 2: sceUserServiceGetInitialUser");
    rc = sceUserServiceGetInitialUser(&g_user_id);
    if (rc < 0) {
        LOG_RC_ERROR("INP", "sceUserServiceGetInitialUser failed", rc);
        return APP_ERR_INPUT_INIT;
    }

    LOG_INFO("INP", "step 3: scePadInit + scePadOpen user_id=%d", g_user_id);
    rc = scePadInit();
    if (rc < 0) {
        LOG_RC_ERROR("INP", "scePadInit failed", rc);
        return APP_ERR_INPUT_INIT;
    }

    g_pad_handle = scePadOpen(g_user_id, 0, 0, NULL);
    if (g_pad_handle < 0) {
        LOG_RC_ERROR("INP", "scePadOpen failed", g_pad_handle);
        return APP_ERR_INPUT_INIT;
    }

    LOG_INFO("INP", "scePad ready user_id=%d handle=%d", g_user_id, g_pad_handle);
    return APP_OK;
}

void hal_input_shutdown(void) {
    if (g_pad_handle >= 0) {
        (void)scePadClose(g_pad_handle);
        g_pad_handle = -1;
    }
}

void hal_input_poll(HalInputState *state) {
    if (!state) {
        return;
    }
    state->prev_buttons = state->buttons;
    state->buttons = 0;
    state->lx = 0;
    state->ly = 0;

    if (g_pad_handle < 0) {
        return;
    }

    OrbisPadData data;
    memset(&data, 0, sizeof(data));
    if (scePadReadState(g_pad_handle, &data) != 0) {
        return;
    }

    uint32_t b = data.buttons;
    uint32_t out = 0;

    /* Note: Never bind DS4 Share — intercepted by Orbis OS. */
    if (b & ORBIS_PAD_BUTTON_UP)        out |= HAL_BTN_UP;
    if (b & ORBIS_PAD_BUTTON_DOWN)      out |= HAL_BTN_DOWN;
    if (b & ORBIS_PAD_BUTTON_LEFT)      out |= HAL_BTN_LEFT;
    if (b & ORBIS_PAD_BUTTON_RIGHT)     out |= HAL_BTN_RIGHT;
    if (b & ORBIS_PAD_BUTTON_CROSS)     out |= HAL_BTN_CROSS;
    if (b & ORBIS_PAD_BUTTON_CIRCLE)    out |= HAL_BTN_CIRCLE;
    if (b & ORBIS_PAD_BUTTON_SQUARE)    out |= HAL_BTN_SQUARE;
    if (b & ORBIS_PAD_BUTTON_TRIANGLE)  out |= HAL_BTN_TRIANGLE;
    if (b & ORBIS_PAD_BUTTON_L1)        out |= HAL_BTN_L1;
    if (b & ORBIS_PAD_BUTTON_R1)        out |= HAL_BTN_R1;
    if (b & ORBIS_PAD_BUTTON_L2)        out |= HAL_BTN_L2;
    if (b & ORBIS_PAD_BUTTON_R2)        out |= HAL_BTN_R2;
    if (b & ORBIS_PAD_BUTTON_L3)        out |= HAL_BTN_L3;
    if (b & ORBIS_PAD_BUTTON_R3)        out |= HAL_BTN_R3;
    if (b & ORBIS_PAD_BUTTON_OPTIONS)   out |= HAL_BTN_OPTIONS;
    if (b & ORBIS_PAD_BUTTON_TOUCH_PAD) out |= HAL_BTN_TOUCHPAD;

    state->buttons = out;
    state->lx = (int16_t)(((int32_t)data.leftStick.x - 128) * 256);
    state->ly = (int16_t)(((int32_t)data.leftStick.y - 128) * 256);
}
