#include <jni.h>

/*
 * The desktop JDK C header types AttachCurrentThread's environment output as
 * void **. Android's NDK C header types the same parameter as JNIEnv **.
 * The shared implementation uses the desktop spelling because the host
 * contract compiles against a JDK; provide Android's exact type here without
 * weakening warnings or changing the runtime call.
 */
#ifdef __ANDROID__
#define AttachCurrentThread(vm, environment, args) \
    AttachCurrentThread((vm), (JNIEnv **)(environment), (args))
#endif

#include "clipboard_jni_impl.inc"

#ifdef __ANDROID__
#undef AttachCurrentThread
#endif
