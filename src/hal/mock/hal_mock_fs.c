#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE
#include "../hal.h"
#include "hal_mock_ctl.h"
#include "../../debug/dbg_log.h"

#include <dirent.h>
#undef d_fileno
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static char g_data_dir[APP_MAX_PATH] = "/tmp/ps4_gb_jit_data";
static char g_app_dir[APP_MAX_PATH]  = "/tmp/ps4_gb_jit_app0";
static bool g_inject_ghosts = false;

void hal_mock_fs_set_data_dir(const char *path) {
    if (path) {
        (void)snprintf(g_data_dir, sizeof(g_data_dir), "%s", path);
    }
}

void hal_mock_fs_set_app_dir(const char *path) {
    if (path) {
        (void)snprintf(g_app_dir, sizeof(g_app_dir), "%s", path);
    }
}

void hal_mock_fs_set_inject_ghosts(bool enable) {
    g_inject_ghosts = enable;
}

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
    size_t len = strlen(tmp);
    if (len > 1 && tmp[len - 1] == '/') {
        tmp[len - 1] = '\0';
    }
    for (char *p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = '\0';
            if (mkdir(tmp, 0755) != 0 && errno != EEXIST) {
                return APP_ERR_IO;
            }
            *p = '/';
        }
    }
    if (mkdir(tmp, 0755) != 0 && errno != EEXIST) {
        return APP_ERR_IO;
    }
    return APP_OK;
}

AppResult hal_fs_stat_size(const char *abs_path, int64_t *out_size, bool *out_is_dir) {
    if (!abs_path) {
        return APP_ERR_INVALID_ARG;
    }
    struct stat st;
    if (stat(abs_path, &st) != 0) {
        return (errno == ENOENT) ? APP_ERR_NOT_FOUND : APP_ERR_IO;
    }
    if (out_size) *out_size = (int64_t)st.st_size;
    if (out_is_dir) *out_is_dir = S_ISDIR(st.st_mode);
    return APP_OK;
}

AppResult hal_fs_read_file(const char *abs_path, uint8_t **out_buf, size_t *out_size) {
    if (!abs_path || !out_buf || !out_size) {
        return APP_ERR_INVALID_ARG;
    }
    int fd = open(abs_path, O_RDONLY);
    if (fd < 0) {
        return (errno == ENOENT) ? APP_ERR_NOT_FOUND : APP_ERR_IO;
    }
    off_t end = lseek(fd, 0, SEEK_END);
    if (end < 0) {
        close(fd);
        return APP_ERR_IO;
    }
    (void)lseek(fd, 0, SEEK_SET);

    size_t sz = (size_t)end;
    uint8_t *buf = (uint8_t *)malloc(sz > 0 ? sz : 1);
    if (!buf) {
        close(fd);
        return APP_ERR_NOMEM;
    }
    size_t off = 0;
    while (off < sz) {
        ssize_t r = read(fd, buf + off, sz - off);
        if (r <= 0) {
            free(buf);
            close(fd);
            return APP_ERR_IO;
        }
        off += (size_t)r;
    }
    close(fd);
    *out_buf = buf;
    *out_size = sz;
    return APP_OK;
}

AppResult hal_fs_write_file(const char *abs_path, const uint8_t *buf, size_t size) {
    if (!abs_path || (!buf && size > 0)) {
        return APP_ERR_INVALID_ARG;
    }
    /* Simulate read-only /app0/ */
    size_t app_len = strlen(g_app_dir);
    if (app_len > 0 && strncmp(abs_path, g_app_dir, app_len) == 0) {
        return APP_ERR_IO;
    }
    int fd = open(abs_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        return APP_ERR_IO;
    }
    size_t off = 0;
    while (off < size) {
        ssize_t w = write(fd, buf + off, size - off);
        if (w <= 0) {
            close(fd);
            return APP_ERR_IO;
        }
        off += (size_t)w;
    }
    (void)fsync(fd);
    close(fd);
    return APP_OK;
}

AppResult hal_fs_unlink(const char *abs_path) {
    if (!abs_path) {
        return APP_ERR_INVALID_ARG;
    }
    return (unlink(abs_path) == 0) ? APP_OK : APP_ERR_IO;
}

AppResult hal_fs_list_dir(const char *abs_dir, HalDirEntry *out_entries,
                          size_t max_entries, size_t *out_count) {
    if (!abs_dir || !out_entries || !out_count) {
        return APP_ERR_INVALID_ARG;
    }
    *out_count = 0;
    DIR *d = opendir(abs_dir);
    if (!d) {
        return (errno == ENOENT) ? APP_ERR_NOT_FOUND : APP_ERR_IO;
    }

    /* Simulate UFS DT_WHT=14 / d_fileno==0 ghosts and verify our filter drops them */
    if (g_inject_ghosts) {
        OrbisDirentWire ghosts[2];
        memset(ghosts, 0, sizeof(ghosts));
        ghosts[0].d_fileno = 0;
        ghosts[0].d_type = 8;
        (void)snprintf(ghosts[0].d_name, sizeof(ghosts[0].d_name), "ghost_zero_ino.gb");
        ghosts[1].d_fileno = 999;
        ghosts[1].d_type = 14; /* DT_WHT */
        (void)snprintf(ghosts[1].d_name, sizeof(ghosts[1].d_name), "ghost_whiteout.gb");
        for (int g = 0; g < 2; g++) {
            if (ghosts[g].d_fileno == 0 || ghosts[g].d_type == 14) {
                continue; /* filtered out exactly like PS4 HAL */
            }
        }
    }

    struct dirent *ent;
    while ((ent = readdir(d)) != NULL && *out_count < max_entries) {
        if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0) {
            continue;
        }
        if (ent->d_ino == 0) {
            continue;
        }
#ifdef DT_WHT
        if (ent->d_type == DT_WHT) {
            continue;
        }
#endif
        char full[APP_MAX_PATH];
        (void)snprintf(full, sizeof(full), "%s/%s", abs_dir, ent->d_name);
        int64_t fsz = 0;
        bool is_dir = false;
        if (hal_fs_stat_size(full, &fsz, &is_dir) != APP_OK) {
            continue;
        }
        HalDirEntry *dst = &out_entries[*out_count];
        (void)snprintf(dst->name, sizeof(dst->name), "%s", ent->d_name);
        dst->is_dir = is_dir;
        dst->size = fsz > 0 ? (uint64_t)fsz : 0;
        (*out_count)++;
    }
    closedir(d);
    return APP_OK;
}
