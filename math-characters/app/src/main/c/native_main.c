#include "pad_model.h"
#include "pad_ui.h"

#include <android/input.h>
#include <android/log.h>
#include <android/native_activity.h>
#include <android/native_window.h>
#include <android/native_window_jni.h>
#include <android_native_app_glue.h>
#include <jni.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define LOG_TAG "ProgrammersUnicodePad"
#define LOG_ERROR(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#define LOG_INFO(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)

static const int32_t COLOR_BACKGROUND = (int32_t)0xFF101318u;
static const int32_t COLOR_PANEL = (int32_t)0xFF1A2028u;
static const int32_t COLOR_KEY = (int32_t)0xFF252E39u;
static const int32_t COLOR_KEY_BORDER = (int32_t)0xFF526274u;
static const int32_t COLOR_ACTION = (int32_t)0xFF31465Eu;
static const int32_t COLOR_TEXT = (int32_t)0xFFF2F5F8u;
static const int32_t COLOR_MUTED = (int32_t)0xFFB5C0CBu;
static const int32_t COLOR_ACCENT = (int32_t)0xFF8ED0FFu;

typedef struct {
    struct android_app *app;
    PadState state;
    size_t layout_index;
    bool redraw;
} AppContext;

typedef struct {
    JNIEnv *env;
    bool detach;
    jobject surface;
    jobject canvas;
    jobject paint;
    jobject typeface;
    jclass surface_class;
    jclass canvas_class;
    jclass paint_class;
    jclass typeface_class;
    jmethodID unlock_canvas;
    jmethodID draw_color;
    jmethodID draw_rect;
    jmethodID draw_text;
    jmethodID paint_set_color;
    jmethodID paint_set_text_size;
    jmethodID paint_measure_text;
    jmethodID paint_ascent;
    jmethodID paint_descent;
} CanvasSession;

typedef struct {
    float left;
    float top;
    float width;
    float height;
} Viewport;

static Viewport content_viewport(const struct android_app *app) {
    const float window_width = (float)ANativeWindow_getWidth(app->window);
    const float window_height = (float)ANativeWindow_getHeight(app->window);
    const int32_t content_width = app->contentRect.right - app->contentRect.left;
    const int32_t content_height = app->contentRect.bottom - app->contentRect.top;
    if (content_width <= 0 || content_height <= 0) {
        const Viewport full_window = {0.0f, 0.0f, window_width, window_height};
        return full_window;
    }
    const Viewport content = {
        (float)app->contentRect.left,
        (float)app->contentRect.top,
        (float)content_width,
        (float)content_height,
    };
    return content;
}

static PadRect place_in_viewport(PadRect rect, Viewport viewport) {
    rect.left += viewport.left;
    rect.right += viewport.left;
    rect.top += viewport.top;
    rect.bottom += viewport.top;
    return rect;
}

static bool attach_environment(ANativeActivity *activity, JNIEnv **environment,
                               bool *detach) {
    *detach = false;
    const jint status = (*activity->vm)->GetEnv(activity->vm, (void **)environment,
                                                JNI_VERSION_1_6);
    if (status == JNI_OK) {
        return true;
    }
    if (status != JNI_EDETACHED ||
        (*activity->vm)->AttachCurrentThread(activity->vm, environment, NULL) != JNI_OK) {
        LOG_ERROR("could not attach native thread to Android runtime");
        return false;
    }
    *detach = true;
    return true;
}

static bool clear_exception(JNIEnv *environment, const char *operation) {
    if ((*environment)->ExceptionCheck(environment) == JNI_FALSE) {
        return false;
    }
    LOG_ERROR("Android framework exception during %s", operation);
    (*environment)->ExceptionDescribe(environment);
    (*environment)->ExceptionClear(environment);
    return true;
}

