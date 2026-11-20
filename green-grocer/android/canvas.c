#include "canvas.h"

#include <android/log.h>
#include <android/native_window_jni.h>
#include <string.h>

static bool exception(gg_canvas *canvas) {
    if ((*canvas->env)->ExceptionCheck(canvas->env) == JNI_FALSE) return false;
    __android_log_print(ANDROID_LOG_ERROR, "GreenGrocer", "Android canvas exception");
    (*canvas->env)->ExceptionDescribe(canvas->env);
    (*canvas->env)->ExceptionClear(canvas->env);
    return true;
}

void gg_canvas_end(gg_canvas *canvas) {
    if (canvas->env != NULL) {
        (void)exception(canvas);
        if (canvas->canvas != NULL && canvas->unlock != NULL) {
            (*canvas->env)->CallVoidMethod(canvas->env, canvas->surface,
                                          canvas->unlock, canvas->canvas);
            (void)exception(canvas);
        }
        if (canvas->frame) (*canvas->env)->PopLocalFrame(canvas->env, NULL);
    }
    if (canvas->detach) (*canvas->vm)->DetachCurrentThread(canvas->vm);
    memset(canvas, 0, sizeof(*canvas));
}

bool gg_canvas_window_release(gg_canvas_window *window) {
    if (window->surface == NULL) return true;
    JNIEnv *env = NULL;
    bool detach = false;
    jint status = (*window->vm)->GetEnv(window->vm, (void **)&env, JNI_VERSION_1_6);
    if (status == JNI_EDETACHED) {
        if ((*window->vm)->AttachCurrentThread(window->vm, &env, NULL) != JNI_OK) return false;
        detach = true;
    } else if (status != JNI_OK) return false;
    (*env)->CallVoidMethod(env, window->surface, window->release);
    bool success = (*env)->ExceptionCheck(env) == JNI_FALSE;
    if (!success) {
        __android_log_print(ANDROID_LOG_ERROR, "GreenGrocer", "Android Surface release failed");
        (*env)->ExceptionDescribe(env);
        (*env)->ExceptionClear(env);
    }
    (*env)->DeleteGlobalRef(env, window->surface);
    ANativeWindow_release(window->window);
    if (detach) (*window->vm)->DetachCurrentThread(window->vm);
    memset(window, 0, sizeof(*window));
    return success;
}

bool gg_canvas_begin(gg_canvas *canvas, gg_canvas_window *retained, ANativeActivity *activity,
                     ANativeWindow *window) {
    memset(canvas, 0, sizeof(*canvas));
    if (retained->surface != NULL && (retained->window != window || retained->vm != activity->vm)) {
        if (!gg_canvas_window_release(retained)) return false;
    }
    canvas->vm = activity->vm;
    jint status = (*canvas->vm)->GetEnv(canvas->vm, (void **)&canvas->env, JNI_VERSION_1_6);
    if (status == JNI_EDETACHED) {
        if ((*canvas->vm)->AttachCurrentThread(canvas->vm, &canvas->env, NULL) != JNI_OK)
            return false;
        canvas->detach = true;
    } else if (status != JNI_OK) return false;
    JNIEnv *env = canvas->env;
    if ((*env)->PushLocalFrame(env, 32) != JNI_OK) goto failed;
    canvas->frame = true;
    if (retained->surface == NULL) {
        jobject created = ANativeWindow_toSurface(env, window);
        if (created == NULL || exception(canvas)) goto failed;
        jclass type = (*env)->GetObjectClass(env, created);
        if (type == NULL || exception(canvas)) goto failed;
        jmethodID release = (*env)->GetMethodID(env, type, "release", "()V");
        if (release == NULL || exception(canvas)) goto failed;
        jobject global = (*env)->NewGlobalRef(env, created);
        if (global == NULL || exception(canvas)) goto failed;
        ANativeWindow_acquire(window);
        *retained = (gg_canvas_window){activity->vm, window, global, release};
    }
    canvas->surface = retained->surface;
    jclass surface = (*env)->GetObjectClass(env, canvas->surface);
    if (surface == NULL || exception(canvas)) goto failed;
    jclass graphics = (*env)->FindClass(env, "android/graphics/Canvas");
    if (graphics == NULL || exception(canvas)) goto failed;
    jclass paint = (*env)->FindClass(env, "android/graphics/Paint");
    if (paint == NULL || exception(canvas)) goto failed;
#define METHOD(destination, owner, name, signature) \
    destination = (*env)->GetMethodID(env, owner, name, signature); \
    if (destination == NULL || exception(canvas)) goto failed
    jmethodID lock, constructor;
    METHOD(lock, surface, "lockCanvas", "(Landroid/graphics/Rect;)Landroid/graphics/Canvas;");
    METHOD(canvas->unlock, surface, "unlockCanvasAndPost", "(Landroid/graphics/Canvas;)V");
    METHOD(constructor, paint, "<init>", "(I)V");
    METHOD(canvas->color, graphics, "drawColor", "(I)V");
    METHOD(canvas->rectangle, graphics, "drawRect", "(FFFFLandroid/graphics/Paint;)V");
    METHOD(canvas->text, graphics, "drawText", "(Ljava/lang/String;FFLandroid/graphics/Paint;)V");
    METHOD(canvas->paint_color, paint, "setColor", "(I)V");
    METHOD(canvas->paint_size, paint, "setTextSize", "(F)V");
    METHOD(canvas->measure, paint, "measureText", "(Ljava/lang/String;)F");
#undef METHOD
    canvas->paint = (*env)->NewObject(env, paint, constructor, 1); /* anti-alias */
    if (canvas->paint == NULL || exception(canvas)) goto failed;
    canvas->canvas = (*env)->CallObjectMethod(env, canvas->surface, lock, NULL);
    if (canvas->canvas == NULL || exception(canvas)) goto failed;
    return true;
failed:
    gg_canvas_end(canvas);
    return false;
}

