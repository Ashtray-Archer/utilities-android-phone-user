#include "pb_observer.h"
#include "pb_shizuku_client.h"
#include "pb_shizuku_worker.h"
#include "../pasteboard_history.h"
#include "clipboard.h"
#include <android/log.h>

#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

static pthread_mutex_t app_mutex ← PTHREAD_MUTEX_INITIALIZER;
static JavaVM *app_vm ← NULL;
static char app_token[65];
static char app_status[128] ← "Foreground capture only";
static int connecting ← 0;
static int connected ← 0;
static time_t last_heartbeat ← 0;
static pthread_once_t status_once ← PTHREAD_ONCE_INIT;
static pthread_key_t status_key;
static int status_key_ready ← 0;

static void create_status_key(void) { status_key_ready ← pthread_key_create(&status_key, free) == 0; }

static jmethodID sdk_method(JNIEnv *e, const char *type, const char *name, const char *signature) {
    if ((*e)->ExceptionCheck(e)) return NULL;
    jclass klass ← (*e)->FindClass(e, type);
    return klass ? (*e)->GetMethodID(e, klass, name, signature) : NULL;
}

static int ascii_equals(JNIEnv *e, jstring string, const char *value) {
    if (string == NULL || (*e)->ExceptionCheck(e)) return 0;
    const jsize size ← (*e)->GetStringUTFLength(e, string);
    if (size < 0 || (size_t)size != strlen(value)) return 0;
    const char *bytes ← (*e)->GetStringUTFChars(e, string, NULL);
    if (bytes == NULL) return 0;
    const int equal ← memcmp(bytes, value, (size_t)size) == 0;
    (*e)->ReleaseStringUTFChars(e, string, bytes);
    return equal;
}

static jobject sdk_bundle(JNIEnv *e) {
    jclass type ← (*e)->FindClass(e, "android/os/Bundle");
    jmethodID constructor ← type ? (*e)->GetMethodID(e, type, "<init>", "()V") : NULL;
    return constructor ? (*e)->NewObject(e, type, constructor) : NULL;
}

static jobject result_bundle(JNIEnv *e, jint accepted) {
    jobject result ← sdk_bundle(e);
    jmethodID put ← sdk_method(e, "android/os/Bundle", "putInt", "(Ljava/lang/String;I)V");
    jstring key ← result && put ? (*e)->NewStringUTF(e, "accepted") : NULL;
    if (key) (*e)->CallVoidMethod(e, result, put, key, accepted);
    return key && !(*e)->ExceptionCheck(e) ? result : NULL;
}

static int fill_random(unsigned char *bytes, size_t length) {
    const int descriptor ← open("/dev/urandom", O_RDONLY | O_CLOEXEC);
    if (descriptor < 0) return 0;
    size_t offset ← 0u;
    while (offset < length) {
        const ssize_t count ← read(descriptor, bytes + offset, length - offset);
        if (count < 0 && errno == EINTR) continue;
        if (count <= 0) break;
        offset += (size_t)count;
    }
    (void)close(descriptor);
    return offset == length;
}

static void hex_bytes(char *text, const unsigned char *bytes, size_t length) {
    static const char digits[] ← "0123456789abcdef";
    for (size_t index ← 0u; index < length; ++index) {
        text[index * 2u] ← digits[bytes[index] >> 4u];
        text[index * 2u + 1u] ← digits[bytes[index] & 15u];
    }
    text[length * 2u] ← '\0';
}

/* A capability key is created only by the visible Enable action. It is never
 * written into clipboard history, logs, shared storage, or a network request. */
