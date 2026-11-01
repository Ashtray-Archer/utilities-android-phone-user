#include "../clipboard.h"

#include <android/log.h>
#include <android/native_activity.h>

#include <stddef.h>
#include <string.h>

#define LOG_TAG "ClipboardSmoke"

static int failures = 0;
static int ran = 0;

static void fail_status(const char *step, ClipboardStatus status) {
    __android_log_print(
        ANDROID_LOG_ERROR,
        LOG_TAG,
        "FAIL %s: %s",
        step,
        clipboard_status_name(status));
    failures += 1;
}

static void fail_text(const char *step) {
    __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "FAIL %s", step);
    failures += 1;
}

static int expect_ok(const char *step, ClipboardStatus status) {
    if (status == CLIPBOARD_OK) {
        return 1;
    }
    fail_status(step, status);
    return 0;
}

static int remove_optional_extras(ANativeActivity *activity) {
    JNIEnv *e = activity->env;
    if ((*e)->PushLocalFrame(e, 24) != JNI_OK) return 0;
    jclass context = (*e)->GetObjectClass(e, activity->clazz);
    jmethodID service = context ? (*e)->GetMethodID(e, context, "getSystemService", "(Ljava/lang/String;)Ljava/lang/Object;") : NULL;
    jstring name = service ? (*e)->NewStringUTF(e, "clipboard") : NULL;
    jobject manager = name ? (*e)->CallObjectMethod(e, activity->clazz, service, name) : NULL;
    jclass manager_type = manager && !(*e)->ExceptionCheck(e) ? (*e)->GetObjectClass(e, manager) : NULL;
    jmethodID get = manager_type ? (*e)->GetMethodID(e, manager_type, "getPrimaryClip", "()Landroid/content/ClipData;") : NULL;
    jobject clip = get ? (*e)->CallObjectMethod(e, manager, get) : NULL;
    jclass clip_type = clip && !(*e)->ExceptionCheck(e) ? (*e)->GetObjectClass(e, clip) : NULL;
    jmethodID item_at = clip_type ? (*e)->GetMethodID(e, clip_type, "getItemAt", "(I)Landroid/content/ClipData$Item;") : NULL;
    jobject item = item_at ? (*e)->CallObjectMethod(e, clip, item_at, 0) : NULL;
    jclass item_type = item && !(*e)->ExceptionCheck(e) ? (*e)->GetObjectClass(e, item) : NULL;
    jmethodID get_text = item_type ? (*e)->GetMethodID(e, item_type, "getText", "()Ljava/lang/CharSequence;") : NULL;
    jobject text = get_text ? (*e)->CallObjectMethod(e, item, get_text) : NULL;
    jmethodID plain = clip_type && !(*e)->ExceptionCheck(e) ? (*e)->GetStaticMethodID(e, clip_type, "newPlainText", "(Ljava/lang/CharSequence;Ljava/lang/CharSequence;)Landroid/content/ClipData;") : NULL;
    jobject fresh = plain && text ? (*e)->CallStaticObjectMethod(e, clip_type, plain, NULL, text) : NULL;
    jmethodID describe = clip_type && !(*e)->ExceptionCheck(e) ? (*e)->GetMethodID(e, clip_type, "getDescription", "()Landroid/content/ClipDescription;") : NULL;
    jobject description = fresh && describe ? (*e)->CallObjectMethod(e, fresh, describe) : NULL;
    jclass description_type = description && !(*e)->ExceptionCheck(e) ? (*e)->GetObjectClass(e, description) : NULL;
    jmethodID get_extras = description_type ? (*e)->GetMethodID(e, description_type, "getExtras", "()Landroid/os/PersistableBundle;") : NULL;
    jobject extras = get_extras ? (*e)->CallObjectMethod(e, description, get_extras) : NULL;
    jmethodID set = manager_type && !(*e)->ExceptionCheck(e) ? (*e)->GetMethodID(e, manager_type, "setPrimaryClip", "(Landroid/content/ClipData;)V") : NULL;
    const int absent = get_extras && extras == NULL && !(*e)->ExceptionCheck(e);
    if (set && absent) (*e)->CallVoidMethod(e, manager, set, fresh);
    const int valid = set && absent && !(*e)->ExceptionCheck(e);
    if ((*e)->ExceptionCheck(e)) (*e)->ExceptionClear(e);
    (*e)->PopLocalFrame(e, NULL);
    return valid;
}

