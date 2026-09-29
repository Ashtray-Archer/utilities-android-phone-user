#define _POSIX_C_SOURCE 200809L

#include <android/log.h>
#include <android/native_window.h>
#include <android_native_app_glue.h>

#include "android_accelerometer.h"
#include "spirit_level_renderer.h"

#include <time.h>

#define SPIRIT_LEVEL_PACKAGE "com.ashtrayarcher.spiritlevel"
#define SPIRIT_LEVEL_PERIOD_US 20000

struct spirit_level_state {
    struct android_app *app;
    struct android_accelerometer source;
    struct accelerometer_model model;
    enum compact_acceleration_encode_status last_status;
    bool source_failed;
    bool reported_frame;
    int64_t accept_after_ns;
    /* Display cache only: there is no retained raw x/y/z numerical state. */
    char diagnostics[64];
};

/* Framework lookup only. No sensor lookup, fusion, or orientation measurement. */
static jobject call_object(
    JNIEnv *env, jobject object, const char *name, const char *signature)
{
    if (object == NULL || (*env)->ExceptionCheck(env)) {
        return NULL;
    }
    jclass type = (*env)->GetObjectClass(env, object);
    if (type == NULL) {
        return NULL;
    }
    jmethodID method = (*env)->GetMethodID(env, type, name, signature);
    (*env)->DeleteLocalRef(env, type);
    return method == NULL ? NULL : (*env)->CallObjectMethod(env, object, method);
}

static int display_rotation(ANativeActivity *activity)
{
    JNIEnv *env = NULL;
    JavaVM *vm = activity->vm;
    bool attached_here = false;
    int rotation = -1;
    jint status = (*vm)->GetEnv(vm, (void **)&env, JNI_VERSION_1_6);
    if (status == JNI_EDETACHED) {
        /* void* accommodates the C JNI parameter spelling across host/NDK
         * headers; env is still the JNIEnv* written by AttachCurrentThread. */
        if ((*vm)->AttachCurrentThread(vm, (void *)&env, NULL) != JNI_OK) {
            return -1;
        }
        attached_here = true;
    } else if (status != JNI_OK) {
        return -1;
    }
    if ((*env)->PushLocalFrame(env, 8) != JNI_OK) {
        if ((*env)->ExceptionCheck(env)) {
            (*env)->ExceptionClear(env);
        }
        if (attached_here) {
            (void)(*vm)->DetachCurrentThread(vm);
        }
        return -1;
    }
    jobject manager = call_object(env, activity->clazz,
        "getWindowManager", "()Landroid/view/WindowManager;");
    jobject display = call_object(env, manager,
        "getDefaultDisplay", "()Landroid/view/Display;");
    if (display != NULL && !(*env)->ExceptionCheck(env)) {
        jclass type = (*env)->GetObjectClass(env, display);
        jmethodID method = type == NULL ? NULL : (*env)->GetMethodID(env, type, "getRotation", "()I");
        if (method != NULL) {
            rotation = (int)(*env)->CallIntMethod(env, display, method);
        }
    }
    if ((*env)->ExceptionCheck(env)) {
        (*env)->ExceptionClear(env);
        rotation = -1;
    }
    (void)(*env)->PopLocalFrame(env, NULL);
    if (attached_here) {
        (void)(*vm)->DetachCurrentThread(vm);
    }
    return rotation >= 0 && rotation <= 3 ? rotation : -1;
}

static void draw_screen(struct spirit_level_state *state)
{
    if (state->app->window == NULL) {
        return;
    }
    bool frame_drawn = false;
    ANativeWindow_Buffer buffer;
    if (ANativeWindow_lock(state->app->window, &buffer, NULL) != 0) {
        return;
    }
    if (buffer.format == WINDOW_FORMAT_RGBA_8888) {
        int left = 0;
        int top = 0;
        int right = buffer.width;
        int bottom = buffer.height;
        ARect content = state->app->contentRect;
        /* Keep the footer inside Android's usable area, above system bars. */
        if (content.right > content.left && content.bottom > content.top) {
            if (content.left > left) { left = content.left; }
            if (content.top > top) { top = content.top; }
            if (content.right < right) { right = content.right; }
            if (content.bottom < bottom) { bottom = content.bottom; }
        }
        if (left >= 0 && top >= 0 && right > left && bottom > top &&
            right <= buffer.width && bottom <= buffer.height) {
            struct spirit_level_surface surface = {
                (uint32_t *)buffer.bits + (size_t)top * (size_t)buffer.stride + (size_t)left,
                right - left, bottom - top, buffer.stride};
            struct spirit_level_geometry geometry = spirit_level_geometry_make(
                &state->model, state->last_status, display_rotation(state->app->activity));
            if (state->source_failed || !android_accelerometer_is_available(&state->source)) {
                geometry.status = SPIRIT_LEVEL_NO_SENSOR;
            }
            frame_drawn = spirit_level_draw(surface, &geometry, state->diagnostics);
        }
    }
    int posted = ANativeWindow_unlockAndPost(state->app->window);
    if (frame_drawn && posted == 0 && !state->reported_frame) {
        __android_log_print(ANDROID_LOG_INFO, "SpiritLevel", "%s", "Rendered spirit-level frame");
        state->reported_frame = true;
    }
}

