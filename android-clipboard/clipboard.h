#ifndef ANDROID_CLIPBOARD_H
#define ANDROID_CLIPBOARD_H

#include <jni.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    CLIPBOARD_OK = 0,
    CLIPBOARD_NO_VISIBLE_CLIP = 1,
    CLIPBOARD_NO_TEXT = 2,
    CLIPBOARD_NOT_INITIALIZED = 3,
    CLIPBOARD_INVALID_ARGUMENT = 4,
    CLIPBOARD_INVALID_UTF8 = 5,
    CLIPBOARD_OUT_OF_MEMORY = 6,
    CLIPBOARD_JNI_ERROR = 7,
    CLIPBOARD_ALREADY_INITIALIZED = 8,
    CLIPBOARD_UNSUPPORTED_ITEMS = 9
} ClipboardStatus;

typedef struct {
    int item_count;
    int sensitive;
} ClipboardDetails;

/*
 * Initialize once from an Android framework thread or another thread that has
 * a valid JNIEnv. `context` may be an Activity, Service, or other Context.
 * The bridge stores only a global reference to ClipboardManager.
 *
 * Initialization/shutdown must not race clipboard operations.
 */
ClipboardStatus clipboard_bridge_init(JNIEnv *environment, jobject context);
ClipboardStatus clipboard_bridge_shutdown(void);

/*
 * Returns CLIPBOARD_OK and writes 0/1 when the clipboard description is
 * visible. Android may hide the primary clip from an app without input focus;
 * that case is CLIPBOARD_NO_VISIBLE_CLIP rather than a fabricated "empty".
 */
ClipboardStatus clipboard_has_text(int *out_has_text);

/*
 * Returns the first ClipData item only when it directly contains text.
 * On CLIPBOARD_OK, `*out_bytes` is malloc-owned, NUL-terminated for
 * convenience, and `*out_len` is authoritative, so embedded NUL is preserved.
 * Release the buffer with clipboard_free(). Empty text is CLIPBOARD_OK,len=0.
 */
ClipboardStatus clipboard_get_utf8(char **out_bytes, size_t *out_len);

/* Capture one immutable ClipData snapshot, including its sensitive-content
 * indication. Multi-item clips are explicitly refused by this history-facing
 * API; the original first-item getter above retains its existing contract.
 * A sensitive clip still returns its exact text and flag to the caller, which
 * must apply the user's retention policy before saving it. */
ClipboardStatus clipboard_get_utf8_details(char **out_bytes, size_t *out_len,
                                          ClipboardDetails *out_details);

/*
 * `bytes[0..len)` is strict standard UTF-8, not JNI modified UTF-8.
 * NULL is accepted only when len is zero. When `sensitive` is nonzero the
 * Android sensitive-clipboard description extra is attached on API 24+; it is
 * interpreted by the system clipboard UI on API 33+.
 */
ClipboardStatus clipboard_set_utf8(const char *bytes, size_t len, int sensitive);

/* API 28+ uses clearPrimaryClip(); older Android receives an empty text clip. */
ClipboardStatus clipboard_clear(void);

void clipboard_free(void *allocation);
const char *clipboard_status_name(ClipboardStatus status);

#ifdef __cplusplus
}
#endif

#endif