static void end_canvas(AppContext *context, CanvasSession *session) {
    JNIEnv *environment = session->env;
    if (session->canvas != NULL && session->surface != NULL &&
        session->unlock_canvas != NULL) {
        (*environment)->CallVoidMethod(environment, session->surface,
                                       session->unlock_canvas, session->canvas);
        (void)clear_exception(environment, "Surface.unlockCanvasAndPost");
    }
    if (session->typeface != NULL) {
        (*environment)->DeleteLocalRef(environment, session->typeface);
    }
    if (session->paint != NULL) {
        (*environment)->DeleteLocalRef(environment, session->paint);
    }
    if (session->canvas != NULL) {
        (*environment)->DeleteLocalRef(environment, session->canvas);
    }
    if (session->surface != NULL) {
        (*environment)->DeleteLocalRef(environment, session->surface);
    }
    if (session->typeface_class != NULL) {
        (*environment)->DeleteLocalRef(environment, session->typeface_class);
    }
    if (session->paint_class != NULL) {
        (*environment)->DeleteLocalRef(environment, session->paint_class);
    }
    if (session->canvas_class != NULL) {
        (*environment)->DeleteLocalRef(environment, session->canvas_class);
    }
    if (session->surface_class != NULL) {
        (*environment)->DeleteLocalRef(environment, session->surface_class);
    }
    if (session->detach) {
        (void)(*context->app->activity->vm)->DetachCurrentThread(
            context->app->activity->vm);
    }
    memset(session, 0, sizeof(*session));
}

static bool begin_canvas(AppContext *context, CanvasSession *session) {
    memset(session, 0, sizeof(*session));
    if (!attach_environment(context->app->activity, &session->env, &session->detach)) {
        return false;
    }
    JNIEnv *environment = session->env;
    session->surface = ANativeWindow_toSurface(environment, context->app->window);
    if (session->surface == NULL || clear_exception(environment, "ANativeWindow_toSurface")) {
        end_canvas(context, session);
        return false;
    }
    session->surface_class = (*environment)->GetObjectClass(environment, session->surface);
    session->canvas_class = (*environment)->FindClass(environment, "android/graphics/Canvas");
    session->paint_class = (*environment)->FindClass(environment, "android/graphics/Paint");
    session->typeface_class = (*environment)->FindClass(environment, "android/graphics/Typeface");
    if (session->surface_class == NULL || session->canvas_class == NULL ||
        session->paint_class == NULL || session->typeface_class == NULL ||
        clear_exception(environment, "finding graphics classes")) {
        end_canvas(context, session);
        return false;
    }

    const jmethodID lock_canvas = (*environment)->GetMethodID(
        environment, session->surface_class, "lockCanvas",
        "(Landroid/graphics/Rect;)Landroid/graphics/Canvas;");
    session->unlock_canvas = (*environment)->GetMethodID(
        environment, session->surface_class, "unlockCanvasAndPost",
        "(Landroid/graphics/Canvas;)V");
    const jmethodID paint_constructor = (*environment)->GetMethodID(
        environment, session->paint_class, "<init>", "()V");
    const jmethodID paint_set_antialias = (*environment)->GetMethodID(
        environment, session->paint_class, "setAntiAlias", "(Z)V");
    const jmethodID paint_set_typeface = (*environment)->GetMethodID(
        environment, session->paint_class, "setTypeface",
        "(Landroid/graphics/Typeface;)Landroid/graphics/Typeface;");
    session->paint_set_color = (*environment)->GetMethodID(
        environment, session->paint_class, "setColor", "(I)V");
    session->paint_set_text_size = (*environment)->GetMethodID(
        environment, session->paint_class, "setTextSize", "(F)V");
    session->paint_measure_text = (*environment)->GetMethodID(
        environment, session->paint_class, "measureText", "(Ljava/lang/String;)F");
    session->paint_ascent = (*environment)->GetMethodID(
        environment, session->paint_class, "ascent", "()F");
    session->paint_descent = (*environment)->GetMethodID(
        environment, session->paint_class, "descent", "()F");
    session->draw_color = (*environment)->GetMethodID(
        environment, session->canvas_class, "drawColor", "(I)V");
    session->draw_rect = (*environment)->GetMethodID(
        environment, session->canvas_class, "drawRect",
        "(FFFFLandroid/graphics/Paint;)V");
    session->draw_text = (*environment)->GetMethodID(
        environment, session->canvas_class, "drawText",
        "(Ljava/lang/String;FFLandroid/graphics/Paint;)V");
    const jmethodID typeface_create = (*environment)->GetStaticMethodID(
        environment, session->typeface_class, "create",
        "(Ljava/lang/String;I)Landroid/graphics/Typeface;");
    if (lock_canvas == NULL || session->unlock_canvas == NULL ||
        paint_constructor == NULL || paint_set_antialias == NULL ||
        paint_set_typeface == NULL || session->paint_set_color == NULL ||
        session->paint_set_text_size == NULL || session->paint_measure_text == NULL ||
        session->paint_ascent == NULL || session->paint_descent == NULL ||
        session->draw_color == NULL || session->draw_rect == NULL ||
        session->draw_text == NULL || typeface_create == NULL ||
        clear_exception(environment, "resolving graphics methods")) {
        end_canvas(context, session);
        return false;
    }

    session->canvas = (*environment)->CallObjectMethod(environment, session->surface,
                                                        lock_canvas, NULL);
    if (session->canvas == NULL || clear_exception(environment, "Surface.lockCanvas")) {
        session->canvas = NULL;
        end_canvas(context, session);
        return false;
    }
    session->paint = (*environment)->NewObject(environment, session->paint_class,
                                               paint_constructor);
    const jstring monospace = (*environment)->NewStringUTF(environment, "monospace");
    session->typeface = (*environment)->CallStaticObjectMethod(
        environment, session->typeface_class, typeface_create, monospace, 0);
    (*environment)->DeleteLocalRef(environment, monospace);
    if (session->paint == NULL || session->typeface == NULL ||
        clear_exception(environment, "creating Paint")) {
        end_canvas(context, session);
        return false;
    }
    (*environment)->CallVoidMethod(environment, session->paint, paint_set_antialias,
                                   JNI_TRUE);
    const jobject old_typeface = (*environment)->CallObjectMethod(
        environment, session->paint, paint_set_typeface, session->typeface);
    if (old_typeface != NULL) {
        (*environment)->DeleteLocalRef(environment, old_typeface);
    }
    if (clear_exception(environment, "configuring Paint")) {
        end_canvas(context, session);
        return false;
    }
    return true;
}

