# Сборка прошивки:     make                 (по умолчанию STM32F401RE)
#                      make MCU=F446RE
# Обе прошивки:        make all-mcus
# Тесты на ПК:         make test
# Тесты с TSan:        make test-tsan
# Прошивка платы:      make flash           (нужен st-flash из stlink-tools)

MCU ?= F401RE

ifeq ($(MCU),F401RE)
  LDSCRIPT := ld/stm32f401re.ld
else ifeq ($(MCU),F446RE)
  LDSCRIPT := ld/stm32f446re.ld
else
  $(error MCU должен быть F401RE или F446RE)
endif

BUILD := build/$(MCU)
TARGET := $(BUILD)/firmware

CROSS ?= arm-none-eabi-
CC := $(CROSS)gcc
OBJCOPY := $(CROSS)objcopy
SIZE := $(CROSS)size

SRCS := $(wildcard src/*.c)
OBJS := $(SRCS:src/%.c=$(BUILD)/%.o)
DEPS := $(OBJS:.o=.d)

ARCH := -mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16 -mfloat-abi=hard

CFLAGS := $(ARCH) -std=c11 -O2 -g3 \
          -Wall -Wextra -Werror -Wshadow -Wdouble-promotion -Wundef \
          -ffunction-sections -fdata-sections -fno-common \
          -DMCU_NAME='"STM32$(MCU)"' -MMD -MP

LDFLAGS := $(ARCH) -T$(LDSCRIPT) -Lld -nostartfiles \
           --specs=nano.specs --specs=nosys.specs \
           -Wl,--gc-sections -Wl,-Map=$(TARGET).map -Wl,--print-memory-usage

.PHONY: all all-mcus flash test test-tsan clean

all: $(TARGET).elf $(TARGET).bin $(TARGET).hex
	@$(SIZE) $(TARGET).elf

$(BUILD)/%.o: src/%.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(TARGET).elf: $(OBJS) $(LDSCRIPT) ld/sections.ld
	$(CC) $(OBJS) $(LDFLAGS) -o $@

$(TARGET).bin: $(TARGET).elf
	$(OBJCOPY) -O binary $< $@

$(TARGET).hex: $(TARGET).elf
	$(OBJCOPY) -O ihex $< $@

$(BUILD):
	mkdir -p $@

all-mcus:
	$(MAKE) MCU=F401RE
	$(MAKE) MCU=F446RE

flash: $(TARGET).bin
	st-flash --reset write $< 0x08000000

# --- тесты на ПК: только модули, не зависящие от железа ---------------------

HOST_CC ?= gcc
TEST_SRCS := tests/test_main.c tests/test_ringbuf.c tests/test_rx_tracker.c \
             tests/test_cli.c tests/test_strutil.c \
             src/ringbuf.c src/rx_tracker.c src/cli.c src/strutil.c
TEST_CFLAGS := -std=c11 -O1 -g -Wall -Wextra -Werror -Isrc -pthread

test: build/host/tests
	./build/host/tests

build/host/tests: $(TEST_SRCS) $(wildcard src/*.h) tests/test.h
	mkdir -p build/host
	$(HOST_CC) $(TEST_CFLAGS) -fsanitize=address,undefined -fno-omit-frame-pointer \
	    $(TEST_SRCS) -o $@

test-tsan: build/host/tests-tsan
	./build/host/tests-tsan

build/host/tests-tsan: $(TEST_SRCS) $(wildcard src/*.h) tests/test.h
	mkdir -p build/host
	$(HOST_CC) $(TEST_CFLAGS) -fsanitize=thread $(TEST_SRCS) -o $@

clean:
	rm -rf build

-include $(DEPS)