static void set_sensor_enabled(struct spirit_level_state *state, bool enabled)
{
    if (!enabled) {
        android_accelerometer_disable(&state->source);
        /* A resumed screen must wait for a fresh sample, not present stale tilt. */
        state->model.have_android_sample = false;
        state->last_status = COMPACT_ACCELERATION_ENCODE_OK;
        (void)snprintf(state->diagnostics, sizeof(state->diagnostics), "X ? Y ? Z ?");
        return;
    }
    if (android_accelerometer_is_enabled(&state->source)) {
        return;
    }
    struct timespec now;
    if (clock_gettime(CLOCK_BOOTTIME, &now) != 0) {
        state->source_failed = true;
        __android_log_print(ANDROID_LOG_ERROR, "SpiritLevel", "%s", "Could not timestamp sensor activation");
        return;
    }
    state->accept_after_ns = (int64_t)now.tv_sec * INT64_C(1000000000) + now.tv_nsec;
    enum android_accelerometer_status status =
        android_accelerometer_enable(&state->source, SPIRIT_LEVEL_PERIOD_US);
    state->source_failed = status != ANDROID_ACCELEROMETER_OK;
    if (state->source_failed) {
        __android_log_print(ANDROID_LOG_ERROR, "SpiritLevel", "%s",
            android_accelerometer_status_text(status));
    }
}

static void handle_app_command(struct android_app *app, int32_t command)
{
    struct spirit_level_state *state = app->userData;
    switch (command) {
        case APP_CMD_INIT_WINDOW:
            if (app->window != NULL) {
                (void)ANativeWindow_setBuffersGeometry(app->window, 0, 0, WINDOW_FORMAT_RGBA_8888);
            }
            draw_screen(state);
            break;
        case APP_CMD_GAINED_FOCUS:
            set_sensor_enabled(state, true);
            draw_screen(state);
            break;
        case APP_CMD_LOST_FOCUS:
        case APP_CMD_PAUSE:
            set_sensor_enabled(state, false);
            break;
        case APP_CMD_CONFIG_CHANGED:
        case APP_CMD_WINDOW_RESIZED:
        case APP_CMD_WINDOW_REDRAW_NEEDED:
        case APP_CMD_CONTENT_RECT_CHANGED:
            draw_screen(state);
            break;
        default:
            break;
    }
}

static void consume_readings(struct spirit_level_state *state)
{
    for (;;) {
        struct android_accelerometer_reading reading;
        enum android_accelerometer_read_result result = android_accelerometer_next(&state->source, &reading);
        if (result == ANDROID_ACCELEROMETER_READ_EMPTY) {
            break;
        }
        if (result == ANDROID_ACCELEROMETER_READ_ERROR) {
            set_sensor_enabled(state, false);
            state->source_failed = true;
            __android_log_print(ANDROID_LOG_ERROR, "SpiritLevel", "%s", "Could not read the Android accelerometer");
            break;
        }
        /* Drain wakeups even while paused; do not accept queued pre-resume data. */
        if (!android_accelerometer_is_enabled(&state->source) ||
            reading.timestamp_ns < state->accept_after_ns) {
            continue;
        }
        spirit_level_diagnostics(state->diagnostics, reading.x, reading.y, reading.z);
        state->last_status = accelerometer_model_accept(&state->model,
            (struct physical_acceleration){reading.x, reading.y, reading.z});
    }
    draw_screen(state);
}

void android_main(struct android_app *app)
{
    struct spirit_level_state state = {0};
    state.app = app;
    accelerometer_model_initialize(&state.model);
    (void)snprintf(state.diagnostics, sizeof(state.diagnostics), "X ? Y ? Z ?");
    app->userData = &state;
    app->onAppCmd = handle_app_command;
    enum android_accelerometer_status status = android_accelerometer_open(
        &state.source, app->looper, LOOPER_ID_USER, SPIRIT_LEVEL_PACKAGE);
    state.source_failed = status != ANDROID_ACCELEROMETER_OK;
    if (state.source_failed) {
        __android_log_print(ANDROID_LOG_ERROR, "SpiritLevel", "%s", android_accelerometer_status_text(status));
    }
    while (app->destroyRequested == 0) {
        int events = 0;
        struct android_poll_source *source = NULL;
        int ident = ALooper_pollOnce(-1, NULL, &events, (void **)&source);
        if (source != NULL) {
            source->process(app, source);
        }
        if (app->destroyRequested != 0) {
            break;
        }
        if (ident == ALOOPER_POLL_ERROR) {
            __android_log_print(ANDROID_LOG_ERROR, "SpiritLevel", "%s", "Could not wait for Android events");
            break;
        }
        if (ident == LOOPER_ID_USER && android_accelerometer_is_available(&state.source)) {
            consume_readings(&state);
        }
    }
    set_sensor_enabled(&state, false);
    android_accelerometer_close(&state.source);
}