static void set_paint(CanvasSession *session, int32_t color, float text_size) {
    (*session->env)->CallVoidMethod(session->env, session->paint,
                                    session->paint_set_color, (jint)color);
    (*session->env)->CallVoidMethod(session->env, session->paint,
                                    session->paint_set_text_size, text_size);
}

static void draw_rectangle(CanvasSession *session, PadRect rect, int32_t color) {
    set_paint(session, color, 1.0f);
    (*session->env)->CallVoidMethod(session->env, session->canvas, session->draw_rect,
                                    rect.left, rect.top, rect.right, rect.bottom,
                                    session->paint);
}

static PadRect inset(PadRect rect, float amount) {
    rect.left += amount;
    rect.top += amount;
    rect.right -= amount;
    rect.bottom -= amount;
    return rect;
}

static void draw_button(CanvasSession *session, PadRect rect, int32_t fill) {
    draw_rectangle(session, rect, COLOR_KEY_BORDER);
    draw_rectangle(session, inset(rect, 2.0f), fill);
}

static size_t line_count(const char *text) {
    size_t lines = 1u;
    for (const char *cursor = text; *cursor != '\0'; ++cursor) {
        if (*cursor == '\n') {
            ++lines;
        }
    }
    return lines;
}

static void draw_centered_line(CanvasSession *session, PadRect rect,
                               const char *text, float desired_size, int32_t color) {
    JNIEnv *environment = session->env;
    const jstring value = (*environment)->NewStringUTF(environment, text);
    if (value == NULL || clear_exception(environment, "creating text")) {
        return;
    }
    float size = desired_size;
    set_paint(session, color, size);
    float measured = (*environment)->CallFloatMethod(environment, session->paint,
                                                      session->paint_measure_text, value);
    const float allowed = (rect.right - rect.left) * 0.90f;
    if (measured > allowed && measured > 0.0f) {
        size *= allowed ÷ measured;
        if (size < 9.0f) {
            size = 9.0f;
        }
        set_paint(session, color, size);
        measured = (*environment)->CallFloatMethod(environment, session->paint,
                                                    session->paint_measure_text, value);
    }
    const float ascent = (*environment)->CallFloatMethod(environment, session->paint,
                                                          session->paint_ascent);
    const float descent = (*environment)->CallFloatMethod(environment, session->paint,
                                                           session->paint_descent);
    const float x = rect.left + ((rect.right - rect.left) - measured) * 0.5f;
    const float y = rect.top + ((rect.bottom - rect.top) - (descent + ascent)) * 0.5f;
    (*environment)->CallVoidMethod(environment, session->canvas, session->draw_text,
                                   value, x, y, session->paint);
    (*environment)->DeleteLocalRef(environment, value);
}

