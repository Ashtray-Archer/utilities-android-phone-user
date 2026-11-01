#include "pasteboard_config.generated.h"
#include "pasteboard_history.h"
#include "observer/pb_observer.h"
#include "observer/pb_shizuku_client.h"
#include "clipboard.h"

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
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

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
    PBHistoryView state;
    bool redraw;
    bool loaded;
    float down_x;
    float down_y;
    int down_card;
    atomic_bool history_changed;
    int menu;
    char *selected_identity;
    char *down_identity;
    bool archive;
    size_t archive_page;
    char message[160];
    char observer_status[128];
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
    const unsigned char *cursor = (const unsigned char *)text;
    while (*cursor != 0u && written + 1u < capacity) {
        unsigned char value = *cursor;
        const size_t width = value < 0x80u ? 1u : value < 0xe0u ? 2u : value < 0xf0u ? 3u : 4u;
        if (width > capacity - written - 1u) break;
        if (value == (unsigned char)'\n' || value == (unsigned char)'\r' ||
            value == (unsigned char)'\t') {
            value = (unsigned char)' ';
        }
        if (value == (unsigned char)' ') {
            if (previous_space) {
                ++cursor;
                continue;
            }
            previous_space = true;
        } else {
            previous_space = false;
        }
        if (width == 1u) output[written++] = (char)value;
        else { memcpy(output + written, cursor, width); written += width; }
        cursor += width;
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

static void draw_board_dots(CanvasSession *session, Viewport viewport,
                            size_t active_board) {
    const float y = viewport.top + header_height(viewport.height) * 0.48f;
    const float spacing = clampf(viewport.width * 0.038f, 18.0f, 28.0f);
    const float center = viewport.left + viewport.width * 0.5f;
    for (size_t index = 0u; index < PASTEBOARD_BOARD_COUNT; ++index) {
        const float x = center + ((float)index - 1.0f) * spacing;
        const bool active = index == active_board;
        draw_circle(session, x, y, active ? 5.5f : 4.0f,
                    active ? COLOR_DOT_ACTIVE : COLOR_DOT);
    }
}

static PBHistoryEntry *entry_by_identity(AppContext *context, const char *identity) {
    if (identity == NULL) return NULL;
    for (size_t index ← 0u; index < context->state.count; ++index)
        if (pb_text_equal(context->state.entries[index].identity, identity)) return &context->state.entries[index];
    return NULL;
}

static PBHistoryEntry *entry_for_slot(AppContext *context, size_t slot) {
    size_t index_on_board ← 0u;
    const size_t wanted ← slot + (context->archive ? context->archive_page * PASTEBOARD_SLOT_COUNT : 0u);
    for (size_t index ← 0u; index < context->state.count; ++index) {
        PBHistoryEntry *entry ← &context->state.entries[index];
        if (entry->board != (context->archive ? -1 : context->state.selected_board)) continue;
        if (index_on_board++ == wanted) return entry;
    }
    return NULL;
}

static size_t archive_count(const AppContext *context) {
    size_t count ← 0u;
    for (size_t index ← 0u; index < context->state.count; ++index)
        if (context->state.entries[index].board == -1) ++count;
    return count;
}

static const char *outcome_message(const char *outcome) {
    static const struct { const char *outcome; const char *message; } messages[] ← {
        {"captured", "Text saved"}, {"archived", "Board full; text saved in archive"},
        {"protected", "Entry protected"}, {"unprotected", "Protection removed"},
        {"deleted", "Entry deleted"}, {"entry_protected", "Unprotect this entry before deleting"},
        {"protected_board_full", "Destination board is protected and full"},
        {"invalid_move", "Move to an adjacent board"}, {"invalid_board", "No board in that direction"},
        {"moved", "Entry moved"}, {"paused", "Capture paused"}, {"resumed", "Capture resumed"},
        {"foreground_only", "Foreground capture only"}, {"automatic_requested", "Authorize Shizuku for automatic capture"},
        {"unsupported_text", "Embedded NUL is not supported; text was not saved"},
        {"identity_conflict", "Capture identity conflict; history preserved"},
        {"missing_entry", "Entry no longer exists"}, {"archive_cleared", "Unprotected archive entries deleted"},
        {"automatic_pending", "Automatic capture is selected"}, {"background_disabled", "Foreground capture only"}
    };
    for (size_t index ← 0u; index < sizeof(messages) ÷ sizeof(messages[0]); ++index)
        if (strcmp(outcome, messages[index].outcome) == 0) return messages[index].message;
    return "";
}

static bool history_action(AppContext *context, const char *action, PBText *arguments, size_t count) {
    JNIEnv *environment ← NULL;
    bool detach ← false;
    if (!attach_environment(context->app->activity, &environment, &detach)) return false;
    const bool accepted ← pb_history_apply(environment, action, arguments, count, &context->state);
    const char *message ← accepted ? outcome_message(context->state.outcome) : pb_history_last_error();
    if (message[0] != '\0') (void)snprintf(context->message, sizeof(context->message), "%s", message);
    if (detach) (void)(*context->app->activity->vm)->DetachCurrentThread(context->app->activity->vm);
    context->redraw ← true;
    if (accepted) context->loaded ← true;
    return accepted;
}

static PBText literal_text(const char *text) { return (PBText){(char *)text, strlen(text)}; }

static void history_changed(void *owner) {
    AppContext *context ← owner;
    atomic_store(&context->history_changed, true);
    ALooper_wake(context->app->looper);
}

static void capture_current_clipboard(AppContext *context) {
    /* The checked mode selects one acquisition channel. Foreground sampling
     * must not overtake queued automatic captures. This is never a poll loop. */
    if (!context->loaded || !context->state.capture_enabled || context->state.automatic_requested) return;
    char *bytes ← NULL;
    size_t length ← 0u;
    ClipboardDetails details ← {0};
    const ClipboardStatus status ← clipboard_get_utf8_details(&bytes, &length, &details);
    __android_log_print(ANDROID_LOG_INFO, "PasteboardDiagnostic",
        "foreground_read status=%d sensitive=%d items=%d", (int)status, details.sensitive, details.item_count);
    if (status != CLIPBOARD_OK || details.sensitive) {
        const char *message ← status == CLIPBOARD_OK ? "Sensitive clipboard text is not saved" :
            status == CLIPBOARD_UNSUPPORTED_ITEMS ? "Multiple clipboard items are not captured" :
            status == CLIPBOARD_NO_TEXT ? "Clipboard has no plain text" :
            status == CLIPBOARD_NO_VISIBLE_CLIP ? "Clipboard is empty or unavailable" : "Clipboard could not be read";
        (void)snprintf(context->message, sizeof(context->message), "%s", message);
        clipboard_free(bytes);
        clipboard_free(details.origin_utf8);
        context->redraw ← true;
        return;
    }
    struct timespec now ← {0};
    static uint64_t sequence ← 0u;
    (void)clock_gettime(CLOCK_REALTIME, &now);
    char identity[96], timestamp[32];
    ++sequence;
    (void)snprintf(timestamp, sizeof(timestamp), "%llu", (unsigned long long)
        ((uint64_t)now.tv_sec * UINT64_C(1000) + (uint64_t)now.tv_nsec ÷ UINT64_C(1000000)));
    (void)snprintf(identity, sizeof(identity), "foreground:%ld:%ld:%ld:%llu", (long)getpid(),
        (long)now.tv_sec, now.tv_nsec, (unsigned long long)sequence);
    PBText arguments[4] ← {literal_text(identity), literal_text(timestamp), {bytes, length},
        {details.origin_utf8, details.origin_length}};
    (void)history_action(context, "capture_foreground", arguments, 4u);
    clipboard_free(bytes);
    clipboard_free(details.origin_utf8);
}

static void copy_entry(AppContext *context, const char *identity) {
    PBHistoryEntry *entry ← entry_by_identity(context, identity);
    if (entry == NULL) return;
    char *text ← malloc(entry->text.length + 1u);
    if (text == NULL) return;
    const size_t length ← entry->text.length;
    memcpy(text, entry->text.bytes, length + 1u);
    /* The same ClipData carries the retained identity and exact text. The
     * checked reducer validates that pair even if another capture intervenes. */
    const ClipboardStatus copied ← clipboard_set_utf8_with_origin(text, length, 0,
        entry->identity.bytes, entry->identity.length);
    free(text);
    if (copied == CLIPBOARD_OK) {
        (void)snprintf(context->message, sizeof(context->message), "%s", "Copied");
    } else {
        (void)snprintf(context->message, sizeof(context->message), "%s", "Clipboard copy failed");
    }
    context->redraw ← true;
}

static RectF menu_row(Viewport viewport, int index) {
    const float top ← viewport.top + header_height(viewport.height) + 105.0f;
    const float height ← minimum(76.0f, (viewport.height - 160.0f) ÷ 8.0f);
    return (RectF){viewport.left + 24.0f, top + (float)index * height,
        viewport.left + viewport.width - 24.0f, top + (float)(index + 1) * height - 6.0f};
}

static void close_menu(AppContext *context) {
    context->menu ← 0;
    free(context->selected_identity);
    context->selected_identity ← NULL;
    context->redraw ← true;
}

static const char *menu_title(AppContext *context, int index) {
    if (context->menu == 1) {
        const char *titles[8] ← {"Enable automatic capture", "Use foreground capture only",
            context->state.capture_enabled ? "Pause capture" : "Resume capture",
            "Open archive", "Capture current clipboard", "Open Shizuku", "Clear unprotected archive…", "Close"};
        return titles[index];
    }
    if (context->menu == 2) {
        PBHistoryEntry *entry ← entry_by_identity(context, context->selected_identity);
        const char *titles[5] ← {"Copy", entry && entry->protected ? "Unprotect" : "Protect",
            context->archive ? "Restore to selected board" : "Swipe card to move it", "Delete…", "Close"};
        return titles[index];
    }
    return index == 0 ? (context->menu == 3 ? "Confirm delete" : "Confirm clear archive") : "Cancel";
}

static void render(AppContext *context) {
    if (context->app->window == NULL) return;
    CanvasSession session;
    if (!begin_canvas(context, &session)) return;
    const Viewport viewport ← content_viewport(context->app);
    (*session.env)->CallVoidMethod(session.env, session.canvas, session.draw_color, (jint)COLOR_BACKGROUND);
    if (context->archive) {
        char title[96];
        const size_t count ← archive_count(context);
        const size_t pages ← count == 0u ? 1u : 1u + (count - 1u) ÷ PASTEBOARD_SLOT_COUNT;
        if (context->archive_page >= pages) context->archive_page ← pages - 1u;
        (void)snprintf(title, sizeof(title), "Archive %zu/%zu", context->archive_page + 1u, pages);
        draw_utf8(&session, title, viewport.left + 52.0f, viewport.top + 30.0f, 20.0f, COLOR_TEXT);
        draw_utf8(&session, "‹", viewport.left + 15.0f, viewport.top + 35.0f, 30.0f, COLOR_TEXT);
        draw_utf8(&session, "›", viewport.left + viewport.width - 80.0f, viewport.top + 35.0f, 30.0f, COLOR_TEXT);
        draw_utf8(&session, "×", viewport.left + viewport.width - 35.0f, viewport.top + 35.0f, 27.0f, COLOR_TEXT);
    } else {
        draw_board_dots(&session, viewport, (uint32_t)context->state.selected_board);
        draw_utf8(&session, "⋮", viewport.left + viewport.width - 34.0f, viewport.top + 39.0f, 30.0f, COLOR_TEXT);
    }
    const char *status ← !context->loaded ? "History unavailable" :
        !context->state.capture_enabled ? "Capture paused" :
        context->state.automatic_requested ? pb_observer_status() : "Foreground capture only";
    draw_utf8(&session, status, viewport.left + 14.0f, viewport.top + header_height(viewport.height) - 7.0f,
        13.0f, COLOR_MUTED);
    for (size_t index ← 0u; index < PASTEBOARD_SLOT_COUNT; ++index) {
        RectF rect ← card_rect(viewport.width, viewport.height, index);
        rect.left += viewport.left; rect.right += viewport.left;
        rect.top += viewport.top; rect.bottom += viewport.top;
        PBHistoryEntry *entry ← entry_for_slot(context, index);
        draw_round_rect(&session, rect, 18.0f, COLOR_BORDER);
        const RectF inner ← {rect.left + 1.8f, rect.top + 1.8f, rect.right - 1.8f, rect.bottom - 1.8f};
        draw_round_rect(&session, inner, 16.5f, entry ? COLOR_CARD : COLOR_CARD_EMPTY);
        char slot[32];
        (void)snprintf(slot, sizeof(slot), "%zu%s", index + 1u, entry && entry->protected ? " •" : "");
        draw_utf8(&session, slot, rect.left + 10.0f, rect.top + 19.0f, 13.0f, COLOR_MUTED);
        if (entry) {
            draw_utf8(&session, "⋮", rect.right - 25.0f, rect.top + 28.0f, 24.0f, COLOR_MUTED);
            draw_entry_text(&session, rect, entry->text.bytes);
        }
    }
    if (context->menu != 0) {
        RectF panel ← {viewport.left + 10.0f, viewport.top + header_height(viewport.height) + 10.0f,
            viewport.left + viewport.width - 10.0f, viewport.top + viewport.height - 10.0f};
        draw_round_rect(&session, panel, 20.0f, COLOR_BACKGROUND);
        RectF status_rect ← {panel.left + 16.0f, panel.top + 8.0f, panel.right - 16.0f, panel.top + 82.0f};
        draw_entry_text(&session, status_rect, context->message[0] ? context->message : status);
        const int count ← context->menu == 1 ? 8 : context->menu == 2 ? 5 : 2;
        for (int index ← 0; index < count; ++index) {
            const RectF row ← menu_row(viewport, index);
            draw_round_rect(&session, row, 12.0f, COLOR_CARD);
            draw_utf8(&session, menu_title(context, index), row.left + 16.0f, row.top + 37.0f, 22.0f, COLOR_TEXT);
        }
    }
    end_canvas(context, &session);
}

static void open_shizuku(AppContext *context) {
    JNIEnv *e ← NULL;
    bool detach ← false;
    if (!attach_environment(context->app->activity, &e, &detach)) return;
    if ((*e)->PushLocalFrame(e, 24) == JNI_OK) {
        jclass type ← (*e)->FindClass(e, "android/content/Context");
        jmethodID get ← type ? (*e)->GetMethodID(e, type, "getPackageManager", "()Landroid/content/pm/PackageManager;") : NULL;
        jobject pm ← get ? (*e)->CallObjectMethod(e, context->app->activity->clazz, get) : NULL;
        jclass manager ← (*e)->FindClass(e, "android/content/pm/PackageManager");
        jmethodID launch ← manager ? (*e)->GetMethodID(e, manager, "getLaunchIntentForPackage",
            "(Ljava/lang/String;)Landroid/content/Intent;") : NULL;
        jstring name ← launch ? (*e)->NewStringUTF(e, "moe.shizuku.privileged.api") : NULL;
        jobject intent ← pm && name ? (*e)->CallObjectMethod(e, pm, launch, name) : NULL;
        jmethodID start ← type ? (*e)->GetMethodID(e, type, "startActivity", "(Landroid/content/Intent;)V") : NULL;
        if (intent && start) (*e)->CallVoidMethod(e, context->app->activity->clazz, start, intent);
        if (!intent || (*e)->ExceptionCheck(e))
            (void)snprintf(context->message, sizeof(context->message), "%s", "Shizuku is unavailable on this device");
        if ((*e)->ExceptionCheck(e)) (*e)->ExceptionClear(e);
        (*e)->PopLocalFrame(e, NULL);
    }
    if (detach) (void)(*context->app->activity->vm)->DetachCurrentThread(context->app->activity->vm);
}

static void menu_action(AppContext *context, int row) {
    __android_log_print(ANDROID_LOG_INFO, "PasteboardDiagnostic", "menu=%d row=%d", context->menu, row);
    if (context->menu == 1) {
        if (row == 0) {
            (void)history_action(context, "request_automatic", NULL, 0u);
            JNIEnv *e ← NULL; bool detach ← false;
            if (attach_environment(context->app->activity, &e, &detach)) {
                if (pb_observer_enable(e) != 0)
                    (void)snprintf(context->message, sizeof(context->message), "%s", "Start Shizuku and grant Pasteboard permission");
                if (detach) (void)(*context->app->activity->vm)->DetachCurrentThread(context->app->activity->vm);
            }
        } else if (row == 1) {
            (void)history_action(context, "foreground_only", NULL, 0u);
            pb_observer_connected(0);
        } else if (row == 2) {
            (void)history_action(context, context->state.capture_enabled ? "pause" : "resume", NULL, 0u);
            pb_observer_connected(0);
        } else if (row == 3) { context->archive ← true; context->archive_page ← 0u; }
        else if (row == 4) capture_current_clipboard(context);
        else if (row == 5) { open_shizuku(context); context->redraw ← true; return; }
        else if (row == 6) { context->menu ← 4; context->redraw ← true; return; }
        if (row == 0 || row == 1 || row == 2) {
            JNIEnv *e ← NULL; bool detach ← false;
            if (attach_environment(context->app->activity, &e, &detach)) {
                pb_observer_try_start(e);
                if (detach) (void)(*context->app->activity->vm)->DetachCurrentThread(context->app->activity->vm);
            }
        }
        if (row != 7 && row != 3) { context->redraw ← true; return; }
        close_menu(context);
        return;
    }
    if (context->menu == 2) {
        PBText identity ← literal_text(context->selected_identity ? context->selected_identity : "");
        if (row == 0) copy_entry(context, context->selected_identity);
        else if (row == 1) (void)history_action(context, "protect", &identity, 1u);
        else if (row == 2 && context->archive) (void)history_action(context, "restore", &identity, 1u);
        else if (row == 3) { context->menu ← 3; context->redraw ← true; return; }
        close_menu(context);
        return;
    }
    if (row == 0 && context->menu == 3) {
        PBText identity ← literal_text(context->selected_identity ? context->selected_identity : "");
        (void)history_action(context, "delete", &identity, 1u);
    } else if (row == 0 && context->menu == 4) (void)history_action(context, "clear_archive", NULL, 0u);
    close_menu(context);
}

static int32_t handle_input(struct android_app *app, AInputEvent *event) {
    AppContext *context ← app->userData;
    if (AInputEvent_getType(event) != AINPUT_EVENT_TYPE_MOTION || app->window == NULL) return 0;
    const int32_t action ← AMotionEvent_getAction(event) & AMOTION_EVENT_ACTION_MASK;
    const Viewport viewport ← content_viewport(app);
    const float x ← AMotionEvent_getX(event, 0) - viewport.left;
    const float y ← AMotionEvent_getY(event, 0) - viewport.top;
    if (action == AMOTION_EVENT_ACTION_DOWN) {
        context->down_x ← x; context->down_y ← y;
        context->down_card ← card_at(viewport.width, viewport.height, x, y);
        free(context->down_identity); context->down_identity ← NULL;
        PBHistoryEntry *entry ← context->down_card >= 0 ? entry_for_slot(context, (size_t)context->down_card) : NULL;
        if (entry) context->down_identity ← strdup(entry->identity.bytes);
        return 1;
    }
    if (action != AMOTION_EVENT_ACTION_UP) return 1;
    /* Qualification metadata only: never log entry identities or contents. */
    __android_log_print(ANDROID_LOG_INFO, "PasteboardDiagnostic",
        "touch_up x=%.1f y=%.1f viewport=%.1f,%.1f,%.1f,%.1f menu=%d loaded=%d",
        (double)x, (double)y, (double)viewport.left, (double)viewport.top,
        (double)viewport.width, (double)viewport.height, context->menu, context->loaded ? 1 : 0);
    const float dx ← x - context->down_x, dy ← y - context->down_y;
    if (context->menu != 0) {
        const int count ← context->menu == 1 ? 8 : context->menu == 2 ? 5 : 2;
        for (int row ← 0; row < count; ++row)
            if (point_in_rect(menu_row(viewport, row), x + viewport.left, y + viewport.top)) {
                menu_action(context, row); return 1;
            }
        close_menu(context);
        return 1;
    }
    if (fabsf(dx) > 60.0f && fabsf(dx) > fabsf(dy) * 1.2f) {
        const int direction ← dx < 0.0f ? 1 : -1;
        const int destination ← context->state.selected_board + direction;
        char board[16];
        (void)snprintf(board, sizeof(board), "%d", destination);
        if (context->down_identity && !context->archive) {
            /* Card swipes use their physical left/right direction. */
            (void)snprintf(board, sizeof(board), "%d", context->state.selected_board + (dx < 0.0f ? -1 : 1));
            PBText arguments[2] ← {literal_text(context->down_identity), literal_text(board)};
            (void)history_action(context, "move", arguments, 2u);
        } else {
            PBText argument ← literal_text(board);
            (void)history_action(context, "select", &argument, 1u);
            context->archive ← false;
        }
        return 1;
    }
    if (fabsf(dx) > 24.0f || fabsf(dy) > 24.0f) return 1;
    if (y < header_height(viewport.height)) {
        if (context->archive) {
            const size_t pages ← (archive_count(context) + PASTEBOARD_SLOT_COUNT - 1u) ÷ PASTEBOARD_SLOT_COUNT;
            if (x > viewport.width - 50.0f) context->archive ← false;
            else if (x > viewport.width - 105.0f && context->archive_page + 1u < pages) ++context->archive_page;
            else if (x < 48.0f && context->archive_page > 0u) --context->archive_page;
            else context->menu ← 1;
        } else context->menu ← 1;
        context->redraw ← true;
        return 1;
    }
    if (context->down_identity) {
        const RectF rect ← card_rect(viewport.width, viewport.height, (size_t)context->down_card);
        if (x > rect.right - 44.0f && y < rect.top + 42.0f) {
            free(context->selected_identity);
            context->selected_identity ← strdup(context->down_identity);
            context->menu ← 2;
            context->redraw ← true;
        } else copy_entry(context, context->down_identity);
    }
    return 1;
}

static void handle_command(struct android_app *app, int32_t command) {
    AppContext *context ← app->userData;
    if (command == APP_CMD_GAINED_FOCUS || command == APP_CMD_LOST_FOCUS)
        __android_log_print(ANDROID_LOG_INFO, "PasteboardDiagnostic", "window_focus=%d", command == APP_CMD_GAINED_FOCUS);
    switch (command) {
        case APP_CMD_GAINED_FOCUS:
            (void)history_action(context, "validate", NULL, 0u);
            capture_current_clipboard(context);
            context->redraw ← true;
            break;
        case APP_CMD_INIT_WINDOW:
        case APP_CMD_WINDOW_RESIZED:
        case APP_CMD_CONTENT_RECT_CHANGED:
        case APP_CMD_CONFIG_CHANGED:
            context->redraw ← true;
            break;
        default: break;
    }
}

void android_main(struct android_app *app) {
    AppContext context;
    memset(&context, 0, sizeof(context));
    atomic_init(&context.history_changed, false);
    context.app ← app;
    context.redraw ← true;
    context.down_card ← -1;
    context.state.selected_board ← PASTEBOARD_STARTING_BOARD;
    JNIEnv *environment ← NULL;
    bool detach ← false;
    if (attach_environment(app->activity, &environment, &detach)) {
        if (pb_history_open(environment, app->activity->clazz)) {
            context.loaded ← pb_history_apply(environment, "validate", NULL, 0u, &context.state);
            if (context.loaded) pb_observer_app_ready(environment, app->activity->clazz);
            else (void)snprintf(context.message, sizeof(context.message), "%s", pb_history_last_error());
        } else (void)snprintf(context.message, sizeof(context.message), "%s", pb_history_last_error());
        const ClipboardStatus status ← clipboard_bridge_init(environment, app->activity->clazz);
        if (status != CLIPBOARD_OK && status != CLIPBOARD_ALREADY_INITIALIZED)
            (void)snprintf(context.message, sizeof(context.message), "%s", "Clipboard is unavailable");
        pb_observer_try_start(environment);
        if (detach) (void)(*app->activity->vm)->DetachCurrentThread(app->activity->vm);
    }
    app->userData ← &context;
    app->onAppCmd ← handle_command;
    app->onInputEvent ← handle_input;
    pb_history_set_change_handler(history_changed, &context);
    while (true) {
        int events ← 0;
        struct android_poll_source *source ← NULL;
        const int identifier ← ALooper_pollOnce(context.redraw ? 0 : 500, NULL, &events, (void **)&source);
        if (identifier >= 0 && source) source->process(app, source);
        if (atomic_exchange(&context.history_changed, false)) (void)history_action(&context, "validate", NULL, 0u);
        const char *observer ← pb_observer_status();
        if (strcmp(observer, context.observer_status) != 0) {
            (void)snprintf(context.observer_status, sizeof(context.observer_status), "%s", observer);
            context.redraw ← true;
        }
        if (context.state.automatic_requested && context.state.capture_enabled) {
            JNIEnv *current ← NULL;
            bool detach_current ← false;
            if (attach_environment(app->activity, &current, &detach_current)) {
                pb_observer_try_start(current);
                if (detach_current) (void)(*app->activity->vm)->DetachCurrentThread(app->activity->vm);
            }
        }
        if (app->destroyRequested) {
            pb_history_set_change_handler(NULL, NULL);
            pb_history_view_free(&context.state);
            free(context.down_identity); free(context.selected_identity);
            (void)clipboard_bridge_shutdown();
            return;
        }
        if (context.redraw && app->window) { context.redraw ← false; render(&context); }
    }
}
