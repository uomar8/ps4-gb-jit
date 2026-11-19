cmake_minimum_required(VERSION 3.16)
project(ps4_gb_jit C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(COMMON_SRCS
    src/main.c
    src/core/app.c
    src/core/gb_machine.c
    src/core/mem/cart.c
    src/core/mem/bus.c
    src/core/hle/timer.c
    src/core/hle/ppu.c
    src/core/hle/apu.c
    src/core/cpu/sm83_interp.c
    src/core/jit/jit_x64_emit.c
    src/core/jit/jit_engine.c
    src/core/jit/jit_compile.c
    src/debug/dbg_sce_err.c
    src/debug/dbg_log.c
    src/debug/dbg_stub.c
    src/debug/dbg_crash.c
    src/debug/dbg_selftest.c
    src/ui/ui_render.c
    src/ui/ui_hud.c
    src/ui/ui_state.c
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

    set(WIN_JIT_TEST_SRCS
        tests/unit/test_win_jit_selftest.c
        src/core/gb_machine.c
        src/core/mem/cart.c
        src/core/mem/bus.c
        src/core/hle/timer.c
        src/core/hle/ppu.c
        src/core/hle/apu.c
        src/core/cpu/sm83_interp.c
        src/core/jit/jit_x64_emit.c
        src/core/jit/jit_engine.c
        src/core/jit/jit_compile.c
        src/debug/dbg_sce_err.c
        src/debug/dbg_log.c
        src/debug/dbg_stub.c
        src/hal/win/hal_win_sys.c
        src/hal/win/hal_win_audio.c
        src/hal/win/hal_win_fs.c
        src/hal/win/hal_win_jit.c
    )
    add_executable(test_win_jit_selftest ${WIN_JIT_TEST_SRCS})
    target_include_directories(test_win_jit_selftest PRIVATE src)
    target_link_libraries(test_win_jit_selftest PRIVATE winmm)

    if(MSVC)
        target_compile_options(ps4-gb-jit-win64 PRIVATE /W4 /D_CRT_SECURE_NO_WARNINGS)
        target_compile_options(test_win_jit_selftest PRIVATE /W4 /D_CRT_SECURE_NO_WARNINGS)
    else()
        target_compile_options(ps4-gb-jit-win64 PRIVATE -Wall -Wextra -Werror=vla -Werror=alloca)
        target_compile_options(test_win_jit_selftest PRIVATE -Wall -Wextra -Werror=vla -Werror=alloca)
    endif()
endif()