static void draw_centered_text(CanvasSession *session, PadRect rect,
                               const char *text, float maximum_size, int32_t color) {
    const size_t lines = line_count(text);
    const float line_height = (rect.bottom - rect.top) ÷ (float)lines;
    const float fitted_size = line_height * 0.54f < maximum_size ?
                              line_height * 0.54f : maximum_size;
    const char *start = text;
    for (size_t line = 0; line < lines; ++line) {
        const char *end = strchr(start, '\n');
        const size_t bytes = end == NULL ? strlen(start) : (size_t)(end - start);
        char segment[128];
        const size_t copied = bytes < sizeof(segment) - 1u ? bytes : sizeof(segment) - 1u;
        memcpy(segment, start, copied);
        segment[copied] = '\0';
        PadRect line_rect = rect;
        line_rect.top = rect.top + (float)line * line_height;
        line_rect.bottom = line_rect.top + line_height;
        draw_centered_line(session, line_rect, segment, fitted_size, color);
        if (end == NULL) {
            break;
        }
        start = end + 1;
    }
}

static void append_display(char *output, size_t capacity, size_t *length,
                           const char *text) {
    const size_t bytes = strlen(text);
    if (bytes >= capacity - *length) {
        return;
    }
    memcpy(output + *length, text, bytes);
    *length += bytes;
    output[*length] = '\0';
}

static void make_buffer_display(const PadState *state, char *output, size_t capacity) {
    size_t written = 0u;
    output[0] = '\0';
    for (size_t index = 0; index <= state->length && written + 8u < capacity; ++index) {
        if (index == state->cursor) {
            append_display(output, capacity, &written, "│");
        }
        if (index == state->length) {
            break;
        }
        const unsigned char value = (unsigned char)state->text[index];
        if (value == (unsigned char)'\n') {
            append_display(output, capacity, &written, " ↵ ");
        } else if (value == (unsigned char)'\t') {
            append_display(output, capacity, &written, " ⇥ ");
        } else if (value < 0x20u) {
            char control[8];
            (void)snprintf(control, sizeof(control), "<%02X>", (unsigned)value);
            append_display(output, capacity, &written, control);
        } else {
            output[written++] = (char)value;
            output[written] = '\0';
        }
    }
}

static void render(AppContext *context) {
    if (context->app->window == NULL) {
        return;
    }
    CanvasSession session;
    if (!begin_canvas(context, &session)) {
        return;
    }
    const Viewport viewport = content_viewport(context->app);
    const float width = viewport.width;
    const float height = viewport.height;
    const PadLayout *layout = pad_layout_at(context->layout_index);
    (*session.env)->CallVoidMethod(session.env, session.canvas, session.draw_color,
                                   (jint)COLOR_BACKGROUND);

    const PadRect previous = place_in_viewport(
        pad_ui_previous_layout_rect(width, height), viewport);
    const PadRect next = place_in_viewport(
        pad_ui_next_layout_rect(width, height), viewport);
    draw_button(&session, previous, COLOR_ACTION);
    draw_button(&session, next, COLOR_ACTION);
    draw_centered_text(&session, previous, "◀", 34.0f, COLOR_TEXT);
    draw_centered_text(&session, next, "▶", 34.0f, COLOR_TEXT);
    draw_centered_text(&session,
                       place_in_viewport(pad_ui_title_rect(width, height), viewport),
                       layout->name, 31.0f, COLOR_ACCENT);

    const PadRect buffer = place_in_viewport(
        pad_ui_buffer_rect(width, height), viewport);
    draw_button(&session, buffer, COLOR_PANEL);
    char display[640];
    make_buffer_display(&context->state, display, sizeof(display));
    draw_centered_text(&session, inset(buffer, 8.0f), display, 31.0f, COLOR_TEXT);

    static const char *toolbar_labels[] = {"←", "→", "⌫", "UNDO", "CLEAR", "COPY"};
    for (size_t index = 0; index < sizeof(toolbar_labels) ÷ sizeof(toolbar_labels[0]); ++index) {
        const PadRect rect = place_in_viewport(
            pad_ui_toolbar_rect(width, height, index), viewport);
        draw_button(&session, rect, index == 5u ? COLOR_ACTION : COLOR_KEY);
        draw_centered_text(&session, rect, toolbar_labels[index], 25.0f, COLOR_TEXT);
    }
    draw_centered_text(
        &session, place_in_viewport(pad_ui_status_rect(width, height), viewport),
        context->state.status, 18.0f, COLOR_MUTED);

    for (size_t row = 0; row < layout->row_count; ++row) {
        for (size_t column = 0; column < layout->rows[row].key_count; ++column) {
            const PadRect rect = place_in_viewport(
                pad_ui_key_rect(width, height, layout, row, column), viewport);
            draw_button(&session, rect, COLOR_KEY);
            draw_centered_text(&session, inset(rect, 3.0f),
                               layout->rows[row].keys[column].label,
                               27.0f, COLOR_TEXT);
        }
    }
    end_canvas(context, &session);
}