static void paint(gg_canvas *canvas, float size, uint32_t color) {
    (*canvas->env)->CallVoidMethod(canvas->env, canvas->paint, canvas->paint_size, size);
    (*canvas->env)->CallVoidMethod(canvas->env, canvas->paint, canvas->paint_color, (jint)color);
}

void gg_canvas_clear(gg_canvas *canvas, uint32_t color) {
    (*canvas->env)->CallVoidMethod(canvas->env, canvas->canvas, canvas->color, (jint)color);
}

void gg_canvas_rect(gg_canvas *canvas, float x, float y, float width,
                    float height, uint32_t color) {
    paint(canvas, 1.0f, color);
    (*canvas->env)->CallVoidMethod(canvas->env, canvas->canvas, canvas->rectangle,
                                  x, y, x + width, y + height, canvas->paint);
}

void gg_canvas_text(gg_canvas *canvas, const char *text, float x,
                    float baseline, float size, uint32_t color) {
    paint(canvas, size, color);
    jstring value = (*canvas->env)->NewStringUTF(canvas->env, text);
    if (value == NULL || exception(canvas)) return;
    (*canvas->env)->CallVoidMethod(canvas->env, canvas->canvas, canvas->text,
                                  value, x, baseline, canvas->paint);
    (*canvas->env)->DeleteLocalRef(canvas->env, value);
}

float gg_canvas_measure(gg_canvas *canvas, const char *text, float size) {
    paint(canvas, size, 0);
    jstring value = (*canvas->env)->NewStringUTF(canvas->env, text);
    if (value == NULL || exception(canvas)) return 0;
    float width = (*canvas->env)->CallFloatMethod(canvas->env, canvas->paint,
                                               canvas->measure, value);
    (*canvas->env)->DeleteLocalRef(canvas->env, value);
    return width;
}

void gg_canvas_text16(gg_canvas *canvas, const uint16_t *text, size_t units,
                      float x, float baseline, float size, uint32_t color) {
    if (units > INT32_MAX) return;
    paint(canvas, size, color);
    jstring value = (*canvas->env)->NewString(canvas->env, text, (jsize)units);
    if (value == NULL || exception(canvas)) return;
    (*canvas->env)->CallVoidMethod(canvas->env, canvas->canvas, canvas->text,
                                  value, x, baseline, canvas->paint);
    (*canvas->env)->DeleteLocalRef(canvas->env, value);
}

float gg_canvas_measure16(gg_canvas *canvas, const uint16_t *text, size_t units,
                         float size) {
    if (units > INT32_MAX) return 0;
    paint(canvas, size, 0);
    jstring value = (*canvas->env)->NewString(canvas->env, text, (jsize)units);
    if (value == NULL || exception(canvas)) return 0;
    float width = (*canvas->env)->CallFloatMethod(canvas->env, canvas->paint,
                                               canvas->measure, value);
    (*canvas->env)->DeleteLocalRef(canvas->env, value);
    return width;
}
