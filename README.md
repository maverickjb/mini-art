# mini-art

A small C runtime that loads an Android APK and drives the launcher activity.

## Build

```sh
make
```

That writes `build/mini-art` for the host. On WSL the window needs `DISPLAY=:0`.

Cross-compile the same sources for arm64 with `aarch64-linux-gnu-gcc`. `BUILD` keeps that tree separate from the host binary:

```sh
make BUILD=build-arm CC=aarch64-linux-gnu-gcc -j"$(nproc)"
```

The binary is `build-arm/mini-art`. SDL is built with the same `CC`. `make clean` removes `build/`; `make BUILD=build-arm clean` removes the arm64 tree.

## Run

```sh
make run
```

`make run` launches `tests/apk/Hello.apk`, Hello World Toast 1.4.0 (MIT, https://github.com/TheHelloWorldWriter/HelloWorldToast). It shows the toast in a window and prints the text. Pass another fixture with `APK`:

```sh
make run APK=tests/apk/Calculator.apk
```

That opens the calculator and leaves the window up until it is closed. `tests/apk/Calculator.apk` is from https://github.com/Amey-Thakur/ANDROID-STUDIO-CALCULATOR. Test APKs live in `tests/apk/`.

The host binary does the same thing without `make`:

```sh
DISPLAY=:0 ./build/mini-art run tests/apk/Hello.apk
DISPLAY=:0 ./build/mini-art run tests/apk/Calculator.apk
```

Hello's window closes after the toast (3.5 s). Calculator stays open until the window is closed.

The arm64 binary runs on the host through `qemu-aarch64-static`. X11 libraries come from `$HOME/aarch64-sysroot`. No full-system VM is required:

```sh
DISPLAY=:0 \
LD_LIBRARY_PATH="$HOME/aarch64-sysroot/usr/lib/aarch64-linux-gnu" \
qemu-aarch64-static -L /usr/aarch64-linux-gnu \
  ./build-arm/mini-art run tests/apk/Hello.apk

DISPLAY=:0 \
LD_LIBRARY_PATH="$HOME/aarch64-sysroot/usr/lib/aarch64-linux-gnu" \
qemu-aarch64-static -L /usr/aarch64-linux-gnu \
  ./build-arm/mini-art run tests/apk/Calculator.apk
```

## Layout

- `src/apk` opens the ZIP, the binary manifest, and `resources.arsc`
- `src/dex` reads `classes.dex` through `classesN.dex`
- `src/verifier` checks a method before it runs
- `src/runtime` is the class linker, interpreter, heap, and JNI
- `src/framework` launches the activity; `src/framework/native` is the framework method stubs
- `src/ui` draws toasts and inflated layouts with LVGL and SDL
- `src/abi` loads `lib/<abi>/*.so` and looks up `JNI_OnLoad`
