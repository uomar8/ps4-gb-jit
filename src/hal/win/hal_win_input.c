#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <xinput.h>
#include "../hal.h"

extern bool hal_win_is_quit_requested(void);

AppResult hal_input_init(void) {
    return APP_OK;
}

void hal_input_shutdown(void) {
}

static bool key_down(int vk) {
    return (GetAsyncKeyState(vk) & 0x8000) != 0;
}

void hal_input_poll(HalInputState *state) {
    if (!state) {
        return;
    }
    state->prev_buttons = state->buttons;
    state->buttons = 0;
    state->lx = 0;
    state->ly = 0;
    state->quit_requested = hal_win_is_quit_requested();

    uint32_t out = 0;
    /* Keyboard mapping */
    if (key_down(VK_UP)    || key_down('W')) out |= HAL_BTN_UP;
    if (key_down(VK_DOWN)  || key_down('S')) out |= HAL_BTN_DOWN;
    if (key_down(VK_LEFT)  || key_down('A')) out |= HAL_BTN_LEFT;
    if (key_down(VK_RIGHT) || key_down('D')) out |= HAL_BTN_RIGHT;
    if (key_down('Z')      || key_down('J')) out |= HAL_BTN_CROSS;
    if (key_down('X')      || key_down('K')) out |= HAL_BTN_CIRCLE;
    if (key_down('C'))                       out |= HAL_BTN_SQUARE;
    if (key_down('R'))                       out |= HAL_BTN_TRIANGLE;
    if (key_down(VK_RETURN))                 out |= HAL_BTN_R1;       /* GB START */
    if (key_down(VK_TAB))                    out |= HAL_BTN_TOUCHPAD; /* GB SELECT */
    if (key_down(VK_ESCAPE))                 out |= HAL_BTN_OPTIONS;  /* Pause Modal */
    if (key_down(VK_SHIFT))                  out |= HAL_BTN_L2;       /* Fast-Forward */
    if (key_down(VK_F5))                     out |= HAL_BTN_R2;       /* Quick Save */
    if (key_down(VK_F7))                     out |= HAL_BTN_L1;       /* Quick Load */
    if (key_down(VK_F12))                    out |= HAL_BTN_HUD_KEY;  /* Live Debug HUD */

    /* Optional XInput gamepad */
    XINPUT_STATE xs;
    if (XInputGetState(0, &xs) == ERROR_SUCCESS) {
        WORD b = xs.Gamepad.wButtons;
        if (b & XINPUT_GAMEPAD_DPAD_UP)        out |= HAL_BTN_UP;
        if (b & XINPUT_GAMEPAD_DPAD_DOWN)      out |= HAL_BTN_DOWN;
        if (b & XINPUT_GAMEPAD_DPAD_LEFT)      out |= HAL_BTN_LEFT;
        if (b & XINPUT_GAMEPAD_DPAD_RIGHT)     out |= HAL_BTN_RIGHT;
        if (b & XINPUT_GAMEPAD_A)              out |= HAL_BTN_CROSS;
        if (b & XINPUT_GAMEPAD_B)              out |= HAL_BTN_CIRCLE;
        if (b & XINPUT_GAMEPAD_X)              out |= HAL_BTN_SQUARE;
        if (b & XINPUT_GAMEPAD_Y)              out |= HAL_BTN_TRIANGLE;
        if (b & XINPUT_GAMEPAD_LEFT_SHOULDER)  out |= HAL_BTN_L1;
        if (b & XINPUT_GAMEPAD_RIGHT_SHOULDER) out |= HAL_BTN_R1;
        if (b & XINPUT_GAMEPAD_LEFT_THUMB)     out |= HAL_BTN_L3;
        if (b & XINPUT_GAMEPAD_RIGHT_THUMB)    out |= HAL_BTN_R3;
        if (b & XINPUT_GAMEPAD_START)          out |= HAL_BTN_OPTIONS;
        if (b & XINPUT_GAMEPAD_BACK)           out |= HAL_BTN_TOUCHPAD;
        if (xs.Gamepad.bLeftTrigger > 128)     out |= HAL_BTN_L2;
        if (xs.Gamepad.bRightTrigger > 128)    out |= HAL_BTN_R2;
        state->lx = xs.Gamepad.sThumbLX;
        state->ly = (int16_t)(-xs.Gamepad.sThumbLY);
    }

    state->buttons = out;
}
