CC ?= cc
BUILD := build
# Upstream SDL2 submodule, pinned to release-2.32.10.
SDL_SRC := third_party/sdl2
SDL_BUILD := $(BUILD)/sdl2
SDL_LIB := $(SDL_BUILD)/libSDL2.a
# Host /usr/include describes the machine that runs cmake, not the target.
# Using it for an aarch64 build makes pthread_mutex_t 32 bytes while the
# target libc writes 48, which corrupts the heap on the first mutex.
CC_MACHINE := $(shell $(CC) -dumpmachine)
HOST_MACHINE := $(shell cc -dumpmachine)
# aarch64 X11 headers and libX11.so live here, separate from the host includes.
AARCH64_SYSROOT := $(HOME)/aarch64-sysroot
ifneq ($(CC_MACHINE),$(HOST_MACHINE))
SDL_CMAKE_FLAGS := \
	-DCMAKE_FIND_ROOT_PATH="$(AARCH64_SYSROOT);/usr/$(CC_MACHINE)" \
	-DCMAKE_FIND_ROOT_PATH_MODE_PROGRAM=NEVER \
	-DCMAKE_FIND_ROOT_PATH_MODE_LIBRARY=ONLY \
	-DCMAKE_FIND_ROOT_PATH_MODE_INCLUDE=ONLY \
	-DCMAKE_FIND_ROOT_PATH_MODE_PACKAGE=ONLY \
	-DSDL_OPENGL=OFF \
	-DSDL_OPENGLES=OFF
ifneq ($(wildcard $(AARCH64_SYSROOT)/usr/include/X11/Xlib.h),)
SDL_CMAKE_FLAGS += -DSDL_X11=ON -DHAVE_XGENERICEVENT=1 -DCMAKE_C_FLAGS=-DNO_SHARED_MEMORY
else
SDL_CMAKE_FLAGS += -DSDL_X11=OFF
endif
SDL_CMAKE_ENV := PKG_CONFIG_LIBDIR=/usr/$(CC_MACHINE)/lib/pkgconfig PKG_CONFIG_PATH=
else
SDL_CMAKE_FLAGS :=
SDL_CMAKE_ENV :=
endif
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
	src/framework/android_compat.c \
	src/framework/loaded_apk.c \
	src/framework/native/native_registry.c \
	src/framework/native/native_random.c \
	src/framework/native/native_system.c \
	src/framework/native/native_thread.c \
	src/framework/native/native_activity.c \
	src/framework/native/native_widget.c \
	src/framework/native/native_media.c \
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
	$(SDL_CMAKE_ENV) cmake -S $(SDL_SRC) -B $(SDL_BUILD) \
		-DCMAKE_C_COMPILER=$(CC) \
		-DSDL_SHARED=OFF \
		-DSDL_STATIC=ON \
		-DSDL_TEST=OFF \
		-DSDL_WAYLAND=OFF \
		-DCMAKE_BUILD_TYPE=Release \
		$(SDL_CMAKE_FLAGS)
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
