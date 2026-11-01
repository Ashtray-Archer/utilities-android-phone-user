#include "pasteboard_model.h"

#include <android/input.h>
#include <android/log.h>
#include <android/native_activity.h>
#include <android/native_window.h>
#include <android/native_window_jni.h>
#include <android_native_app_glue.h>
#include <jni.h>

#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#define LOG_TAG "Pasteboard"
#define LOG_ERROR(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#define LOG_INFO(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)

static const int32_t COLOR_BACKGROUND = (int32_t)0xFF0F1115u;
static const int32_t COLOR_CARD = (int32_t)0xFF1A1E24u;
static const int32_t COLOR_CARD_EMPTY = (int32_t)0xFF14171Cu;
static const int32_t COLOR_BORDER = (int32_t)0xFF343B46u;
static const int32_t COLOR_TEXT = (int32_t)0xFFF2F4F7u;
static const int32_t COLOR_MUTED = (int32_t)0xFF8A939Fu;
static const int32_t COLOR_DOT = (int32_t)0xFF56606Du;
static const int32_t COLOR_DOT_ACTIVE = (int32_t)0xFFE7EBF0u;

typedef struct {
    float left;
    float top;
    float right;
    float bottom;
} RectF;

typedef struct {
    float left;
    float top;
    float width;
    float height;
} Viewport;

typedef struct {
    struct android_app *app;
    PasteboardState state;
    bool redraw;
    bool loaded;
    float down_x;
    float down_y;
    int down_card;
    char storage_path[512];
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
    jclass string_class;
    jmethodID unlock_canvas;
    jmethodID draw_color;
    jmethodID draw_round_rect;
    jmethodID draw_text;
    jmethodID draw_circle;
    jmethodID paint_set_color;
    jmethodID paint_set_text_size;
    jmethodID paint_break_text;
    jmethodID string_substring;
    jmethodID string_bytes_constructor;
} CanvasSession;

static float minimum(float a, float b) {
    return a < b ? a : b;
}

static float maximum(float a, float b) {
    return a > b ? a : b;
}

static float clampf(float value, float low, float high) {
    return maximum(low, minimum(value, high));
}

static bool point_in_rect(RectF rect, float x, float y) {
    return x >= rect.left && x <= rect.right && y >= rect.top && y <= rect.bottom;
}

static Viewport content_viewport(const struct android_app *app) {
    const float window_width = (float)ANativeWindow_getWidth(app->window);
    const float window_height = (float)ANativeWindow_getHeight(app->window);
    const int32_t content_width = app->contentRect.right - app->contentRect.left;
    const int32_t content_height = app->contentRect.bottom - app->contentRect.top;
    if (content_width <= 0 || content_height <= 0) {
        const Viewport full = {0.0f, 0.0f, window_width, window_height};
        return full;
    }
    const Viewport content = {
        (float)app->contentRect.left,
        (float)app->contentRect.top,
        (float)content_width,
        (float)content_height,
    };
    return content;
}

static float header_height(float height) {
    return clampf(height * 0.058f, 48.0f, 72.0f);
}

static RectF card_rect(float width, float height, size_t index) {
    const float side = clampf(width * 0.025f, 10.0f, 18.0f);
    const float gap_x = clampf(width * 0.018f, 8.0f, 14.0f);
    const float gap_y = clampf(height * 0.008f, 7.0f, 13.0f);
    const float top = header_height(height) + gap_y;
    const float bottom = height - gap_y;
    const float rows = (float)(PASTEBOARD_SLOT_COUNT / PASTEBOARD_COLUMNS);
    const float card_width = (width - (2.0f * side) - gap_x) / 2.0f;
    const float card_height = (bottom - top - ((rows - 1.0f) * gap_y)) / rows;
    const size_t row = index / PASTEBOARD_COLUMNS;
    const size_t column = index % PASTEBOARD_COLUMNS;
    const float left = side + (float)column * (card_width + gap_x);
    const float y = top + (float)row * (card_height + gap_y);
    const RectF rect = {left, y, left + card_width, y + card_height};
    return rect;
}