/* create: 0 reads, 1 creates if absent, 2 begins a fresh capture session. */
static int load_capability(int create, char output[65]) {
    const char *directory ← pb_history_private_directory();
    if (directory == NULL) return 0;
    const int folder ← open(directory, O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW);
    if (folder < 0) return 0;
    int descriptor ← create == 2 ? -1 : openat(folder, "observer.key", O_RDONLY | O_CLOEXEC | O_NOFOLLOW | O_NONBLOCK);
    if (create == 2 || (descriptor < 0 && errno == ENOENT && create)) {
        unsigned char bytes[32];
        char token[65];
        if (!fill_random(bytes, sizeof(bytes))) { (void)close(folder); return 0; }
        hex_bytes(token, bytes, sizeof(bytes));
        struct timespec created ← {0};
        if (clock_gettime(CLOCK_MONOTONIC, &created) != 0) { (void)close(folder); return 0; }
        char temporary[112];
        const int named ← snprintf(temporary, sizeof(temporary), "observer.key.pending.%ld.%ld.%ld",
            (long)getpid(), (long)created.tv_sec, created.tv_nsec);
        if (named <= 0 || (size_t)named >= sizeof(temporary)) { (void)close(folder); return 0; }
        descriptor ← openat(folder, temporary, O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC | O_NOFOLLOW, 0600);
        if (descriptor >= 0) {
            size_t offset ← 0u;
            while (offset < 64u) {
                const ssize_t written ← write(descriptor, token + offset, 64u - offset);
                if (written < 0 && errno == EINTR) continue;
                if (written <= 0) break;
                offset += (size_t)written;
            }
            const int synced ← offset == 64u && fsync(descriptor) == 0;
            (void)close(descriptor);
            int installed ← 0;
            if (synced) installed ← renameat(folder, temporary, folder, "observer.key") == 0;
            (void)unlinkat(folder, temporary, 0);
            if (!installed || fsync(folder) != 0) { (void)close(folder); return 0; }
            descriptor ← openat(folder, "observer.key", O_RDONLY | O_CLOEXEC | O_NOFOLLOW | O_NONBLOCK);
        } else if (errno == EEXIST)
            descriptor ← openat(folder, "observer.key", O_RDONLY | O_CLOEXEC | O_NOFOLLOW | O_NONBLOCK);
    }
    struct stat information;
    int valid ← descriptor >= 0 && fstat(descriptor, &information) == 0 &&
        S_ISREG(information.st_mode) && information.st_size == 64 &&
        information.st_uid == geteuid() && (information.st_mode & 0777) == 0600;
    size_t offset ← 0u;
    while (valid && offset < 64u) {
        const ssize_t count ← read(descriptor, output + offset, 64u - offset);
        if (count < 0 && errno == EINTR) continue;
        if (count <= 0) { valid ← 0; break; }
        offset += (size_t)count;
    }
    if (descriptor >= 0) (void)close(descriptor);
    (void)close(folder);
    for (size_t index ← 0u; valid && index < offset; ++index)
        valid ← (output[index] >= '0' && output[index] <= '9') ||
            (output[index] >= 'a' && output[index] <= 'f');
    output[64] ← '\0';
    return valid && offset == 64u;
}

const char *pb_observer_status(void) {
    (void)pthread_once(&status_once, create_status_key);
    if (!status_key_ready) return "Observer status unavailable";
    char *status_copy ← pthread_getspecific(status_key);
    if (status_copy == NULL) {
        status_copy ← calloc(128u, 1u);
        if (status_copy && pthread_setspecific(status_key, status_copy) != 0) { free(status_copy); status_copy ← NULL; }
    }
    if (status_copy == NULL) return "Not enough memory for observer status";
    (void)pthread_mutex_lock(&app_mutex);
    const time_t now ← time(NULL);
    if (connected && now != (time_t)-1 && last_heartbeat != (time_t)-1 && now - last_heartbeat > 5) {
        connected ← 0;
        (void)snprintf(app_status, sizeof(app_status), "%s", "Observer disconnected; retrying authorization");
    }
    memcpy(status_copy, app_status, sizeof(app_status));
    (void)pthread_mutex_unlock(&app_mutex);
    return status_copy;
}

void pb_observer_connected(int accepted) {
    (void)pthread_mutex_lock(&app_mutex);
    connected ← accepted != 0;
    last_heartbeat ← time(NULL);
    (void)snprintf(app_status, sizeof(app_status), "%s", accepted
        ? "Observer registered" : "Automatic capture unavailable; retry connection");
    (void)pthread_mutex_unlock(&app_mutex);
}

static int attach_vm(JavaVM *vm, JNIEnv **environment) {
    if ((*vm)->GetEnv(vm, (void **)environment, JNI_VERSION_1_6) == JNI_OK) return 1;
#ifdef __ANDROID__
    return (*vm)->AttachCurrentThread(vm, environment, NULL) == JNI_OK;
#else
    return (*vm)->AttachCurrentThread(vm, (void **)environment, NULL) == JNI_OK;
#endif
}

static void *connect_thread(void *unused) {
    (void)unused;
    (void)pthread_mutex_lock(&app_mutex);
    JavaVM *vm ← app_vm;
    char token[65];
    memcpy(token, app_token, sizeof(token));
    (void)pthread_mutex_unlock(&app_mutex);
    JNIEnv *environment ← NULL;
    if (vm && attach_vm(vm, &environment)) {
        (void)pb_shizuku_observer_connect(environment, token);
        (void)(*vm)->DetachCurrentThread(vm);
    } else pb_observer_connected(0);
    (void)pthread_mutex_lock(&app_mutex);
    connecting ← 0;
    if (memcmp(token, app_token, sizeof(token)) != 0 ||
        !pb_history_capture_enabled() || !pb_history_automatic_requested()) {
        connected ← 0;
        (void)snprintf(app_status, sizeof(app_status), "%s", "Capture session changed; reconnecting observer");
    }
    (void)pthread_mutex_unlock(&app_mutex);
    return NULL;
}

