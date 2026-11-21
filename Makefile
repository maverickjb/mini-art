CC ?= cc
BUILD := build
# Upstream SDL2 submodule, pinned to release-2.32.10.
SDL_SRC := third_party/sdl2
SDL_BUILD := $(BUILD)/sdl2
SDL_LIB := $(SDL_BUILD)/libSDL2.a
SDL_CFLAGS := -D_REENTRANT \
	-I$(SDL_BUILD)/include-config-release/SDL2 \
	-I$(SDL_BUILD)/include/SDL2 \
	-I$(SDL_BUILD)/include

CFLAGS := -std=c11 -Wall -Wextra -Wpedantic -D_POSIX_C_SOURCE=200809L -Iinclude -Ithird_party/lvgl $(SDL_CFLAGS)
LDFLAGS := -pthread -ldl -lm

LVGL_DIR := third_party/lvgl
LVGL_SRCS := $(shell find $(LVGL_DIR)/src -name '*.c')
LVGL_CFLAGS := -std=c11 -O2 -w -D_POSIX_C_SOURCE=200809L -I$(LVGL_DIR) -Iinclude $(SDL_CFLAGS) -DLV_CONF_INCLUDE_SIMPLE

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
	src/runtime/java_object.c \
	src/runtime/class_linker.c \
	src/runtime/interpreter.c \
	src/runtime/heap.c \
	src/runtime/thread.c \
	src/runtime/jni.c \
	src/runtime/well_known_classes.c \
	src/runtime/mirror.c \
	src/framework/activity_thread.c \
	src/framework/loaded_apk.c \
	src/framework/native/native_registry.c \
	src/framework/native/native_activity.c \
	src/framework/native/native_widget.c \
	src/framework/native/native_string.c \
	src/framework/native/native_math.c \
	src/ui/ui.c \
	src/ui/toast_view.c \
	src/abi/native_loader.c \
	src/abi/jni_onload.c

APK ?= tests/apk/Hello.apk
OBJS := $(SRCS:%.c=$(BUILD)/%.o)
LVGL_OBJS := $(LVGL_SRCS:%.c=$(BUILD)/%.o)
DEPS := $(OBJS:.o=.d)

.PHONY: all clean run

all: $(BUILD)/mini-art

run: $(BUILD)/mini-art
	$(BUILD)/mini-art run $(APK)

$(SDL_LIB): $(SDL_SRC)/CMakeLists.txt
	cmake -S $(SDL_SRC) -B $(SDL_BUILD) \
		-DCMAKE_C_COMPILER=$(CC) \
		-DSDL_SHARED=OFF \
		-DSDL_STATIC=ON \
		-DSDL_TEST=OFF \
		-DSDL_WAYLAND=OFF \
		-DCMAKE_BUILD_TYPE=Release
	cmake --build $(SDL_BUILD) --target SDL2-static -j

$(BUILD)/mini-art: $(OBJS) $(LVGL_OBJS) $(SDL_LIB)
	$(CC) -o $@ $(OBJS) $(LVGL_OBJS) $(SDL_LIB) $(LDFLAGS)

$(BUILD)/%.o: %.c $(SDL_LIB)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -MMD -MP -c -o $@ $<

$(BUILD)/$(LVGL_DIR)/%.o: $(LVGL_DIR)/%.c include/lv_conf.h $(SDL_LIB)
	@mkdir -p $(dir $@)
	$(CC) $(LVGL_CFLAGS) -c -o $@ $<

-include $(DEPS)

clean:
	rm -rf $(BUILD)