static int card_at(float width, float height, float x, float y) {
    for (size_t index = 0u; index < PASTEBOARD_SLOT_COUNT; ++index) {
        if (point_in_rect(card_rect(width, height, index), x, y)) {
            return (int)index;
        }
    }
    return -1;
}

static bool attach_environment(ANativeActivity *activity, JNIEnv **environment, bool *detach) {
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

static jstring new_java_string_utf8_with_class(JNIEnv *environment, jclass string_class,
                                                jmethodID constructor, const char *text) {
    const size_t bytes = strlen(text);
    if (bytes > (size_t)INT32_MAX) {
        return NULL;
    }
    jbyteArray array = (*environment)->NewByteArray(environment, (jsize)bytes);
    if (array == NULL || clear_exception(environment, "allocating UTF-8 byte array")) {
        return NULL;
    }
    if (bytes > 0u) {
        (*environment)->SetByteArrayRegion(environment, array, 0, (jsize)bytes,
                                           (const jbyte *)text);
    }
    const jstring charset = (*environment)->NewStringUTF(environment, "UTF-8");
    jstring result = NULL;
    if (charset != NULL && !clear_exception(environment, "creating UTF-8 charset name")) {
        result = (jstring)(*environment)->NewObject(environment, string_class, constructor,
                                                    array, charset);
        (void)clear_exception(environment, "constructing Java UTF-8 string");
    }
    if (charset != NULL) {
        (*environment)->DeleteLocalRef(environment, charset);
    }
    (*environment)->DeleteLocalRef(environment, array);
    return result;
}

static jstring new_java_string_utf8(JNIEnv *environment, const char *text) {
    jclass string_class = (*environment)->FindClass(environment, "java/lang/String");
    if (string_class == NULL || clear_exception(environment, "finding java.lang.String")) {
        return NULL;
    }
    const jmethodID constructor = (*environment)->GetMethodID(
        environment, string_class, "<init>", "([BLjava/lang/String;)V");
    jstring result = NULL;
    if (constructor != NULL && !clear_exception(environment, "finding UTF-8 String constructor")) {
        result = new_java_string_utf8_with_class(environment, string_class, constructor, text);
    }
    (*environment)->DeleteLocalRef(environment, string_class);
    return result;
}

static void end_canvas(AppContext *context, CanvasSession *session) {
    JNIEnv *environment = session->env;
    if (session->canvas != NULL && session->surface != NULL && session->unlock_canvas != NULL) {
        (*environment)->CallVoidMethod(environment, session->surface,
                                       session->unlock_canvas, session->canvas);
        (void)clear_exception(environment, "Surface.unlockCanvasAndPost");
    }
    if (session->typeface != NULL) (*environment)->DeleteLocalRef(environment, session->typeface);
    if (session->paint != NULL) (*environment)->DeleteLocalRef(environment, session->paint);
    if (session->canvas != NULL) (*environment)->DeleteLocalRef(environment, session->canvas);
    if (session->surface != NULL) (*environment)->DeleteLocalRef(environment, session->surface);
    if (session->string_class != NULL) (*environment)->DeleteLocalRef(environment, session->string_class);
    if (session->typeface_class != NULL) (*environment)->DeleteLocalRef(environment, session->typeface_class);
    if (session->paint_class != NULL) (*environment)->DeleteLocalRef(environment, session->paint_class);
    if (session->canvas_class != NULL) (*environment)->DeleteLocalRef(environment, session->canvas_class);
    if (session->surface_class != NULL) (*environment)->DeleteLocalRef(environment, session->surface_class);
    if (session->detach) {
        (void)(*context->app->activity->vm)->DetachCurrentThread(context->app->activity->vm);
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
    session->string_class = (*environment)->FindClass(environment, "java/lang/String");
    if (session->surface_class == NULL || session->canvas_class == NULL ||
        session->paint_class == NULL || session->typeface_class == NULL ||
        session->string_class == NULL || clear_exception(environment, "finding graphics classes")) {
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
    session->paint_break_text = (*environment)->GetMethodID(
        environment, session->paint_class, "breakText", "(Ljava/lang/String;ZF[F)I");
    session->draw_color = (*environment)->GetMethodID(
        environment, session->canvas_class, "drawColor", "(I)V");
    session->draw_round_rect = (*environment)->GetMethodID(
        environment, session->canvas_class, "drawRoundRect",
        "(FFFFFFLandroid/graphics/Paint;)V");
    session->draw_text = (*environment)->GetMethodID(
        environment, session->canvas_class, "drawText",
        "(Ljava/lang/String;FFLandroid/graphics/Paint;)V");
    session->draw_circle = (*environment)->GetMethodID(
        environment, session->canvas_class, "drawCircle",
        "(FFFLandroid/graphics/Paint;)V");
    const jmethodID typeface_create = (*environment)->GetStaticMethodID(
        environment, session->typeface_class, "create",
        "(Ljava/lang/String;I)Landroid/graphics/Typeface;");
    session->string_substring = (*environment)->GetMethodID(
        environment, session->string_class, "substring", "(II)Ljava/lang/String;");
    session->string_bytes_constructor = (*environment)->GetMethodID(
        environment, session->string_class, "<init>", "([BLjava/lang/String;)V");

    if (lock_canvas == NULL || session->unlock_canvas == NULL ||
        paint_constructor == NULL || paint_set_antialias == NULL ||
        paint_set_typeface == NULL || session->paint_set_color == NULL ||
        session->paint_set_text_size == NULL || session->paint_break_text == NULL ||
        session->draw_color == NULL || session->draw_round_rect == NULL ||
        session->draw_text == NULL || session->draw_circle == NULL ||
        typeface_create == NULL || session->string_substring == NULL ||
        session->string_bytes_constructor == NULL ||
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
    const jstring family = (*environment)->NewStringUTF(environment, "sans-serif");
    session->typeface = family == NULL ? NULL : (*environment)->CallStaticObjectMethod(
        environment, session->typeface_class, typeface_create, family, 0);
    if (family != NULL) {
        (*environment)->DeleteLocalRef(environment, family);
    }
    if (session->paint == NULL || session->typeface == NULL ||
        clear_exception(environment, "creating Paint")) {
        end_canvas(context, session);
        return false;
    }
    (*environment)->CallVoidMethod(environment, session->paint, paint_set_antialias, JNI_TRUE);
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

static void draw_round_rect(CanvasSession *session, RectF rect, float radius, int32_t color) {
    set_paint(session, color, 1.0f);
    (*session->env)->CallVoidMethod(session->env, session->canvas, session->draw_round_rect,
                                    rect.left, rect.top, rect.right, rect.bottom,
                                    radius, radius, session->paint);
}

static void draw_circle(CanvasSession *session, float x, float y, float radius, int32_t color) {
    set_paint(session, color, 1.0f);
    (*session->env)->CallVoidMethod(session->env, session->canvas, session->draw_circle,
                                    x, y, radius, session->paint);
}

static void draw_utf8(CanvasSession *session, const char *text, float x, float y,
                      float size, int32_t color) {
    JNIEnv *environment = session->env;
    jstring value = new_java_string_utf8_with_class(environment, session->string_class,
                                                     session->string_bytes_constructor, text);
    if (value == NULL) {
        return;
    }
    set_paint(session, color, size);
    (*environment)->CallVoidMethod(environment, session->canvas, session->draw_text,
                                   value, x, y, session->paint);
    (*environment)->DeleteLocalRef(environment, value);
}

static size_t make_preview(const char *text, char *output, size_t capacity) {
    if (capacity == 0u) {
        return 0u;
    }
    size_t written = 0u;
    bool previous_space = false;
    for (const unsigned char *cursor = (const unsigned char *)text;
         *cursor != 0u && written + 1u < capacity; ++cursor) {
        unsigned char value = *cursor;
        if (value == (unsigned char)'\n' || value == (unsigned char)'\r' ||
            value == (unsigned char)'\t') {
            value = (unsigned char)' ';
        }
        if (value == (unsigned char)' ') {
            if (previous_space) {
                continue;
            }
            previous_space = true;
        } else {
            previous_space = false;
        }
        output[written++] = (char)value;
    }
    output[written] = '\0';
    return written;
}

static void draw_entry_text(CanvasSession *session, RectF rect, const char *text) {
    char preview[768];
    const size_t bytes = make_preview(text, preview, sizeof(preview));
    if (bytes == 0u) {
        return;
    }
    JNIEnv *environment = session->env;
    jstring value = new_java_string_utf8_with_class(environment, session->string_class,
                                                     session->string_bytes_constructor, preview);
    if (value == NULL) {
        return;
    }
    const jsize length = (*environment)->GetStringLength(environment, value);
    const float card_height = rect.bottom - rect.top;
    float text_size = 0.0f;
    if (bytes <= 54u) {
        text_size = clampf(card_height * 0.24f, 22.0f, 34.0f);
    } else if (bytes <= 160u) {
        text_size = clampf(card_height * 0.20f, 19.0f, 29.0f);
    } else {
        text_size = clampf(card_height * 0.17f, 17.0f, 24.0f);
    }
    set_paint(session, COLOR_TEXT, text_size);

    const float padding = clampf((rect.right - rect.left) * 0.045f, 10.0f, 18.0f);
    const float available_width = (rect.right - rect.left) - (2.0f * padding);
    const float line_height = text_size * 1.20f;
    const int max_lines = card_height >= 145.0f ? 3 : 2;
    jsize offset = 0;
    bool truncated = false;
    jfloatArray measured = (*environment)->NewFloatArray(environment, 1);
    if (measured == NULL || clear_exception(environment, "allocating text measurement")) {
        if (measured != NULL) (*environment)->DeleteLocalRef(environment, measured);
        (*environment)->DeleteLocalRef(environment, value);
        return;
    }

    float y = rect.top + padding + text_size + 12.0f;
    for (int line = 0; line < max_lines && offset < length; ++line) {
        jobject remainder = (*environment)->CallObjectMethod(environment, value,
                                                              session->string_substring,
                                                              offset, length);
        if (remainder == NULL || clear_exception(environment, "taking text remainder")) {
            if (remainder != NULL) (*environment)->DeleteLocalRef(environment, remainder);
            break;
        }
        const jint fit = (*environment)->CallIntMethod(environment, session->paint,
                                                       session->paint_break_text,
                                                       remainder, JNI_TRUE,
                                                       available_width, measured);
        (*environment)->DeleteLocalRef(environment, remainder);
        if (clear_exception(environment, "breaking text") || fit <= 0) {
            break;
        }
        jsize end = offset + fit;
        if (end > length) {
            end = length;
        }
        jobject segment = (*environment)->CallObjectMethod(environment, value,
                                                            session->string_substring,
                                                            offset, end);
        if (segment == NULL || clear_exception(environment, "taking text line")) {
            if (segment != NULL) (*environment)->DeleteLocalRef(environment, segment);
            break;
        }
        (*environment)->CallVoidMethod(environment, session->canvas, session->draw_text,
                                       segment, rect.left + padding, y, session->paint);
        (*environment)->DeleteLocalRef(environment, segment);
        offset = end;
        y += line_height;
    }
    truncated = offset < length || strlen(text) >= sizeof(preview) - 1u;
    if (truncated) {
        draw_utf8(session, "…", rect.right - padding - text_size, rect.bottom - padding,
                  text_size, COLOR_MUTED);
    }
    (*environment)->DeleteLocalRef(environment, measured);
    (*environment)->DeleteLocalRef(environment, value);
}

static void draw_board_dots(CanvasSession *session, float width, float height,
                            size_t active_board) {
    const float y = header_height(height) * 0.48f;
    const float spacing = clampf(width * 0.038f, 18.0f, 28.0f);
    const float center = width * 0.5f;
    for (size_t index = 0u; index < PASTEBOARD_BOARD_COUNT; ++index) {
        const float x = center + ((float)index - 1.0f) * spacing;
        const bool active = index == active_board;
        draw_circle(session, x, y, active ? 5.5f : 4.0f,
                    active ? COLOR_DOT_ACTIVE : COLOR_DOT);
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
    (*session.env)->CallVoidMethod(session.env, session.canvas, session.draw_color,
                                   (jint)COLOR_BACKGROUND);

    draw_board_dots(&session, viewport.width, viewport.height, context->state.current_board);
    const PasteboardBoard *board = &context->state.boards[context->state.current_board];
    for (size_t index = 0u; index < PASTEBOARD_SLOT_COUNT; ++index) {
        RectF rect = card_rect(viewport.width, viewport.height, index);
        rect.left += viewport.left;
        rect.right += viewport.left;
        rect.top += viewport.top;
        rect.bottom += viewport.top;
        const bool occupied = index < board->count;
        draw_round_rect(&session, rect, 18.0f, COLOR_BORDER);
        RectF inner = {rect.left + 1.8f, rect.top + 1.8f,
                       rect.right - 1.8f, rect.bottom - 1.8f};
        draw_round_rect(&session, inner, 16.5f, occupied ? COLOR_CARD : COLOR_CARD_EMPTY);

        char slot[8];
        (void)snprintf(slot, sizeof(slot), "%zu", index + 1u);
        draw_utf8(&session, slot, rect.left + 10.0f, rect.top + 19.0f,
                  13.0f, COLOR_MUTED);
        if (occupied) {
            draw_entry_text(&session, rect, board->entries[index].text);
        }
    }
    end_canvas(context, &session);
}

static size_t valid_utf8_prefix(const char *text, size_t length) {
    if (length == 0u) {
        return 0u;
    }
    size_t start = length - 1u;
    while (start > 0u && (((unsigned char)text[start] & 0xC0u) == 0x80u)) {
        --start;
    }
    const unsigned char lead = (unsigned char)text[start];
    size_t expected = 1u;
    if ((lead & 0xE0u) == 0xC0u) expected = 2u;
    else if ((lead & 0xF0u) == 0xE0u) expected = 3u;
    else if ((lead & 0xF8u) == 0xF0u) expected = 4u;
    const size_t actual = length - start;
    return actual < expected ? start : length;
}

static bool read_current_clipboard(AppContext *context, char *output, size_t capacity) {
    if (capacity < 2u) {
        return false;
    }
    output[0] = '\0';
    ANativeActivity *activity = context->app->activity;
    JNIEnv *environment = NULL;
    bool detach = false;
    if (!attach_environment(activity, &environment, &detach)) {
        return false;
    }
    bool success = false;
    jobject clipboard = NULL;
    jobject clip = NULL;
    jobject item = NULL;
    jobject chars = NULL;
    jstring string = NULL;
    jbyteArray bytes = NULL;
    jstring service_name = NULL;
    jstring charset = NULL;
    jclass activity_class = NULL;
    jclass clipboard_class = NULL;
    jclass clip_class = NULL;
    jclass item_class = NULL;
    jclass chars_class = NULL;
    jclass string_class = NULL;

    activity_class = (*environment)->GetObjectClass(environment, activity->clazz);
    const jmethodID get_system_service = activity_class == NULL ? NULL :
        (*environment)->GetMethodID(environment, activity_class, "getSystemService",
                                    "(Ljava/lang/String;)Ljava/lang/Object;");
    service_name = (*environment)->NewStringUTF(environment, "clipboard");
    if (get_system_service != NULL && service_name != NULL) {
        clipboard = (*environment)->CallObjectMethod(environment, activity->clazz,
                                                      get_system_service, service_name);
    }
    clipboard_class = clipboard == NULL ? NULL : (*environment)->GetObjectClass(environment, clipboard);
    const jmethodID get_primary_clip = clipboard_class == NULL ? NULL :
        (*environment)->GetMethodID(environment, clipboard_class, "getPrimaryClip",
                                    "()Landroid/content/ClipData;");
    if (get_primary_clip != NULL) {
        clip = (*environment)->CallObjectMethod(environment, clipboard, get_primary_clip);
    }
    clip_class = clip == NULL ? NULL : (*environment)->GetObjectClass(environment, clip);
    const jmethodID get_item_count = clip_class == NULL ? NULL :
        (*environment)->GetMethodID(environment, clip_class, "getItemCount", "()I");
    const jmethodID get_item_at = clip_class == NULL ? NULL :
        (*environment)->GetMethodID(environment, clip_class, "getItemAt",
                                    "(I)Landroid/content/ClipData$Item;");
    if (get_item_count != NULL && get_item_at != NULL) {
        const jint count = (*environment)->CallIntMethod(environment, clip, get_item_count);
        if (count > 0) {
            item = (*environment)->CallObjectMethod(environment, clip, get_item_at, 0);
        }
    }
    item_class = item == NULL ? NULL : (*environment)->GetObjectClass(environment, item);
    const jmethodID get_text = item_class == NULL ? NULL :
        (*environment)->GetMethodID(environment, item_class, "getText",
                                    "()Ljava/lang/CharSequence;");
    const jmethodID coerce_to_text = item_class == NULL ? NULL :
        (*environment)->GetMethodID(environment, item_class, "coerceToText",
                                    "(Landroid/content/Context;)Ljava/lang/CharSequence;");
    if (get_text != NULL) {
        chars = (*environment)->CallObjectMethod(environment, item, get_text);
    }
    if (chars == NULL && coerce_to_text != NULL) {
        chars = (*environment)->CallObjectMethod(environment, item, coerce_to_text, activity->clazz);
    }
    chars_class = chars == NULL ? NULL : (*environment)->GetObjectClass(environment, chars);
    const jmethodID to_string = chars_class == NULL ? NULL :
        (*environment)->GetMethodID(environment, chars_class, "toString",
                                    "()Ljava/lang/String;");
    if (to_string != NULL) {
        string = (jstring)(*environment)->CallObjectMethod(environment, chars, to_string);
    }
    string_class = string == NULL ? NULL : (*environment)->FindClass(environment, "java/lang/String");
    const jmethodID get_bytes = string_class == NULL ? NULL :
        (*environment)->GetMethodID(environment, string_class, "getBytes",
                                    "(Ljava/lang/String;)[B");
    charset = (*environment)->NewStringUTF(environment, "UTF-8");
    if (get_bytes != NULL && charset != NULL) {
        bytes = (jbyteArray)(*environment)->CallObjectMethod(environment, string, get_bytes, charset);
    }
    if (bytes != NULL && !clear_exception(environment, "reading clipboard text")) {
        const jsize byte_count = (*environment)->GetArrayLength(environment, bytes);
        size_t copied = (size_t)byte_count;
        if (copied >= capacity) {
            copied = capacity - 1u;
        }
        if (copied > 0u) {
            (*environment)->GetByteArrayRegion(environment, bytes, 0, (jsize)copied,
                                                (jbyte *)output);
            if ((size_t)byte_count >= capacity) {
                copied = valid_utf8_prefix(output, copied);
            }
        }
        output[copied] = '\0';
        success = copied > 0u && !clear_exception(environment, "copying clipboard bytes");
    } else {
        (void)clear_exception(environment, "reading clipboard");
    }

    if (charset != NULL) (*environment)->DeleteLocalRef(environment, charset);
    if (bytes != NULL) (*environment)->DeleteLocalRef(environment, bytes);
    if (string_class != NULL) (*environment)->DeleteLocalRef(environment, string_class);
    if (string != NULL) (*environment)->DeleteLocalRef(environment, string);
    if (chars_class != NULL) (*environment)->DeleteLocalRef(environment, chars_class);
    if (chars != NULL) (*environment)->DeleteLocalRef(environment, chars);
    if (item_class != NULL) (*environment)->DeleteLocalRef(environment, item_class);
    if (item != NULL) (*environment)->DeleteLocalRef(environment, item);
    if (clip_class != NULL) (*environment)->DeleteLocalRef(environment, clip_class);
    if (clip != NULL) (*environment)->DeleteLocalRef(environment, clip);
    if (clipboard_class != NULL) (*environment)->DeleteLocalRef(environment, clipboard_class);
    if (clipboard != NULL) (*environment)->DeleteLocalRef(environment, clipboard);
    if (service_name != NULL) (*environment)->DeleteLocalRef(environment, service_name);
    if (activity_class != NULL) (*environment)->DeleteLocalRef(environment, activity_class);
    if (detach) {
        (void)(*activity->vm)->DetachCurrentThread(activity->vm);
    }
    return success;
}

static bool copy_to_clipboard(AppContext *context, const char *text) {
    ANativeActivity *activity = context->app->activity;
    JNIEnv *environment = NULL;
    bool detach = false;
    if (!attach_environment(activity, &environment, &detach)) {
        return false;
    }
    bool success = false;
    jclass activity_class = (*environment)->GetObjectClass(environment, activity->clazz);
    const jmethodID get_system_service = activity_class == NULL ? NULL :
        (*environment)->GetMethodID(environment, activity_class, "getSystemService",
                                    "(Ljava/lang/String;)Ljava/lang/Object;");
    jstring service_name = (*environment)->NewStringUTF(environment, "clipboard");
    jobject clipboard = (get_system_service == NULL || service_name == NULL) ? NULL :
        (*environment)->CallObjectMethod(environment, activity->clazz,
                                          get_system_service, service_name);
    jclass clip_data_class = (*environment)->FindClass(environment, "android/content/ClipData");
    const jmethodID new_plain_text = clip_data_class == NULL ? NULL :
        (*environment)->GetStaticMethodID(environment, clip_data_class, "newPlainText",
            "(Ljava/lang/CharSequence;Ljava/lang/CharSequence;)Landroid/content/ClipData;");
    jstring label = (*environment)->NewStringUTF(environment, "Pasteboard");
    jstring value = new_java_string_utf8(environment, text);
    jobject clip = (new_plain_text == NULL || label == NULL || value == NULL) ? NULL :
        (*environment)->CallStaticObjectMethod(environment, clip_data_class,
                                               new_plain_text, label, value);
    jclass clipboard_class = clipboard == NULL ? NULL :
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
    if (clipboard_class != NULL) (*environment)->DeleteLocalRef(environment, clipboard_class);
    if (clip_data_class != NULL) (*environment)->DeleteLocalRef(environment, clip_data_class);
    if (clipboard != NULL) (*environment)->DeleteLocalRef(environment, clipboard);
    if (service_name != NULL) (*environment)->DeleteLocalRef(environment, service_name);
    if (activity_class != NULL) (*environment)->DeleteLocalRef(environment, activity_class);
    if (detach) {
        (void)(*activity->vm)->DetachCurrentThread(activity->vm);
    }
    return success;
}

static uint64_t now_ms(void) {
    struct timespec value;
    if (clock_gettime(CLOCK_REALTIME, &value) != 0) {
        return 0u;
    }
    return (uint64_t)value.tv_sec * UINT64_C(1000) + (uint64_t)value.tv_nsec / UINT64_C(1000000);
}

static void save_state(AppContext *context) {
    if (!pasteboard_save(&context->state, context->storage_path)) {
        LOG_ERROR("could not save pasteboard state to %s", context->storage_path);
    }
}

static void capture_current_clipboard(AppContext *context) {
    char text[PASTEBOARD_TEXT_CAPACITY];
    if (read_current_clipboard(context, text, sizeof(text)) &&
        pasteboard_capture(&context->state, context->state.current_board, text, now_ms())) {
        save_state(context);
        context->redraw = true;
        LOG_INFO("captured current clipboard into board %u", context->state.current_board);
    }
}

static int32_t handle_input(struct android_app *app, AInputEvent *event) {
    AppContext *context = app->userData;
    if (AInputEvent_getType(event) != AINPUT_EVENT_TYPE_MOTION || app->window == NULL) {
        return 0;
    }
    const int32_t action = AMotionEvent_getAction(event) & AMOTION_EVENT_ACTION_MASK;
    const Viewport viewport = content_viewport(app);
    const float raw_x = AMotionEvent_getX(event, 0u);
    const float raw_y = AMotionEvent_getY(event, 0u);
    const float x = raw_x - viewport.left;
    const float y = raw_y - viewport.top;

    if (action == AMOTION_EVENT_ACTION_DOWN) {
        context->down_x = x;
        context->down_y = y;
        context->down_card = card_at(viewport.width, viewport.height, x, y);
        return 1;
    }
    if (action != AMOTION_EVENT_ACTION_UP) {
        return 1;
    }

    const float dx = x - context->down_x;
    const float dy = y - context->down_y;
    const float swipe = clampf(viewport.width * 0.14f, 54.0f, 120.0f);
    const bool horizontal_swipe = fabsf(dx) >= swipe && fabsf(dx) > fabsf(dy) * 1.2f;
    const size_t current = context->state.current_board;

    if (horizontal_swipe) {
        if (context->down_card >= 0) {
            if (dx < 0.0f && current > 0u) {
                if (pasteboard_move(&context->state, current, (size_t)context->down_card, current - 1u)) {
                    save_state(context);
                    context->redraw = true;
                }
            } else if (dx > 0.0f && current + 1u < PASTEBOARD_BOARD_COUNT) {
                if (pasteboard_move(&context->state, current, (size_t)context->down_card, current + 1u)) {
                    save_state(context);
                    context->redraw = true;
                }
            }
        } else {
            size_t destination = current;
            if (dx < 0.0f && current + 1u < PASTEBOARD_BOARD_COUNT) {
                destination = current + 1u;
            } else if (dx > 0.0f && current > 0u) {
                destination = current - 1u;
            }
            if (destination != current && pasteboard_set_current_board(&context->state, destination)) {
                save_state(context);
                context->redraw = true;
                capture_current_clipboard(context);
            }
        }
        return 1;
    }

    const float tap_slop = 24.0f;
    if (fabsf(dx) <= tap_slop && fabsf(dy) <= tap_slop && context->down_card >= 0) {
        const PasteboardEntry *entry = pasteboard_entry(&context->state, current,
                                                        (size_t)context->down_card);
        if (entry != NULL && copy_to_clipboard(context, entry->text)) {
            pasteboard_mark_clipboard_seen(&context->state, entry->text);
            save_state(context);
        }
    }
    return 1;
}

static void handle_command(struct android_app *app, int32_t command) {
    AppContext *context = app->userData;
    switch (command) {
        case APP_CMD_INIT_WINDOW:
        case APP_CMD_WINDOW_RESIZED:
        case APP_CMD_CONTENT_RECT_CHANGED:
            context->redraw = true;
            break;
        case APP_CMD_GAINED_FOCUS:
            capture_current_clipboard(context);
            context->redraw = true;
            break;
        case APP_CMD_LOST_FOCUS:
        case APP_CMD_PAUSE:
        case APP_CMD_STOP:
        case APP_CMD_TERM_WINDOW:
            save_state(context);
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
    context.down_card = -1;
    pasteboard_state_init(&context.state);
    const char *internal_path = app->activity->internalDataPath;
    if (internal_path == NULL) {
        internal_path = ".";
    }
    (void)snprintf(context.storage_path, sizeof(context.storage_path),
                   "%s/pasteboard.bin", internal_path);
    context.loaded = pasteboard_load(&context.state, context.storage_path);
    LOG_INFO("state %s from %s", context.loaded ? "loaded" : "started fresh",
             context.storage_path);

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
            save_state(&context);
            return;
        }
        if (context.redraw && app->window != NULL) {
            context.redraw = false;
            render(&context);
        }
    }
}