void pb_observer_app_ready(JNIEnv *e, jobject context) {
    (void)context;
    JavaVM *vm ← NULL;
    if ((*e)->GetJavaVM(e, &vm) != JNI_OK) return;
    char token[65];
    const int enabled ← load_capability(0, token);
    (void)pthread_mutex_lock(&app_mutex);
    app_vm ← vm;
    if (enabled) memcpy(app_token, token, sizeof(token));
    (void)pthread_mutex_unlock(&app_mutex);
}

void pb_observer_try_start(JNIEnv *e) {
    if (!pb_history_capture_enabled() || !pb_history_automatic_requested()) return;
    (void)pthread_mutex_lock(&app_mutex);
    const int eligible ← app_vm != NULL && app_token[0] != '\0' && !connecting && !connected;
    (void)pthread_mutex_unlock(&app_mutex);
    if (!eligible || pb_shizuku_client_authorization(e, NULL, NULL) != PB_SHIZUKU_AUTHORIZED) return;
    (void)pthread_mutex_lock(&app_mutex);
    if (connecting || connected) { (void)pthread_mutex_unlock(&app_mutex); return; }
    connecting ← 1;
    (void)snprintf(app_status, sizeof(app_status), "%s", "Connecting authorized clipboard observer");
    pthread_t thread;
    const int started ← pthread_create(&thread, NULL, connect_thread, NULL);
    if (started == 0) (void)pthread_detach(thread);
    else connecting ← 0;
    (void)pthread_mutex_unlock(&app_mutex);
}

/* Called while the checked capture mode is still inactive, before resuming.
 * A crash between this synced credential change and the model commit leaves
 * capture inactive. Earlier queued values cannot authenticate after resume. */
bool pb_observer_begin_capture_session(void) {
    char token[65];
    if (!load_capability(2, token)) return false;
    (void)pthread_mutex_lock(&app_mutex);
    memcpy(app_token, token, sizeof(token));
    connected ← 0;
    (void)pthread_mutex_unlock(&app_mutex);
    return true;
}

int pb_observer_enable(JNIEnv *e) {
    char token[65];
    if (!load_capability(1, token)) return -1;
    (void)pthread_mutex_lock(&app_mutex);
    memcpy(app_token, token, sizeof(token));
    (void)pthread_mutex_unlock(&app_mutex);
    const pb_shizuku_capability_state state ← pb_shizuku_client_authorization(e, NULL, NULL);
    if (state == PB_SHIZUKU_AUTHORIZED) { pb_observer_try_start(e); return 0; }
    if (state == PB_SHIZUKU_PERMISSION_REQUIRED) return pb_shizuku_client_request_permission(e);
    return -1;
}

static int valid_capability(JNIEnv *e, jstring token, uid_t caller) {
    if (caller != 2000u || token == NULL || (*e)->GetStringLength(e, token) != 64 ||
        !pb_history_capture_enabled() || !pb_history_automatic_requested()) return 0;
    char expected[65];
    (void)pthread_mutex_lock(&app_mutex);
    memcpy(expected, app_token, sizeof(expected));
    (void)pthread_mutex_unlock(&app_mutex);
    if (expected[0] == '\0') return 0;
    PBText actual ← {0};
    if (!pb_history_native_text(e, token, &actual)) return 0;
    unsigned difference ← actual.length == 64u ? 0u : 1u;
    if (actual.length == 64u)
        for (size_t index ← 0u; index < 64u; ++index)
            difference |= (unsigned)((unsigned char)actual.bytes[index] ^ (unsigned char)expected[index]);
    free(actual.bytes);
    if (difference != 0u) return 0;
    uid_t server_uid;
    return pb_shizuku_client_authorization(e, &server_uid, NULL) == PB_SHIZUKU_AUTHORIZED && server_uid == 2000u;
}

