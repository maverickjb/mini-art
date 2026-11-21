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

## Layout

- `src/apk` opens the ZIP, the binary manifest, and `resources.arsc`
- `src/dex` reads `classes.dex` through `classesN.dex`
- `src/verifier` checks a method before it runs
- `src/runtime` is the class linker, interpreter, heap, and JNI
- `src/framework` launches the activity; `src/framework/native` is the framework method stubs
- `src/ui` draws toasts and inflated layouts with LVGL and SDL
- `src/abi` loads `lib/<abi>/*.so` and looks up `JNI_OnLoad`
