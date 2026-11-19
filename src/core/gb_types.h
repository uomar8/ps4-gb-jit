#ifndef PS4_GB_JIT_GB_TYPES_H
#define PS4_GB_JIT_GB_TYPES_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#define GB_SCREEN_W        160
#define GB_SCREEN_H        144
#define APP_FB_W           1920
#define APP_FB_H           1080
#define APP_AUDIO_RATE     48000
#define APP_AUDIO_GRAIN    512
#define APP_MAX_PATH       512

/*
 * Orbis kernel 120-byte stat structure.
 * OpenOrbis v0.5.4 musl shifts st_size to @80 (st_blocks); always read st_size at @72.
 */
typedef struct OrbisTimespecWire {
    int64_t tv_sec;
    int64_t tv_nsec;
} OrbisTimespecWire;

typedef struct OrbisStatWire {
    uint32_t          st_dev;      /* @0 */
    uint32_t          st_ino;      /* @4 */
    uint16_t          st_mode;     /* @8 */
    uint16_t          st_nlink;    /* @10 */
    uint32_t          st_uid;      /* @12 */
    uint32_t          st_gid;      /* @16 */
    uint32_t          st_rdev;     /* @20 */
    OrbisTimespecWire st_atim;     /* @24 */
    OrbisTimespecWire st_mtim;     /* @40 */
    OrbisTimespecWire st_ctim;     /* @56 */
    int64_t           st_size;     /* @72 */
    int64_t           st_blocks;   /* @80 */
    uint32_t          st_blksize;  /* @88 */
    uint32_t          st_flags;    /* @92 */
    uint32_t          st_gen;      /* @96 */
    int32_t           st_lspare;   /* @100 */
    OrbisTimespecWire st_birthtim; /* @104 */
} OrbisStatWire;

_Static_assert(sizeof(OrbisStatWire) == 120, "OrbisStatWire must be 120 bytes");
_Static_assert(offsetof(OrbisStatWire, st_size) == 72, "OrbisStatWire.st_size must be at offset 72");
_Static_assert(offsetof(OrbisStatWire, st_blocks) == 80, "OrbisStatWire.st_blocks must be at offset 80");

/* FreeBSD / Orbis kernel dirent wire layout for directory entry reads */
typedef struct OrbisDirentWire {
    uint32_t d_fileno;   /* @0 */
    uint16_t d_reclen;   /* @4 */
    uint8_t  d_type;     /* @6 (14 == DT_WHT whiteout ghost) */
    uint8_t  d_namlen;   /* @7 */
    char     d_name[256];/* @8 */
} OrbisDirentWire;

_Static_assert(offsetof(OrbisDirentWire, d_fileno) == 0, "OrbisDirentWire.d_fileno @0");
_Static_assert(offsetof(OrbisDirentWire, d_reclen) == 4, "OrbisDirentWire.d_reclen @4");
_Static_assert(offsetof(OrbisDirentWire, d_type) == 6, "OrbisDirentWire.d_type @6");
_Static_assert(offsetof(OrbisDirentWire, d_namlen) == 7, "OrbisDirentWire.d_namlen @7");
_Static_assert(offsetof(OrbisDirentWire, d_name) == 8, "OrbisDirentWire.d_name @8");

/* Save state wire header */
typedef struct GbSaveStateHeader {
    char     magic[4];      /* "GBJT" */
    uint32_t version;       /* 1 */
    uint32_t rom_crc32;
    uint32_t cpu_size;
    uint32_t ppu_size;
    uint32_t apu_size;
    uint32_t mem_size;
    uint32_t mbc_size;
    uint8_t  reserved[32];
} GbSaveStateHeader;

_Static_assert(sizeof(GbSaveStateHeader) == 64, "GbSaveStateHeader must be 64 bytes");

typedef struct AppConfig {
    bool     jit_enabled;
    bool     force_interp;
    uint32_t palette_index;
    uint32_t log_level;
    bool     swap_ab;
} AppConfig;

#endif /* PS4_GB_JIT_GB_TYPES_H */
