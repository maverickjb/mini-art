#ifndef MINI_ART_FRAMEWORK_H
#define MINI_ART_FRAMEWORK_H

#include "apk.h"
#include "runtime.h"

typedef struct LoadedApk LoadedApk;

typedef struct RuntimeConfig {
    int sdk_version;
} RuntimeConfig;

int android_compat_init(Runtime *rt, const RuntimeConfig *config);

LoadedApk *loaded_apk_create(Runtime *rt, Apk *apk);
void loaded_apk_destroy(LoadedApk *loaded);
const char *loaded_apk_launcher(const LoadedApk *loaded);
const Apk *loaded_apk_apk(const LoadedApk *loaded);

int activity_thread_launch(Runtime *rt, LoadedApk *loaded);
int native_register_framework(Runtime *rt);
void native_activity_prepare(const Apk *apk, Runtime *rt);
int native_activity_content_open(void);
void native_activity_clear(void);

#endif