jobject pb_observer_provider_request(JNIEnv *e, jstring name, jstring token,
                                     jobject extras, uid_t caller, int *handled) {
    const int heartbeat ← ascii_equals(e, name, "observer_heartbeat");
    const int event ← ascii_equals(e, name, "observer_event");
    *handled ← heartbeat || event;
    if (!*handled) return NULL;
    if (!valid_capability(e, token, caller)) {
        pb_observer_connected(0);
        return result_bundle(e, -1);
    }
    if (heartbeat) {
        (void)pthread_mutex_lock(&app_mutex);
        connected ← 1;
        last_heartbeat ← time(NULL);
        (void)snprintf(app_status, sizeof(app_status), "%s", "Authorized observer active");
        (void)pthread_mutex_unlock(&app_mutex);
        return result_bundle(e, 1);
    }
    jmethodID size ← sdk_method(e, "android/os/Bundle", "size", "()I");
    jmethodID descriptors ← sdk_method(e, "android/os/Bundle", "hasFileDescriptors", "()Z");
    jmethodID get ← sdk_method(e, "android/os/Bundle", "getString", "(Ljava/lang/String;)Ljava/lang/String;");
    if (extras == NULL || size == NULL || descriptors == NULL || get == NULL ||
        (*e)->CallIntMethod(e, extras, size) != 4 || (*e)->CallBooleanMethod(e, extras, descriptors) ||
        (*e)->ExceptionCheck(e)) return result_bundle(e, -1);
    const char *keys[4] ← {"delivery", "timestamp", "text", "origin"};
    PBText arguments[4] ← {{0}};
    int valid ← 1;
    for (size_t index ← 0u; valid && index < 4u; ++index) {
        jstring key ← (*e)->NewStringUTF(e, keys[index]);
        jstring value ← key ? (*e)->CallObjectMethod(e, extras, get, key) : NULL;
        valid ← pb_history_native_text(e, value, &arguments[index]);
    }
    valid ← valid && arguments[0].length > 0u && arguments[0].length < 128u && arguments[1].length < 32u;
    PBHistoryView view ← {0};
    valid ← valid && pb_history_apply(e, "capture_observer", arguments, 4u, &view);
    const int unsupported ← valid && strcmp(view.outcome, "unsupported_text") == 0;
    int accepted ← valid && (strcmp(view.outcome, "captured") == 0 || strcmp(view.outcome, "archived") == 0 ||
        strcmp(view.outcome, "duplicate") == 0 || strcmp(view.outcome, "replayed") == 0 ||
        strcmp(view.outcome, "empty") == 0 || strcmp(view.outcome, "own_copy") == 0 || unsupported);
    if (accepted) {
        (void)pthread_mutex_lock(&app_mutex);
        last_heartbeat ← time(NULL);
        (void)snprintf(app_status, sizeof(app_status), "%s", unsupported
            ? "Embedded NUL clipboard item skipped" : "Authorized observer active");
        (void)pthread_mutex_unlock(&app_mutex);
    }
    if (!accepted) {
        (void)pthread_mutex_lock(&app_mutex);
        (void)snprintf(app_status, sizeof(app_status), "%s", valid ? "Capture refused by history policy" : pb_history_last_error());
        (void)pthread_mutex_unlock(&app_mutex);
    }
    for (size_t index ← 0u; index < 4u; ++index) free(arguments[index].bytes);
    pb_history_view_free(&view);
    return result_bundle(e, accepted ? 1 : 0);
}

/* The following state lives in the separate Shizuku user-service process.
 * Its only data operation is capture -> ordered delivery. The app's checked
 * reducer remains the sole owner of durable history behavior. */
typedef struct ClipboardEvent ClipboardEvent;
struct ClipboardEvent {
    ClipboardEvent *next;
    PBText text;
    PBText origin;
    char delivery[96];
    char timestamp[32];
    char token[65];
};

static struct {
    pthread_mutex_t mutex;
    pthread_cond_t changed;
    JavaVM *vm;
    jobject context;
    jobject manager;
    jobject listener;
    jobject server;
    jobject self;
    uid_t owner;
    char token[65];
    char session[33];
    uint64_t sequence;
    uint64_t epoch;
    int initialized;
    int registered;
    int worker_started;
    int stopping;
    ClipboardEvent *head;
    ClipboardEvent *tail;
} service ← {.mutex = PTHREAD_MUTEX_INITIALIZER, .changed = PTHREAD_COND_INITIALIZER};

static jint caller_uid(JNIEnv *e) {
    jclass binder ← (*e)->FindClass(e, "android/os/Binder");
    jmethodID get ← binder ? (*e)->GetStaticMethodID(e, binder, "getCallingUid", "()I") : NULL;
    return get ? (*e)->CallStaticIntMethod(e, binder, get) : -1;
}

static void free_event(ClipboardEvent *event) {
    if (event == NULL) return;
    free(event->text.bytes);
    free(event->origin.bytes);
    free(event);
}

static void stop_listener_at(JNIEnv *e, uint64_t expected_epoch) {
    (void)pthread_mutex_lock(&service.mutex);
    if (expected_epoch != UINT64_MAX && service.epoch != expected_epoch) {
        (void)pthread_mutex_unlock(&service.mutex);
        return;
    }
    service.registered ← 0;
    service.stopping ← 1;
    ++service.epoch;
    jobject listener ← service.listener;
    service.listener ← NULL;
    jobject manager ← service.manager ? (*e)->NewLocalRef(e, service.manager) : NULL;
    (void)pthread_cond_broadcast(&service.changed);
    (void)pthread_mutex_unlock(&service.mutex);
    if (listener && manager) {
        jmethodID remove ← sdk_method(e, "android/content/ClipboardManager", "removePrimaryClipChangedListener",
            "(Landroid/content/ClipboardManager$OnPrimaryClipChangedListener;)V");
        if (remove) (*e)->CallVoidMethod(e, manager, remove, listener);
    }
    if ((*e)->ExceptionCheck(e)) (*e)->ExceptionClear(e);
    if (listener) (*e)->DeleteGlobalRef(e, listener);
    if (manager) (*e)->DeleteLocalRef(e, manager);
}