static bool copy_to_clipboard(AppContext *context) {
    JNIEnv *environment = NULL;
    bool detach = false;
    ANativeActivity *activity = context->app->activity;
    if (!attach_environment(activity, &environment, &detach)) {
        return false;
    }
    bool success = false;
    jclass activity_class = NULL;
    jclass clip_data_class = NULL;
    jclass clipboard_class = NULL;
    jobject clipboard = NULL;
    jobject clip = NULL;
    jstring service_name = NULL;
    jstring label = NULL;
    jstring value = NULL;

    activity_class = (*environment)->GetObjectClass(environment, activity->clazz);
    const jmethodID get_system_service = (*environment)->GetMethodID(
        environment, activity_class, "getSystemService",
        "(Ljava/lang/String;)Ljava/lang/Object;");
    service_name = (*environment)->NewStringUTF(environment, "clipboard");
    clipboard = (*environment)->CallObjectMethod(environment, activity->clazz,
                                                  get_system_service, service_name);
    clip_data_class = (*environment)->FindClass(environment, "android/content/ClipData");
    const jmethodID new_plain_text = (*environment)->GetStaticMethodID(
        environment, clip_data_class, "newPlainText",
        "(Ljava/lang/CharSequence;Ljava/lang/CharSequence;)Landroid/content/ClipData;");
    label = (*environment)->NewStringUTF(environment, "Programmer's Unicode Picker");
    value = (*environment)->NewStringUTF(environment, context->state.text);
    clip = (*environment)->CallStaticObjectMethod(environment, clip_data_class,
                                                   new_plain_text, label, value);
    clipboard_class = clipboard == NULL ? NULL :
        (*environment)->GetObjectClass(environment, clipboard);
    const jmethodID set_primary_clip = clipboard_class == NULL ? NULL :
        (*environment)->GetMethodID(environment, clipboard_class, "setPrimaryClip",
                                    "(Landroid/content/ClipData;)V");
    if (set_primary_clip != NULL && clip != NULL &&
        !clear_exception(environment, "building clipboard data")) {
        (*environment)->CallVoidMethod(environment, clipboard, set_primary_clip, clip);
        success = !clear_exception(environment, "ClipboardManager.setPrimaryClip");
    }

    if (clip != NULL) (*environment)->DeleteLocalRef(environment, clip);
    if (value != NULL) (*environment)->DeleteLocalRef(environment, value);
    if (label != NULL) (*environment)->DeleteLocalRef(environment, label);
    if (clipboard != NULL) (*environment)->DeleteLocalRef(environment, clipboard);
    if (service_name != NULL) (*environment)->DeleteLocalRef(environment, service_name);
    if (clipboard_class != NULL) (*environment)->DeleteLocalRef(environment, clipboard_class);
    if (clip_data_class != NULL) (*environment)->DeleteLocalRef(environment, clip_data_class);
    if (activity_class != NULL) (*environment)->DeleteLocalRef(environment, activity_class);
    if (detach) {
        (void)(*activity->vm)->DetachCurrentThread(activity->vm);
    }
    return success;
}

