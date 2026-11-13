CC ?= cc
CFLAGS := -std=c11 -Wall -Wextra -Wpedantic -D_POSIX_C_SOURCE=200809L -Iinclude
LDFLAGS := -ldl

SRCS := \
	src/main.c \
	src/apk/zip_archive.c \
	src/apk/inflate.c \
	src/apk/binary_xml.c \
	src/apk/resource_table.c \
	src/dex/dex_file.c \
	src/dex/code_item.c \
	src/dex/multi_dex.c \
	src/verifier/verifier.c \
	src/runtime/runtime.c \
	src/runtime/class_linker.c \
	src/runtime/interpreter.c \
	src/runtime/heap.c \
	src/runtime/thread.c \
	src/runtime/jni.c \
	src/runtime/well_known_classes.c \
	src/runtime/mirror.c \
	src/framework/activity_thread.c \
	src/framework/loaded_apk.c \
	src/abi/native_loader.c \
	src/abi/jni_onload.c \
	boot/native/log_jni.c \
	boot/native/system_jni.c

BUILD := build
APK ?= tests/apps/Hello.apk
OBJS := $(SRCS:%.c=$(BUILD)/%.o)
DEPS := $(OBJS:.o=.d)

.PHONY: all clean run

all: $(BUILD)/mini-art

run: $(BUILD)/mini-art
	$(BUILD)/mini-art run $(APK)

$(BUILD)/mini-art: $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(BUILD)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -MMD -MP -c -o $@ $<

-include $(DEPS)

clean:
	rm -rf $(BUILD)