static void run_smoke(ANativeActivity *activity) {
    ClipboardStatus status = clipboard_bridge_init(activity->env, activity->clazz);
    if (!expect_ok("init", status)) {
        goto finish;
    }

    static const unsigned char sample[] = {
        'A', 0x00, 'B', ' ',
        0xce, 0xbb, ' ',
        0xf0, 0x9f, 0x93, 0x8b
    };

    if (!expect_ok(
            "set foreground UTF-8",
            clipboard_set_utf8((const char *)sample, sizeof(sample), 0))) {
        goto shutdown;
    }

    int has_text = 0;
    if (expect_ok("has text", clipboard_has_text(&has_text)) && has_text != 1) {
        fail_text("has text returned 0 after plain-text copy");
    }

    char *actual = NULL;
    size_t actual_len = 0;
    status = clipboard_get_utf8(&actual, &actual_len);
    if (expect_ok("get foreground UTF-8", status)) {
        if (actual_len != sizeof(sample) ||
            memcmp(actual, sample, sizeof(sample)) != 0) {
            fail_text("foreground UTF-8 byte roundtrip");
        }
    }
    clipboard_free(actual);

    if (!remove_optional_extras(activity)) fail_text("copy without optional extras");
    ClipboardDetails details = {0};
    actual = NULL;
    actual_len = 0;
    if (expect_ok("read copy timestamp", clipboard_get_utf8_details(&actual, &actual_len, &details))) {
        if (details.sensitive || details.origin_utf8 || details.origin_length != 0u)
            fail_text("absent extras has no sensitive/origin metadata");
        if ((activity->sdkVersion >= 26 && details.copied_at_millis <= 0) ||
            (activity->sdkVersion < 26 && details.copied_at_millis != 0)) fail_text("SDK copy timestamp boundary");
        if (actual_len != sizeof(sample) || memcmp(actual, sample, sizeof(sample)) != 0)
            fail_text("timestamp metadata preserves full UTF-8");
    }
    clipboard_free(actual);
    clipboard_free(details.origin_utf8);

    /* This proves the sensitive write path executes on Android. The system
     * preview treatment itself remains a separate API-33+ acceptance check. */
    expect_ok(
        "set sensitive UTF-8",
        clipboard_set_utf8((const char *)sample, sizeof(sample), 1));

    if (expect_ok("clear", clipboard_clear())) {
        actual = NULL;
        actual_len = 0;
        status = clipboard_get_utf8(&actual, &actual_len);
        if (activity->sdkVersion >= 28) {
            if (status != CLIPBOARD_NO_VISIBLE_CLIP) {
                fail_status("clear API 28+ expected no visible clip", status);
            }
        } else if (status != CLIPBOARD_OK || actual_len != 0) {
            fail_status("clear API 24-27 expected empty text", status);
        }
        clipboard_free(actual);
    }

shutdown:
    expect_ok("shutdown", clipboard_bridge_shutdown());

finish:
    if (failures == 0) {
        __android_log_print(
            ANDROID_LOG_INFO,
            LOG_TAG,
            "CLIPBOARD_SMOKE PASS sdk=%d",
            activity->sdkVersion);
    } else {
        __android_log_print(
            ANDROID_LOG_ERROR,
            LOG_TAG,
            "CLIPBOARD_SMOKE FAIL failures=%d sdk=%d",
            failures,
            activity->sdkVersion);
    }
    ANativeActivity_finish(activity);
}

static void on_window_focus_changed(ANativeActivity *activity, int has_focus) {
    if (has_focus == 0 || ran != 0) {
        return;
    }
    ran = 1;
    run_smoke(activity);
}

__attribute__((visibility("default")))
void ANativeActivity_onCreate(
    ANativeActivity *activity,
    void *saved_state,
    size_t saved_state_size) {
    (void)saved_state;
    (void)saved_state_size;
    failures = 0;
    ran = 0;

    if (activity == NULL || activity->env == NULL || activity->clazz == NULL ||
        activity->callbacks == NULL) {
        __android_log_print(
            ANDROID_LOG_ERROR,
            LOG_TAG,
            "CLIPBOARD_SMOKE FAIL invalid NativeActivity boundary");
        return;
    }

    activity->callbacks->onWindowFocusChanged = on_window_focus_changed;
}
