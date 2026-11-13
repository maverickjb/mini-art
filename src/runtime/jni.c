#include "runtime.h"

int jni_call(ArtThread *thread, ArtMethod *method, JValue *args, JValue *result)
{
    (void)thread;
    (void)method;
    (void)args;
    (void)result;
    return -1;
}
