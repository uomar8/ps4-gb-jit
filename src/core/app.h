#ifndef PS4_GB_JIT_APP_H
#define PS4_GB_JIT_APP_H

#include "result.h"
#include "gb_types.h"

/* Run the application. If max_frames > 0, exit after max_frames (for headless CI). */
AppResult app_run(uint32_t max_frames);

#endif /* PS4_GB_JIT_APP_H */