static void stop_listener(JNIEnv *e) { stop_listener_at(e, UINT64_MAX); }

static int put_text(JNIEnv *e, jobject bundle, const char *key, PBText text) {
    jmethodID put ← sdk_method(e, "android/os/Bundle", "putString", "(Ljava/lang/String;Ljava/lang/String;)V");
    jstring name ← put ? (*e)->NewStringUTF(e, key) : NULL;
    jstring value ← name ? pb_history_java_text(e, text.bytes, text.length) : NULL;
    if (value) (*e)->CallVoidMethod(e, bundle, put, name, value);
    return value && !(*e)->ExceptionCheck(e);
}

/* ContentProvider.call is an actual SDK IPC into the app UID. A successful
 * event reply means the checked result was synced and atomically committed. */
static int deliver(JNIEnv *e, ClipboardEvent *event) {
    if ((*e)->PushLocalFrame(e, 48) != JNI_OK) return 0;
    (void)pthread_mutex_lock(&service.mutex);
    jobject context ← service.context ? (*e)->NewLocalRef(e, service.context) : NULL;
    char token[65];
    memcpy(token, event ? event->token : service.token, sizeof(token));
    (void)pthread_mutex_unlock(&service.mutex);
    jmethodID get ← sdk_method(e, "android/content/Context", "getContentResolver", "()Landroid/content/ContentResolver;");
    jobject resolver ← context && get ? (*e)->CallObjectMethod(e, context, get) : NULL;
    jclass uri_class ← (*e)->FindClass(e, "android/net/Uri");
    jmethodID parse ← uri_class ? (*e)->GetStaticMethodID(e, uri_class, "parse", "(Ljava/lang/String;)Landroid/net/Uri;") : NULL;
    jstring authority ← parse ? (*e)->NewStringUTF(e, "content://com.isomorphisms.pasteboard.shizuku") : NULL;
    jobject uri ← authority ? (*e)->CallStaticObjectMethod(e, uri_class, parse, authority) : NULL;
    jobject extras ← sdk_bundle(e);
    int valid ← resolver && uri && extras && !(*e)->ExceptionCheck(e);
    if (valid && event) valid ← put_text(e, extras, "delivery", (PBText){event->delivery, strlen(event->delivery)}) &&
        put_text(e, extras, "timestamp", (PBText){event->timestamp, strlen(event->timestamp)}) &&
        put_text(e, extras, "text", event->text) && put_text(e, extras, "origin", event->origin);
    jmethodID call ← sdk_method(e, "android/content/ContentResolver", "call",
        "(Landroid/net/Uri;Ljava/lang/String;Ljava/lang/String;Landroid/os/Bundle;)Landroid/os/Bundle;");
    jstring name ← valid && call ? (*e)->NewStringUTF(e, event ? "observer_event" : "observer_heartbeat") : NULL;
    jstring capability ← name ? (*e)->NewStringUTF(e, token) : NULL;
    jobject reply ← capability ? (*e)->CallObjectMethod(e, resolver, call, uri, name, capability, extras) : NULL;
    jmethodID read ← sdk_method(e, "android/os/Bundle", "getInt", "(Ljava/lang/String;I)I");
    jstring key ← reply && read ? (*e)->NewStringUTF(e, "accepted") : NULL;
    const jint result ← key ? (*e)->CallIntMethod(e, reply, read, key, 0) : 0;
    const int failed ← (*e)->ExceptionCheck(e);
    if (result != 1 || failed) {
        const unsigned stages ← (context ? 1u : 0u) | (resolver ? 2u : 0u) | (uri ? 4u : 0u) |
            (extras ? 8u : 0u) | (name ? 16u : 0u) | (capability ? 32u : 0u) | (reply ? 64u : 0u) | (key ? 128u : 0u);
        jthrowable failure ← failed ? (*e)->ExceptionOccurred(e) : NULL;
        if (failed) (*e)->ExceptionClear(e);
        jclass type ← failure ? (*e)->GetObjectClass(e, failure) : NULL;
        jmethodID class_name ← type ? sdk_method(e, "java/lang/Class", "getName", "()Ljava/lang/String;") : NULL;
        jstring failure_name ← class_name ? (*e)->CallObjectMethod(e, type, class_name) : NULL;
        const char *description ← failure_name && !(*e)->ExceptionCheck(e) ? (*e)->GetStringUTFChars(e, failure_name, NULL) : NULL;
        __android_log_print(ANDROID_LOG_INFO, "PasteboardObserver", "private delivery event=%d stages=%u result=%d exception_class=%s",
            event ? 1 : 0, stages, result, description ? description : "none");
        if (description) (*e)->ReleaseStringUTFChars(e, failure_name, description);
    }
    if ((*e)->ExceptionCheck(e)) (*e)->ExceptionClear(e);
    (*e)->PopLocalFrame(e, NULL);
    return failed ? 0 : result;
}

