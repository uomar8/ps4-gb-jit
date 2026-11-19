#include "../hal.h"
#include "../../debug/dbg_log.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>

#include <orbis/libkernel.h>

/* FreeBSD/Orbis kernel open(2) flags — NEVER pass musl Linux O_CREAT/O_TRUNC/O_DIRECTORY to sceKernelOpen! */
#define PS4_O_RDONLY    0x0000
#define PS4_O_WRONLY    0x0001
#define PS4_O_CREAT     0x0200
#define PS4_O_TRUNC     0x0400
#define PS4_O_DIRECTORY 0x20000

const char *hal_fs_get_data_dir(void) {
    return "/data/gbjit";
}

const char *hal_fs_get_app_dir(void) {
    return "/app0";
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
            (void)sceKernelMkdir(tmp, 0777);
            *p = '/';
        }
    }
    int32_t rc = sceKernelMkdir(tmp, 0777);
    if (rc < 0 && (uint32_t)rc != 0x80020011U /* EEXIST */) {
        LOG_RC_ERROR("FS", "sceKernelMkdir failed", rc);
        return APP_ERR_IO;
    }
    return APP_OK;
}

AppResult hal_fs_stat_size(const char *abs_path, int64_t *out_size, bool *out_is_dir) {
    if (!abs_path) {
        return APP_ERR_INVALID_ARG;
    }
    OrbisStatWire st;
    memset(&st, 0, sizeof(st));
    int32_t rc = sceKernelStat(abs_path, (OrbisKernelStat *)&st);
    if (rc < 0) {
        return ((uint32_t)rc == 0x80020002U) ? APP_ERR_NOT_FOUND : APP_ERR_IO;
    }
    /* Read st_size at exact Orbis offset 72 via OrbisStatWire */
    if (out_size) *out_size = st.st_size;
    if (out_is_dir) *out_is_dir = S_ISDIR(st.st_mode);
    return APP_OK;
}

AppResult hal_fs_read_file(const char *abs_path, uint8_t **out_buf, size_t *out_size) {
    if (!abs_path || !out_buf || !out_size) {
        return APP_ERR_INVALID_ARG;
    }
    int32_t fd = sceKernelOpen(abs_path, PS4_O_RDONLY, 0);
    if (fd < 0) {
        return ((uint32_t)fd == 0x80020002U) ? APP_ERR_NOT_FOUND : APP_ERR_IO;
    }
    int64_t end = sceKernelLseek(fd, 0, SEEK_END);
    if (end < 0) {
        (void)sceKernelClose(fd);
        return APP_ERR_IO;
    }
    (void)sceKernelLseek(fd, 0, SEEK_SET);

    size_t sz = (size_t)end;
    uint8_t *buf = (uint8_t *)malloc(sz > 0 ? sz : 1);
    if (!buf) {
        (void)sceKernelClose(fd);
        return APP_ERR_NOMEM;
    }

    size_t off = 0;
    while (off < sz) {
        ssize_t r = sceKernelRead(fd, buf + off, sz - off);
        if (r <= 0) {
            free(buf);
            (void)sceKernelClose(fd);
            return APP_ERR_IO;
        }
        off += (size_t)r;
    }
    (void)sceKernelClose(fd);
    *out_buf = buf;
    *out_size = sz;
    return APP_OK;
}

AppResult hal_fs_write_file(const char *abs_path, const uint8_t *buf, size_t size) {
    if (!abs_path || (!buf && size > 0)) {
        return APP_ERR_INVALID_ARG;
    }
    int32_t fd = sceKernelOpen(abs_path, PS4_O_WRONLY | PS4_O_CREAT | PS4_O_TRUNC, 0666);
    if (fd < 0) {
        LOG_RC_ERROR("FS", "sceKernelOpen(write) failed", fd);
        return APP_ERR_IO;
    }
    size_t off = 0;
    while (off < size) {
        ssize_t w = sceKernelWrite(fd, buf + off, size - off);
        if (w <= 0) {
            (void)sceKernelClose(fd);
            return APP_ERR_IO;
        }
        off += (size_t)w;
    }
    (void)sceKernelFsync(fd);
    (void)sceKernelClose(fd);
    return APP_OK;
}

AppResult hal_fs_unlink(const char *abs_path) {
    if (!abs_path) {
        return APP_ERR_INVALID_ARG;
    }
    int32_t rc = sceKernelUnlink(abs_path);
    return (rc == 0) ? APP_OK : APP_ERR_IO;
}

AppResult hal_fs_list_dir(const char *abs_dir, HalDirEntry *out_entries,
                          size_t max_entries, size_t *out_count) {
    if (!abs_dir || !out_entries || !out_count) {
        return APP_ERR_INVALID_ARG;
    }
    *out_count = 0;
    int32_t fd = sceKernelOpen(abs_dir, PS4_O_RDONLY | PS4_O_DIRECTORY, 0);
    if (fd < 0) {
        return ((uint32_t)fd == 0x80020002U) ? APP_ERR_NOT_FOUND : APP_ERR_IO;
    }

    uint8_t *buf = (uint8_t *)malloc(8192);
    if (!buf) {
        (void)sceKernelClose(fd);
        return APP_ERR_NOMEM;
    }

    for (;;) {
        int32_t nread = sceKernelGetdents(fd, (char *)buf, 8192);
        if (nread <= 0) {
            break;
        }
        int32_t pos = 0;
        while (pos < nread && *out_count < max_entries) {
            const OrbisDirentWire *ent = (const OrbisDirentWire *)(buf + pos);
            if (ent->d_reclen == 0) {
                break;
            }
            pos += ent->d_reclen;

            /* Always skip deleted UFS slots (d_fileno==0) and whiteouts (DT_WHT==14) */
            if (ent->d_fileno == 0 || ent->d_type == 14) {
                continue;
            }
            char name[256];
            size_t nlen = ent->d_namlen;
            if (nlen >= sizeof(name)) {
                nlen = sizeof(name) - 1;
            }
            memcpy(name, ent->d_name, nlen);
            name[nlen] = '\0';
            if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) {
                continue;
            }

            char full[APP_MAX_PATH];
            (void)snprintf(full, sizeof(full), "%s/%s", abs_dir, name);
            int64_t fsz = 0;
            bool is_dir = false;
            if (hal_fs_stat_size(full, &fsz, &is_dir) != APP_OK) {
                continue;
            }

            HalDirEntry *dst = &out_entries[*out_count];
            (void)snprintf(dst->name, sizeof(dst->name), "%s", name);
            dst->is_dir = is_dir;
            dst->size = fsz > 0 ? (uint64_t)fsz : 0;
            (*out_count)++;
        }
    }

    free(buf);
    (void)sceKernelClose(fd);
    return APP_OK;
}
