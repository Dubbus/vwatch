# vwatch SDK build rules. An app Makefile sets APP and APP_SRCS, then:
#     VWATCH_SDK ?= ../../sdk
#     include $(VWATCH_SDK)/sdk.mk
# Targets: all (build/<APP>.elf), size, run (headless host), clean.

PORT    ?= qemu-mps2
CC      := arm-none-eabi-gcc
SIZE    := arm-none-eabi-size
CPU     := -mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16 -mfloat-abi=hard
CFLAGS  += $(CPU) -O2 -g -std=c11 -Wall -Wextra -ffunction-sections -fdata-sections \
           -I$(VWATCH_SDK)/include -I$(VWATCH_SDK)/src
LDFLAGS += $(CPU) --specs=nano.specs --specs=nosys.specs -T $(VWATCH_SDK)/port/$(PORT)/mps2_an386.ld \
           -Wl,--gc-sections -Wl,--no-warn-rwx-segments -Wl,-Map=build/$(APP).map
SDK_SRCS := $(VWATCH_SDK)/src/runtime.c $(VWATCH_SDK)/src/gfx.c $(VWATCH_SDK)/src/font5x8.c \
            $(VWATCH_SDK)/port/$(PORT)/port.c $(VWATCH_SDK)/port/$(PORT)/startup.c

all: build/$(APP).elf

build/$(APP).elf: $(APP_SRCS) $(SDK_SRCS) $(wildcard $(VWATCH_SDK)/include/*.h $(VWATCH_SDK)/src/*.h)
	@mkdir -p build
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(APP_SRCS) $(SDK_SRCS) -lm
	@$(SIZE) $@

size: build/$(APP).elf
	@$(SIZE) -A $< | awk '/^\.(text|data|bss)/ {print}'

run: build/$(APP).elf
	python3 $(VWATCH_SDK)/../host/vwhost.py build/$(APP).elf --ticks 12 --png build/$(APP).png

clean:
	rm -rf build

.PHONY: all size run clean