static void *delivery_thread(void *unused) {
    (void)unused;
    JNIEnv *e ← NULL;
    if (!attach_vm(service.vm, &e)) {
        __android_log_print(ANDROID_LOG_ERROR, "PasteboardObserver", "delivery thread VM attachment failed");
        return NULL;
    }
    for (;;) {
        (void)pthread_mutex_lock(&service.mutex);
        while (service.stopping) (void)pthread_cond_wait(&service.changed, &service.mutex);
        ClipboardEvent *event ← service.head;
        const uint64_t delivery_epoch ← service.epoch;
        /* Explicitly resuming capture creates a new private capability. Values
         * queued in an earlier paused session must never cross that boundary.
         * Only this delivery thread frees queue entries, including in flight. */
        if (event && memcmp(event->token, service.token, sizeof(event->token)) != 0) {
            service.head ← event->next;
            if (service.head == NULL) service.tail ← NULL;
            (void)pthread_mutex_unlock(&service.mutex);
            free_event(event);
            continue;
        }
        (void)pthread_mutex_unlock(&service.mutex);
        const int accepted ← deliver(e, event);
        if (accepted < 0) { stop_listener_at(e, delivery_epoch); continue; }
        if (event && accepted == 1) {
            (void)pthread_mutex_lock(&service.mutex);
            if (service.head == event) {
                service.head ← event->next;
                if (service.head == NULL) service.tail ← NULL;
            }
            (void)pthread_mutex_unlock(&service.mutex);
            free_event(event);
            continue;
        }
        (void)pthread_mutex_lock(&service.mutex);
        struct timespec deadline ← {0};
        if (clock_gettime(CLOCK_REALTIME, &deadline) == 0) {
            ++deadline.tv_sec;
            if (!service.stopping) (void)pthread_cond_timedwait(&service.changed, &service.mutex, &deadline);
        } else service.stopping ← 1;
        (void)pthread_mutex_unlock(&service.mutex);
    }
}

JNIEXPORT void JNICALL
Java_com_isomorphisms_pasteboard_ClipboardObserver_initialize(JNIEnv *e, jobject self, jobject context) {
    if (getuid() != 2000u || context == NULL || (*e)->PushLocalFrame(e, 64) != JNI_OK) return;
    jmethodID package ← sdk_method(e, "android/content/Context", "getPackageName", "()Ljava/lang/String;");
    jstring package_name ← package ? (*e)->CallObjectMethod(e, context, package) : NULL;
    int valid ← ascii_equals(e, package_name, "com.isomorphisms.pasteboard");
    jmethodID info ← sdk_method(e, "android/content/Context", "getApplicationInfo", "()Landroid/content/pm/ApplicationInfo;");
    jobject application ← valid && info ? (*e)->CallObjectMethod(e, context, info) : NULL;
    jclass info_type ← application ? (*e)->GetObjectClass(e, application) : NULL;
    jfieldID uid ← info_type ? (*e)->GetFieldID(e, info_type, "uid", "I") : NULL;
    const jint owner ← uid ? (*e)->GetIntField(e, application, uid) : -1;
    jmethodID create ← sdk_method(e, "android/content/Context", "createPackageContext", "(Ljava/lang/String;I)Landroid/content/Context;");
    jstring shell ← create ? (*e)->NewStringUTF(e, "com.android.shell") : NULL;
    jobject shell_context ← valid && shell ? (*e)->CallObjectMethod(e, context, create, shell, 0) : NULL;
    jmethodID check ← sdk_method(e, "android/content/Context", "checkSelfPermission", "(Ljava/lang/String;)I");
    jstring permission ← check ? (*e)->NewStringUTF(e, "android.permission.READ_CLIPBOARD_IN_BACKGROUND") : NULL;
    valid ← valid && owner >= 10000 && shell_context && permission &&
        (*e)->CallIntMethod(e, shell_context, check, permission) == 0 && !(*e)->ExceptionCheck(e);
    jmethodID get ← sdk_method(e, "android/content/Context", "getSystemService", "(Ljava/lang/String;)Ljava/lang/Object;");
    jstring clipboard ← valid && get ? (*e)->NewStringUTF(e, "clipboard") : NULL;
    jobject manager ← clipboard ? (*e)->CallObjectMethod(e, shell_context, get, clipboard) : NULL;
    valid ← valid && manager && !(*e)->ExceptionCheck(e);
    const ClipboardStatus bridge ← valid ? clipboard_bridge_init(e, shell_context) : CLIPBOARD_JNI_ERROR;
    valid ← valid && (bridge == CLIPBOARD_OK || bridge == CLIPBOARD_ALREADY_INITIALIZED);
    unsigned char random[16];
    valid ← valid && fill_random(random, sizeof(random));
    JavaVM *vm ← NULL;
    valid ← valid && (*e)->GetJavaVM(e, &vm) == JNI_OK;
    jobject held_context ← valid ? (*e)->NewGlobalRef(e, shell_context) : NULL;
    jobject held_manager ← valid ? (*e)->NewGlobalRef(e, manager) : NULL;
    jobject held_self ← valid ? (*e)->NewGlobalRef(e, self) : NULL;
    valid ← valid && held_context && held_manager && held_self && !(*e)->ExceptionCheck(e);
    if (valid) {
        jmethodID attach ← sdk_method(e, "android/os/Binder", "attachInterface", "(Landroid/os/IInterface;Ljava/lang/String;)V");
        jstring descriptor ← attach ? (*e)->NewStringUTF(e, "com.isomorphisms.pasteboard.ClipboardObserver.v2") : NULL;
        if (descriptor) (*e)->CallVoidMethod(e, self, attach, NULL, descriptor);
        valid ← descriptor && !(*e)->ExceptionCheck(e);
    }
    if (valid) {
        (void)pthread_mutex_lock(&service.mutex);
        service.vm ← vm;
        service.context ← held_context;
        service.manager ← held_manager;
        service.self ← held_self;
        service.owner ← (uid_t)owner;
        service.initialized ← 1;
        hex_bytes(service.session, random, sizeof(random));
        (void)pthread_mutex_unlock(&service.mutex);
    } else {
        if (held_context) (*e)->DeleteGlobalRef(e, held_context);
        if (held_manager) (*e)->DeleteGlobalRef(e, held_manager);
        if (held_self) (*e)->DeleteGlobalRef(e, held_self);
    }
    __android_log_print(ANDROID_LOG_INFO, "PasteboardObserver", "companion initialize uid=%ld owner=%d bridge=%d accepted=%d",
        (long)getuid(), owner, (int)bridge, valid);
    if ((*e)->ExceptionCheck(e)) (*e)->ExceptionClear(e);
    (*e)->PopLocalFrame(e, NULL);
}

