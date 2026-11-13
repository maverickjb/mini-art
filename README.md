# mini-art

A small C runtime that loads an Android APK and drives the launcher activity.

## Build

```sh
make
```

## Run

```sh
make run
```

`tests/apps/Hello.apk` is Hello World Toast 1.4.0, a one-activity hello-world APK (MIT, https://github.com/TheHelloWorldWriter/HelloWorldToast). `make run` launches its activity and prints the toast text. Larger fixtures go in `apps/`.

## Layout

- `src/apk` opens the ZIP, the binary manifest, and `resources.arsc`
- `src/dex` reads `classes.dex` through `classesN.dex`
- `src/verifier` checks a method before it runs
- `src/runtime` is the class linker, interpreter, heap, and JNI
- `boot/java` is the stub framework, compiled to `boot.dex` later
- `src/framework` calls `Application` and `Activity` lifecycle methods
- `src/abi` loads `lib/<abi>/*.so` and looks up `JNI_OnLoad`