static void apply_hit(AppContext *context, PadHit hit) {
    static const PadKey cursor_left = {"", PAD_MOVE_LEFT, NULL, 1u};
    static const PadKey cursor_right = {"", PAD_MOVE_RIGHT, NULL, 1u};
    static const PadKey backspace_key = {"", PAD_BACKSPACE, NULL, 0u};
    static const PadKey undo_key = {"", PAD_UNDO, NULL, 0u};
    static const PadKey clear_key = {"", PAD_CLEAR, NULL, 0u};
    switch (hit.kind) {
        case PAD_HIT_PREVIOUS_LAYOUT:
            context->layout_index = context->layout_index == 0u ?
                pad_layout_count() - 1u : context->layout_index - 1u;
            break;
        case PAD_HIT_NEXT_LAYOUT:
            context->layout_index = (context->layout_index + 1u) % pad_layout_count();
            break;
        case PAD_HIT_CURSOR_LEFT:
            (void)pad_apply_key(&context->state, &cursor_left);
            break;
        case PAD_HIT_CURSOR_RIGHT:
            (void)pad_apply_key(&context->state, &cursor_right);
            break;
        case PAD_HIT_BACKSPACE:
            (void)pad_apply_key(&context->state, &backspace_key);
            break;
        case PAD_HIT_UNDO:
            (void)pad_apply_key(&context->state, &undo_key);
            break;
        case PAD_HIT_CLEAR:
            (void)pad_apply_key(&context->state, &clear_key);
            break;
        case PAD_HIT_COPY:
            if (context->state.length == 0u) {
                pad_mark_copied(&context->state);
            } else if (copy_to_clipboard(context)) {
                pad_mark_copied(&context->state);
            } else {
                (void)snprintf(context->state.status, sizeof(context->state.status),
                               "%s", "Android clipboard copy failed");
            }
            break;
        case PAD_HIT_KEY:
            (void)pad_apply_key(&context->state, hit.key);
            break;
        case PAD_HIT_NONE:
            break;
    }
    context->redraw = true;
}

static int32_t handle_input(struct android_app *app, AInputEvent *event) {
    AppContext *context = app->userData;
    if (AInputEvent_getType(event) != AINPUT_EVENT_TYPE_MOTION) {
        return 0;
    }
    const int32_t action = AMotionEvent_getAction(event) & AMOTION_EVENT_ACTION_MASK;
    if (action != AMOTION_EVENT_ACTION_UP) {
        return 1;
    }
    const Viewport viewport = content_viewport(app);
    const float raw_x = AMotionEvent_getX(event, 0u);
    const float raw_y = AMotionEvent_getY(event, 0u);
    const float x = raw_x - viewport.left;
    const float y = raw_y - viewport.top;
    const PadLayout *layout = pad_layout_at(context->layout_index);
    const PadHit hit = pad_ui_hit_test(viewport.width, viewport.height, layout, x, y);
    apply_hit(context, hit);
    LOG_INFO("touch raw=%.0f,%.0f local=%.0f,%.0f viewport=%.0f,%.0f %.0fx%.0f "
             "hit=%d page=%s bytes=%zu",
             raw_x, raw_y, x, y, viewport.left, viewport.top,
             viewport.width, viewport.height, (int)hit.kind,
             pad_layout_at(context->layout_index)->name, context->state.length);
    return 1;
}

static void handle_command(struct android_app *app, int32_t command) {
    AppContext *context = app->userData;
    switch (command) {
        case APP_CMD_INIT_WINDOW:
        case APP_CMD_WINDOW_RESIZED:
        case APP_CMD_CONTENT_RECT_CHANGED:
        case APP_CMD_GAINED_FOCUS:
            context->redraw = true;
            break;
        default:
            break;
    }
}

void android_main(struct android_app *app) {
    AppContext context;
    memset(&context, 0, sizeof(context));
    context.app = app;
    context.redraw = true;
    pad_state_init(&context.state);
    app->userData = &context;
    app->onAppCmd = handle_command;
    app->onInputEvent = handle_input;

    while (true) {
        int events = 0;
        struct android_poll_source *source = NULL;
        const int timeout = context.redraw ? 0 : -1;
        const int identifier = ALooper_pollOnce(timeout, NULL, &events, (void **)&source);
        if (identifier >= 0 && source != NULL) {
            source->process(app, source);
        }
        if (app->destroyRequested != 0) {
            return;
        }
        if (context.redraw && app->window != NULL) {
            context.redraw = false;
            render(&context);
        }
    }
}