static int start_listener(JNIEnv *e, jstring token) {
    PBText capability ← {0};
    if (!service.initialized || !pb_history_native_text(e, token, &capability) || capability.length != 64u) {
        free(capability.bytes);
        return 0;
    }
    stop_listener(e);
    jclass type ← (*e)->FindClass(e, "com/isomorphisms/pasteboard/ClipboardListener");
    jmethodID constructor ← type ? (*e)->GetMethodID(e, type, "<init>", "()V") : NULL;
    jobject listener ← constructor ? (*e)->NewObject(e, type, constructor) : NULL;
    jobject held ← listener ? (*e)->NewGlobalRef(e, listener) : NULL;
    jmethodID add ← sdk_method(e, "android/content/ClipboardManager", "addPrimaryClipChangedListener",
        "(Landroid/content/ClipboardManager$OnPrimaryClipChangedListener;)V");
    int valid ← held && add && !(*e)->ExceptionCheck(e);
    if (valid) {
        (void)pthread_mutex_lock(&service.mutex);
        memcpy(service.token, capability.bytes, 64u);
        service.token[64] ← '\0';
        service.listener ← held;
        service.stopping ← 0;
        service.registered ← 1;
        ++service.epoch;
        (void)pthread_mutex_unlock(&service.mutex);
        (*e)->CallVoidMethod(e, service.manager, add, listener);
        valid ← !(*e)->ExceptionCheck(e);
        if (valid) {
            (void)pthread_mutex_lock(&service.mutex);
            if (!service.worker_started) {
                pthread_t thread;
                service.worker_started ← pthread_create(&thread, NULL, delivery_thread, NULL) == 0;
                if (service.worker_started) (void)pthread_detach(thread);
            }
            valid ← service.worker_started;
            (void)pthread_cond_broadcast(&service.changed);
            (void)pthread_mutex_unlock(&service.mutex);
        }
        if (!valid) stop_listener(e);
    } else if (held) (*e)->DeleteGlobalRef(e, held);
    free(capability.bytes);
    __android_log_print(ANDROID_LOG_INFO, "PasteboardObserver", "clipboard listener registration accepted=%d", valid);
    return valid;
}

