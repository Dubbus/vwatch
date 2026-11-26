# vwatch SDK build rules. An app Makefile sets APP and APP_SRCS, then:
#     VWATCH_SDK ?= ../../sdk
#     include $(VWATCH_SDK)/sdk.mk
# Optional: USE_LVGL := 1 to build the UI with LVGL (see include/vw_lvgl.h).
# Targets: all (build/<APP>.elf), size, run (headless host), clean.

PORT    ?= qemu-mps2
CC      := arm-none-eabi-gcc
AR      := arm-none-eabi-ar
SIZE    := arm-none-eabi-size
CPU     := -mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16 -mfloat-abi=hard
MAKEFLAGS += -j8
.DEFAULT_GOAL := all

SDK_INC := -I$(VWATCH_SDK)/include -I$(VWATCH_SDK)/src
CFLAGS  += $(CPU) -O2 -g -std=c11 -Wall -Wextra -ffunction-sections -fdata-sections $(SDK_INC)
LDFLAGS += $(CPU) --specs=nano.specs --specs=nosys.specs -T $(VWATCH_SDK)/port/$(PORT)/mps2_an386.ld \
           -Wl,--gc-sections -Wl,--no-warn-rwx-segments -Wl,-Map=build/$(APP).map

# Always linked: runtime, port, startup.
CORE_SRCS := $(VWATCH_SDK)/src/runtime.c $(VWATCH_SDK)/port/$(PORT)/port.c $(VWATCH_SDK)/port/$(PORT)/startup.c
# Library members: only pulled in when the app uses them (vw_* drawing or LVGL),
# so an LVGL app carries no framebuffer and vice versa.
LIB_SRCS  := $(VWATCH_SDK)/src/gfx.c $(VWATCH_SDK)/src/font5x8.c

ifeq ($(USE_LVGL),1)
LVGL_DIR   := $(VWATCH_SDK)/third_party/lvgl
LVGL_BUILD := $(VWATCH_SDK)/build/lvgl-$(PORT)
LVGL_INC   := -DLV_CONF_INCLUDE_SIMPLE -I$(VWATCH_SDK)/lvgl -I$(LVGL_DIR)
CFLAGS     += $(LVGL_INC)
LIB_SRCS   += $(VWATCH_SDK)/src/vw_lvgl.c
LVGL_SRCS  := $(shell find $(LVGL_DIR)/src -name '*.c')
LVGL_OBJS  := $(patsubst $(LVGL_DIR)/%.c,$(LVGL_BUILD)/%.o,$(LVGL_SRCS))
LVGL_LIB   := $(LVGL_BUILD)/liblvgl.a

$(LVGL_BUILD)/%.o: $(LVGL_DIR)/%.c $(VWATCH_SDK)/lvgl/lv_conf.h
	@mkdir -p $(dir $@)
	@$(CC) $(CPU) -O2 -std=gnu11 -ffunction-sections -fdata-sections -w $(LVGL_INC) -c $< -o $@

$(LVGL_LIB): $(LVGL_OBJS)
	@$(AR) rcs $@ $^
	@echo "  LVGL built: $(words $(LVGL_OBJS)) objects -> $@"

$(LVGL_DIR)/src/lvgl.h:
	@echo "LVGL sources missing: run  git submodule update --init" && false
endif

LIB_OBJS := $(patsubst %.c,build/sdk/%.o,$(notdir $(LIB_SRCS)))
SDK_LIB  := build/sdk/libvwatch.a

build/sdk/%.o: $(VWATCH_SDK)/src/%.c $(wildcard $(VWATCH_SDK)/include/*.h $(VWATCH_SDK)/src/*.h)
	@mkdir -p build/sdk
	$(CC) $(CFLAGS) -c $< -o $@

$(SDK_LIB): $(LIB_OBJS)
	@$(AR) rcs $@ $^

all: build/$(APP).elf

build/$(APP).elf: $(APP_SRCS) $(CORE_SRCS) $(SDK_LIB) $(LVGL_LIB) $(wildcard $(VWATCH_SDK)/include/*.h $(VWATCH_SDK)/src/*.h)
	@mkdir -p build
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(APP_SRCS) $(CORE_SRCS) $(SDK_LIB) $(LVGL_LIB) -lm
	@$(SIZE) $@

size: build/$(APP).elf
	@$(SIZE) -A $< | awk '/^\.(text|data|bss)/ {print}'

run: build/$(APP).elf
	python3 $(VWATCH_SDK)/../host/vwhost.py build/$(APP).elf --ticks 12 --png build/$(APP).png

clean:
	rm -rf build

.PHONY: all size run clean
