# PS4 Game Boy (DMG-01) JIT + HLE (ps4-gb-jit)
APP_TITLE  := PS4 GB-JIT
TITLE_ID   := GBJT00001
CONTENT_ID := IV0000-GBJT00001_00-PS4GBJIT00000001
APP_VER    := 01.04
PKG_NAME   := PS4-GB-JIT-v$(APP_VER).pkg

# JIT-capable authinfo (0xE0 bit set at capability byte 0x47 -> 0xF0)
PS4_PAID     := 0x3800000000000011
PS4_AUTHINFO := 000000000000000000000000001C004000FF000000000080000000000000000000000000000000000000008000400040000000000000008000000000000000080040FFFF000000F000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000

COMMON_SRCS := \
	src/core/app.c \
	src/core/gb_machine.c \
	src/core/mem/cart.c \
	src/core/mem/bus.c \
	src/core/hle/timer.c \
	src/core/hle/ppu.c \
	src/core/hle/apu.c \
	src/core/cpu/sm83_interp.c \
	src/core/jit/jit_x64_emit.c \
	src/core/jit/jit_engine.c \
	src/core/jit/jit_compile.c \
	src/debug/dbg_sce_err.c \
	src/debug/dbg_log.c \
	src/debug/dbg_stub.c \
	src/debug/dbg_crash.c \
	src/debug/dbg_selftest.c \
	src/ui/ui_render.c \
	src/ui/ui_hud.c \
	src/ui/ui_state.c

MOCK_SRCS := \
	src/hal/mock/hal_mock_sys.c \
	src/hal/mock/hal_mock_video.c \
	src/hal/mock/hal_mock_audio.c \
	src/hal/mock/hal_mock_input.c \
	src/hal/mock/hal_mock_fs.c \
	src/hal/mock/hal_mock_jit.c

PS4_SRCS := \
	src/main.c \
	src/hal/ps4/hal_ps4_sys.c \
	src/hal/ps4/hal_ps4_video.c \
	src/hal/ps4/hal_ps4_audio.c \
	src/hal/ps4/hal_ps4_input.c \
	src/hal/ps4/hal_ps4_fs.c \
	src/hal/ps4/hal_ps4_jit.c

# Host / CI (Linux x64) settings
HOST_CC      ?= gcc
HOST_CFLAGS  := -std=c11 -O1 -g -Wall -Wextra -Werror=vla -Werror=alloca -march=x86-64 -Isrc
SAN_FLAGS    := -fsanitize=address,undefined -fno-omit-frame-pointer

# OpenOrbis v0.5.4 PS4 cross-compile settings
PS4_CC       := clang
PS4_LD       := ld.lld
PS4_BUILD    := build/ps4
PKG_STAGE    := build/pkg_stage
PS4_CFLAGS   := --target=x86_64-pc-freebsd12-elf -std=c11 -O3 -march=btver2 -mtune=btver2 \
                -fPIC -funwind-tables -fstack-usage -c \
                -Wall -Wextra -Werror=vla -Werror=alloca \
                -D__ORBIS__ -D__PS4__ -D_BSD_SOURCE -D_POSIX_C_SOURCE=200809L \
                -Isrc -isysroot $(OO_PS4_TOOLCHAIN) -isystem $(OO_PS4_TOOLCHAIN)/include
PS4_LIBS     := -lc -lkernel -lScePosix -lSceSysmodule -lSceVideoOut -lSceAudioOut -lScePad -lSceUserService -lSceSystemService
PS4_LDFLAGS  := -m elf_x86_64 -pie --script $(OO_PS4_TOOLCHAIN)/link.x --eh-frame-hdr \
                -L$(OO_PS4_TOOLCHAIN)/lib $(PS4_LIBS) $(OO_PS4_TOOLCHAIN)/lib/crt1.o

PS4_OBJS     := $(patsubst %.c,$(PS4_BUILD)/%.o,$(COMMON_SRCS) $(PS4_SRCS))

.PHONY: all lint test host_run ps4 clean

all: lint test

lint:
	bash scripts/lint.sh

test:
	@mkdir -p build/test
	$(HOST_CC) $(HOST_CFLAGS) $(SAN_FLAGS) \
		tests/unit/test_phase1_harness.c $(COMMON_SRCS) $(MOCK_SRCS) \
		-o build/test/test_phase1_harness
	./build/test/test_phase1_harness
	$(HOST_CC) $(HOST_CFLAGS) $(SAN_FLAGS) \
		tests/unit/test_mbc_hle.c $(COMMON_SRCS) $(MOCK_SRCS) \
		-o build/test/test_mbc_hle
	./build/test/test_mbc_hle
	$(HOST_CC) $(HOST_CFLAGS) $(SAN_FLAGS) \
		tests/unit/test_sm83_jit_lockstep.c $(COMMON_SRCS) $(MOCK_SRCS) \
		-o build/test/test_sm83_jit_lockstep
	./build/test/test_sm83_jit_lockstep
	$(HOST_CC) $(HOST_CFLAGS) $(SAN_FLAGS) \
		tests/e2e/test_e2e_walkthrough.c $(COMMON_SRCS) $(MOCK_SRCS) \
		-o build/test/test_e2e_walkthrough
	./build/test/test_e2e_walkthrough

