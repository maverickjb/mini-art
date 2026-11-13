#ifndef MINI_ART_FRAMEWORK_H
#define MINI_ART_FRAMEWORK_H

#include "apk.h"
#include "runtime.h"

typedef struct LoadedApk LoadedApk;

LoadedApk *loaded_apk_create(Runtime *rt, Apk *apk);
void loaded_apk_destroy(LoadedApk *loaded);
const char *loaded_apk_launcher(const LoadedApk *loaded);
const Apk *loaded_apk_apk(const LoadedApk *loaded);

int activity_thread_launch(Runtime *rt, LoadedApk *loaded);

#endif
