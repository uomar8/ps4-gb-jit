#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "../hal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char g_data_dir[APP_MAX_PATH] = "./data";
static char g_app_dir[APP_MAX_PATH]  = "./assets";

const char *hal_fs_get_data_dir(void) {
    return g_data_dir;
}

const char *hal_fs_get_app_dir(void) {
    return g_app_dir;
}

AppResult hal_fs_mkdir_p(const char *abs_path) {
    if (!abs_path || abs_path[0] == '\0') {
        return APP_ERR_INVALID_ARG;
    }
    char tmp[APP_MAX_PATH];
    (void)snprintf(tmp, sizeof(tmp), "%s", abs_path);
    for (char *p = tmp + 1; *p; p++) {
        if (*p == '/' || *p == '\\') {
            char saved = *p;
            *p = '\0';
            (void)CreateDirectoryA(tmp, NULL);
            *p = saved;
        }
    }
    if (!CreateDirectoryA(tmp, NULL)) {
        DWORD err = GetLastError();
        if (err != ERROR_ALREADY_EXISTS) {
            return APP_ERR_IO;
        }
    }
    return APP_OK;
}

AppResult hal_fs_stat_size(const char *abs_path, int64_t *out_size, bool *out_is_dir) {
    if (!abs_path) {
        return APP_ERR_INVALID_ARG;
    }
    WIN32_FILE_ATTRIBUTE_DATA fad;
    if (!GetFileAttributesExA(abs_path, GetFileExInfoStandard, &fad)) {
        return APP_ERR_NOT_FOUND;
    }
    bool is_dir = (fad.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
    LARGE_INTEGER li;
    li.HighPart = (LONG)fad.nFileSizeHigh;
    li.LowPart = fad.nFileSizeLow;
    if (out_size) *out_size = (int64_t)li.QuadPart;
    if (out_is_dir) *out_is_dir = is_dir;
    return APP_OK;
}

AppResult hal_fs_read_file(const char *abs_path, uint8_t **out_buf, size_t *out_size) {
    if (!abs_path || !out_buf || !out_size) {
        return APP_ERR_INVALID_ARG;
    }
    HANDLE h = CreateFileA(abs_path, GENERIC_READ, FILE_SHARE_READ, NULL,
                           OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) {
        return APP_ERR_NOT_FOUND;
    }
    LARGE_INTEGER sz;
    if (!GetFileSizeEx(h, &sz) || sz.QuadPart < 0) {
        CloseHandle(h);
        return APP_ERR_IO;
    }
    size_t len = (size_t)sz.QuadPart;
    uint8_t *buf = (uint8_t *)malloc(len > 0 ? len : 1);
    if (!buf) {
        CloseHandle(h);
        return APP_ERR_NOMEM;
    }
    DWORD got = 0;
    if (len > 0 && (!ReadFile(h, buf, (DWORD)len, &got, NULL) || got != (DWORD)len)) {
        free(buf);
        CloseHandle(h);
        return APP_ERR_IO;
    }
    CloseHandle(h);
    *out_buf = buf;
    *out_size = len;
    return APP_OK;
}

AppResult hal_fs_write_file(const char *abs_path, const uint8_t *buf, size_t size) {
    if (!abs_path || (!buf && size > 0)) {
        return APP_ERR_INVALID_ARG;
    }
    HANDLE h = CreateFileA(abs_path, GENERIC_WRITE, 0, NULL,
                           CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) {
        return APP_ERR_IO;
    }
    DWORD wrote = 0;
    if (size > 0 && (!WriteFile(h, buf, (DWORD)size, &wrote, NULL) || wrote != (DWORD)size)) {
        CloseHandle(h);
        return APP_ERR_IO;
    }
    FlushFileBuffers(h);
    CloseHandle(h);
    return APP_OK;
}

AppResult hal_fs_unlink(const char *abs_path) {
    if (!abs_path) {
        return APP_ERR_INVALID_ARG;
    }
    return DeleteFileA(abs_path) ? APP_OK : APP_ERR_IO;
}

AppResult hal_fs_list_dir(const char *abs_dir, HalDirEntry *out_entries,
                          size_t max_entries, size_t *out_count) {
    if (!abs_dir || !out_entries || !out_count) {
        return APP_ERR_INVALID_ARG;
    }
    *out_count = 0;
    char pattern[APP_MAX_PATH];
    (void)snprintf(pattern, sizeof(pattern), "%s\\*", abs_dir);
    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA(pattern, &fd);
    if (h == INVALID_HANDLE_VALUE) {
        return APP_ERR_NOT_FOUND;
    }
    do {
        if (strcmp(fd.cFileName, ".") == 0 || strcmp(fd.cFileName, "..") == 0) {
            continue;
        }
        if (*out_count >= max_entries) {
            break;
        }
        HalDirEntry *dst = &out_entries[*out_count];
        (void)snprintf(dst->name, sizeof(dst->name), "%s", fd.cFileName);
        dst->is_dir = (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
        dst->size = ((uint64_t)fd.nFileSizeHigh << 32) | (uint64_t)fd.nFileSizeLow;
        (*out_count)++;
    } while (FindNextFileA(h, &fd));
    FindClose(h);
    return APP_OK;
}
