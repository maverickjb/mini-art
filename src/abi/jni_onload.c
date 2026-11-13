#include "abi.h"

#include <stddef.h>

int jni_onload(NativeLib *lib, void *vm)
{
    void *symbol;

    (void)vm;
    if (lib == NULL) {
        return -1;
    }

    symbol = native_loader_symbol(lib, "JNI_OnLoad");
    if (symbol == NULL) {
        return -1;
    }
    (void)symbol;
    return -1;
}
