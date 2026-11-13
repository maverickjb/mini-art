#ifndef MINI_ART_ABI_H
#define MINI_ART_ABI_H

typedef struct NativeLib NativeLib;

NativeLib *native_loader_load(const char *path);
void native_loader_close(NativeLib *lib);
void *native_loader_symbol(NativeLib *lib, const char *name);

/* Looks up JNI_OnLoad. Calling it needs a JNIEnv, which is not wired yet. */
int jni_onload(NativeLib *lib, void *vm);

#endif
