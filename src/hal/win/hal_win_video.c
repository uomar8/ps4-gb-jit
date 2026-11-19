#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "../hal.h"
#include "../../debug/dbg_log.h"

#include <stdlib.h>
#include <string.h>

static HWND       g_hwnd = NULL;
static uint32_t  *g_fb = NULL;
static BITMAPINFO g_bmi;
static volatile bool g_win_quit = false;

bool hal_win_is_quit_requested(void) {
    return g_win_quit;
}

static LRESULT CALLBACK hal_win_wndproc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_CLOSE || msg == WM_DESTROY) {
        g_win_quit = true;
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcA(hwnd, msg, wParam, lParam);
}

AppResult hal_video_init(void) {
    g_fb = (uint32_t *)calloc((size_t)APP_FB_W * (size_t)APP_FB_H, sizeof(uint32_t));
    if (!g_fb) {
        return APP_ERR_NOMEM;
    }

    WNDCLASSA wc;
    memset(&wc, 0, sizeof(wc));
    wc.lpfnWndProc = hal_win_wndproc;
    wc.hInstance = GetModuleHandleA(NULL);
    wc.lpszClassName = "PS4GbJitWinClass";
    wc.hCursor = LoadCursorA(NULL, IDC_ARROW);
    RegisterClassA(&wc);

    /* Open a comfortable 1280x720 window that stretches the 1920x1080 ARGB8888 buffer */
    RECT rc = { 0, 0, 1280, 720 };
    AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);
    g_hwnd = CreateWindowExA(0, wc.lpszClassName, "PS4 GB-JIT (Windows HAL Tier 4)",
                             WS_OVERLAPPEDWINDOW | WS_VISIBLE,
                             CW_USEDEFAULT, CW_USEDEFAULT,
                             rc.right - rc.left, rc.bottom - rc.top,
                             NULL, NULL, wc.hInstance, NULL);
    if (!g_hwnd) {
        LOG_ERROR("VID", "CreateWindowExA failed");
        return APP_ERR_VIDEO_INIT;
    }

    memset(&g_bmi, 0, sizeof(g_bmi));
    g_bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    g_bmi.bmiHeader.biWidth = APP_FB_W;
    g_bmi.bmiHeader.biHeight = -APP_FB_H; /* top-down DIB */
    g_bmi.bmiHeader.biPlanes = 1;
    g_bmi.bmiHeader.biBitCount = 32;
    g_bmi.bmiHeader.biCompression = BI_RGB;

    return APP_OK;
}

void hal_video_shutdown(void) {
    if (g_hwnd) {
        DestroyWindow(g_hwnd);
        g_hwnd = NULL;
    }
    free(g_fb);
    g_fb = NULL;
}

uint32_t *hal_video_get_backbuffer(int *out_width, int *out_height, int *out_pitch_px) {
    if (out_width) *out_width = APP_FB_W;
    if (out_height) *out_height = APP_FB_H;
    if (out_pitch_px) *out_pitch_px = APP_FB_W;
    return g_fb;
}

AppResult hal_video_present(void) {
    if (!g_hwnd || !g_fb) {
        return APP_ERR_VIDEO_INIT;
    }
    MSG msg;
    while (PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE)) {
        if (msg.message == WM_QUIT) {
            g_win_quit = true;
        }
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }

    RECT client;
    GetClientRect(g_hwnd, &client);
    HDC hdc = GetDC(g_hwnd);
    if (hdc) {
        SetStretchBltMode(hdc, COLORONCOLOR);
        StretchDIBits(hdc,
                      0, 0, client.right - client.left, client.bottom - client.top,
                      0, 0, APP_FB_W, APP_FB_H,
                      g_fb, &g_bmi, DIB_RGB_COLORS, SRCCOPY);
        ReleaseDC(g_hwnd, hdc);
    }
    return APP_OK;
}
