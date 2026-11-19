cmake_minimum_required(VERSION 3.16)
project(ps4_gb_jit C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(COMMON_SRCS
    src/main.c
    src/core/app.c
    src/debug/dbg_sce_err.c
    src/debug/dbg_log.c
    src/debug/dbg_stub.c
    src/debug/dbg_crash.c
    src/debug/dbg_selftest.c
    src/ui/ui_render.c
    src/ui/ui_hud.c
)

if(WIN32)
    set(HAL_SRCS
        src/hal/win/hal_win_sys.c
        src/hal/win/hal_win_video.c
        src/hal/win/hal_win_audio.c
        src/hal/win/hal_win_input.c
        src/hal/win/hal_win_fs.c
        src/hal/win/hal_win_jit.c
    )
    add_executable(ps4-gb-jit-win64 ${COMMON_SRCS} ${HAL_SRCS})
    target_include_directories(ps4-gb-jit-win64 PRIVATE src)
    target_link_libraries(ps4-gb-jit-win64 PRIVATE user32 gdi32 winmm xinput)
    if(MSVC)
        target_compile_options(ps4-gb-jit-win64 PRIVATE /W4 /D_CRT_SECURE_NO_WARNINGS)
    else()
        target_compile_options(ps4-gb-jit-win64 PRIVATE -Wall -Wextra -Werror=vla -Werror=alloca)
    endif()
endif()
