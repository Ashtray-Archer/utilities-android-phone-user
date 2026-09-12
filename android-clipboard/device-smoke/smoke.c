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
