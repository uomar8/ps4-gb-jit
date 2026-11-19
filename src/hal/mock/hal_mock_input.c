#include "../hal.h"
#include "hal_mock_ctl.h"

#include <string.h>

#define MOCK_INPUT_QUEUE_CAP 4096

typedef struct MockInputFrame {
    uint32_t buttons;
    bool     quit;
} MockInputFrame;

static MockInputFrame g_queue[MOCK_INPUT_QUEUE_CAP];
static size_t         g_q_head = 0;
static size_t         g_q_tail = 0;

AppResult hal_input_init(void) {
    return APP_OK;
}

void hal_input_shutdown(void) {
}

void hal_mock_input_reset(void) {
    g_q_head = 0;
    g_q_tail = 0;
}

void hal_mock_input_push(uint32_t buttons, uint32_t repeat_frames) {
    for (uint32_t i = 0; i < repeat_frames && g_q_tail < MOCK_INPUT_QUEUE_CAP; i++) {
        g_queue[g_q_tail].buttons = buttons;
        g_queue[g_q_tail].quit = false;
        g_q_tail++;
    }
}

void hal_mock_input_push_quit(void) {
    if (g_q_tail < MOCK_INPUT_QUEUE_CAP) {
        g_queue[g_q_tail].buttons = 0;
        g_queue[g_q_tail].quit = true;
        g_q_tail++;
    }
}

void hal_input_poll(HalInputState *state) {
    if (!state) {
        return;
    }
    state->prev_buttons = state->buttons;
    state->lx = 0;
    state->ly = 0;
    if (g_q_head < g_q_tail) {
        state->buttons = g_queue[g_q_head].buttons;
        state->quit_requested = g_queue[g_q_head].quit;
        g_q_head++;
    } else {
        state->buttons = 0;
    }
}