host_run:
	@mkdir -p build/host
	$(HOST_CC) $(HOST_CFLAGS) src/main.c $(COMMON_SRCS) $(MOCK_SRCS) -o build/host/host_run

$(PS4_BUILD)/%.o: %.c
	@mkdir -p $(dir $@)
	$(PS4_CC) $(PS4_CFLAGS) $< -o $@

$(PS4_BUILD)/eboot.elf: $(PS4_OBJS)
	$(PS4_LD) $(PS4_OBJS) $(PS4_LDFLAGS) -o $@

ps4: $(PS4_BUILD)/eboot.elf
	@mkdir -p $(PKG_STAGE)/sce_sys $(PKG_STAGE)/sce_module
	$(OO_PS4_TOOLCHAIN)/bin/linux/create-fself \
		-in=$(PS4_BUILD)/eboot.elf \
		-out=$(PS4_BUILD)/eboot.oelf \
		--eboot=$(PKG_STAGE)/eboot.bin \
		--paid=$(PS4_PAID) \
		--authinfo=$(PS4_AUTHINFO)
	@if [ ! -f assets/icon0.png ] || [ ! -f assets/pic0.png ]; then python3 scripts/gen_assets.py; fi
	cp assets/icon0.png $(PKG_STAGE)/sce_sys/icon0.png
	cp assets/pic0.png  $(PKG_STAGE)/sce_sys/pic0.png
	@LIBC_PRX=$$(find "$(OO_PS4_TOOLCHAIN)" -name "libc.prx" | head -n 1); \
	FIOS_PRX=$$(find "$(OO_PS4_TOOLCHAIN)" -name "libSceFios2.prx" | head -n 1); \
	if [ -z "$$LIBC_PRX" ] || [ -z "$$FIOS_PRX" ]; then \
		echo "ERROR: Missing libc.prx or libSceFios2.prx in $(OO_PS4_TOOLCHAIN)"; \
		find "$(OO_PS4_TOOLCHAIN)" -name "*.prx" || true; \
		exit 1; \
	fi; \
	cp "$$LIBC_PRX" $(PKG_STAGE)/sce_module/libc.prx; \
	cp "$$FIOS_PRX" $(PKG_STAGE)/sce_module/libSceFios2.prx
	@PKGTOOL="$(OO_PS4_TOOLCHAIN)/bin/linux/PkgTool.Core"; \
	if [ ! -x "$$PKGTOOL" ]; then PKGTOOL="dotnet --roll-forward Major $(OO_PS4_TOOLCHAIN)/bin/windows/PkgTool.Core.dll"; fi; \
	$$PKGTOOL sfo_new $(PKG_STAGE)/sce_sys/param.sfo; \
	$$PKGTOOL sfo_setentry $(PKG_STAGE)/sce_sys/param.sfo APP_TYPE --type Integer --maxsize 4 --value 1; \
	$$PKGTOOL sfo_setentry $(PKG_STAGE)/sce_sys/param.sfo APP_VER --type Utf8 --maxsize 8 --value "$(APP_VER)"; \
	$$PKGTOOL sfo_setentry $(PKG_STAGE)/sce_sys/param.sfo ATTRIBUTE --type Integer --maxsize 4 --value 0; \
	$$PKGTOOL sfo_setentry $(PKG_STAGE)/sce_sys/param.sfo CATEGORY --type Utf8 --maxsize 4 --value "gd"; \
	$$PKGTOOL sfo_setentry $(PKG_STAGE)/sce_sys/param.sfo CONTENT_ID --type Utf8 --maxsize 48 --value "$(CONTENT_ID)"; \
	$$PKGTOOL sfo_setentry $(PKG_STAGE)/sce_sys/param.sfo DOWNLOAD_DATA_SIZE --type Integer --maxsize 4 --value 0; \
	$$PKGTOOL sfo_setentry $(PKG_STAGE)/sce_sys/param.sfo FORMAT --type Utf8 --maxsize 4 --value "obs"; \
	$$PKGTOOL sfo_setentry $(PKG_STAGE)/sce_sys/param.sfo PARENTAL_LEVEL --type Integer --maxsize 4 --value 1; \
	$$PKGTOOL sfo_setentry $(PKG_STAGE)/sce_sys/param.sfo SYSTEM_VER --type Integer --maxsize 4 --value 0; \
	$$PKGTOOL sfo_setentry $(PKG_STAGE)/sce_sys/param.sfo TITLE --type Utf8 --maxsize 128 --value "$(APP_TITLE)"; \
	$$PKGTOOL sfo_setentry $(PKG_STAGE)/sce_sys/param.sfo TITLE_ID --type Utf8 --maxsize 12 --value "$(TITLE_ID)"; \
	$$PKGTOOL sfo_setentry $(PKG_STAGE)/sce_sys/param.sfo VERSION --type Utf8 --maxsize 8 --value "$(APP_VER)"; \
	python3 scripts/make_gp4.py "$(CONTENT_ID)" $(PKG_STAGE)/pkg.gp4; \
	(cd $(PKG_STAGE) && $$PKGTOOL pkg_build pkg.gp4 .); \
	mv $(PKG_STAGE)/$(CONTENT_ID).pkg $(PKG_NAME)
	bash scripts/audit_ps4.sh $(PS4_BUILD)/eboot.elf $(PKG_NAME) $(PS4_BUILD)

clean:
	rm -rf build $(PKG_NAME)
