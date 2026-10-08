#ifndef GREEN_GROCER_CANVAS_H
#define GREEN_GROCER_CANVAS_H

#include <android/native_activity.h>
#include <android/native_window.h>
#include <jni.h>
#include <stdbool.h>
#include <stdint.h>

/* Disposable Android drawing boundary. All JNI references belong to one frame. */
typedef struct {
    JNIEnv *env;
    JavaVM *vm;
    bool detach;
    bool frame;
    jobject surface, canvas, paint;
    jmethodID unlock, color, rectangle, text, paint_color, paint_size, measure;
} gg_canvas;

bool gg_canvas_begin(gg_canvas *canvas, ANativeActivity *activity,
                     ANativeWindow *window);
void gg_canvas_end(gg_canvas *canvas);
void gg_canvas_clear(gg_canvas *canvas, uint32_t color);
void gg_canvas_rect(gg_canvas *canvas, float x, float y, float width,
                    float height, uint32_t color);
void gg_canvas_text(gg_canvas *canvas, const char *text, float x,
                    float baseline, float size, uint32_t color);
float gg_canvas_measure(gg_canvas *canvas, const char *text, float size);

#endif