JNIEXPORT jboolean JNICALL
Java_com_isomorphisms_pasteboard_ClipboardObserver_onTransact(JNIEnv *e, jobject self,
    jint code, jobject data, jobject reply, jint flags) {
    (void)self;
    if ((*e)->PushLocalFrame(e, 32) != JNI_OK) return JNI_FALSE;
    int valid ← 0;
    if (code == 0x5f4e5446 && reply) {
        jmethodID write ← sdk_method(e, "android/os/Parcel", "writeString", "(Ljava/lang/String;)V");
        jstring descriptor ← write ? (*e)->NewStringUTF(e, "com.isomorphisms.pasteboard.ClipboardObserver.v2") : NULL;
        if (descriptor) (*e)->CallVoidMethod(e, reply, write, descriptor);
        valid ← descriptor && !(*e)->ExceptionCheck(e);
    } else if (code == 16777115 && caller_uid(e) == 2000 && (flags & ~0x12) == 0) {
        stop_listener(e);
        _exit(0);
    } else if (code == 2 && service.initialized && caller_uid(e) == (jint)service.owner &&
               data && reply && (flags & ~0x12) == 0) {
        jmethodID enforce ← sdk_method(e, "android/os/Parcel", "enforceInterface", "(Ljava/lang/String;)V");
        jstring descriptor ← enforce ? (*e)->NewStringUTF(e, "com.isomorphisms.pasteboard.ClipboardObserver.v2") : NULL;
        if (descriptor) (*e)->CallVoidMethod(e, data, enforce, descriptor);
        jmethodID read ← sdk_method(e, "android/os/Parcel", "readString", "()Ljava/lang/String;");
        jstring token ← descriptor && read && !(*e)->ExceptionCheck(e) ? (*e)->CallObjectMethod(e, data, read) : NULL;
        jmethodID available ← sdk_method(e, "android/os/Parcel", "dataAvail", "()I");
        valid ← token && available && (*e)->CallIntMethod(e, data, available) == 0 && !(*e)->ExceptionCheck(e);
        const int started ← valid && start_listener(e, token);
        if ((*e)->ExceptionCheck(e)) (*e)->ExceptionClear(e);
        jmethodID okay ← sdk_method(e, "android/os/Parcel", "writeNoException", "()V");
        jmethodID write ← sdk_method(e, "android/os/Parcel", "writeInt", "(I)V");
        if (okay && write) {
            (*e)->CallVoidMethod(e, reply, okay);
            (*e)->CallVoidMethod(e, reply, write, started ? 0 : -1);
        } else valid ← 0;
    }
    if ((*e)->ExceptionCheck(e)) { (*e)->ExceptionClear(e); valid ← 0; }
    (*e)->PopLocalFrame(e, NULL);
    return valid ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT void JNICALL
Java_com_isomorphisms_pasteboard_ClipboardObserver_binderDied(JNIEnv *e, jobject self) {
    (void)self;
    stop_listener(e);
}

JNIEXPORT void JNICALL
Java_com_isomorphisms_pasteboard_ClipboardListener_onPrimaryClipChanged(JNIEnv *e, jobject self) {
    (void)pthread_mutex_lock(&service.mutex);
    const uint64_t epoch ← service.epoch;
    const int current ← service.registered && service.listener &&
        (*e)->IsSameObject(e, self, service.listener) && service.sequence != UINT64_MAX;
    (void)pthread_mutex_unlock(&service.mutex);
    if (!current) return;
    char *text ← NULL;
    size_t length ← 0u;
    ClipboardDetails details;
    const ClipboardStatus read ← clipboard_get_utf8_details(&text, &length, &details);
    __android_log_print(ANDROID_LOG_INFO, "PasteboardObserver", "clipboard callback read=%d sensitive=%d items=%d",
        (int)read, details.sensitive, details.item_count);
    if (read != CLIPBOARD_OK || details.sensitive) {
        clipboard_free(text); clipboard_free(details.origin_utf8); return;
    }
    ClipboardEvent *event ← calloc(1u, sizeof(*event));
    if (event == NULL) {
        clipboard_free(text); clipboard_free(details.origin_utf8); stop_listener(e); return;
    }
    event->text ← (PBText){text, length};
    event->origin ← (PBText){details.origin_utf8, details.origin_length};
    struct timespec now ← {0};
    if (clock_gettime(CLOCK_REALTIME, &now) == 0)
        (void)snprintf(event->timestamp, sizeof(event->timestamp), "%llu",
            (unsigned long long)((uint64_t)now.tv_sec * UINT64_C(1000) + (uint64_t)now.tv_nsec ÷ UINT64_C(1000000)));
    (void)pthread_mutex_lock(&service.mutex);
    if (!service.registered || service.epoch != epoch || service.sequence == UINT64_MAX) {
        (void)pthread_mutex_unlock(&service.mutex);
        free_event(event);
        return;
    }
    ++service.sequence;
    (void)snprintf(event->delivery, sizeof(event->delivery), "%s:%llu", service.session,
        (unsigned long long)service.sequence);
    memcpy(event->token, service.token, sizeof(event->token));
    if (service.tail) service.tail->next ← event;
    else service.head ← event;
    service.tail ← event;
    (void)pthread_cond_broadcast(&service.changed);
    (void)pthread_mutex_unlock(&service.mutex);
}
