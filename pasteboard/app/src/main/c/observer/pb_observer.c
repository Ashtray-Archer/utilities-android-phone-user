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
static int64_t app_capture_since ← 0;
static char app_status[128] ← "Foreground capture only";
static int connecting ← 0;
static int connected ← 0;
static time_t last_heartbeat ← 0;
static pthread_once_t status_once ← PTHREAD_ONCE_INIT;
static pthread_key_t status_key;
static int status_key_ready ← 0;
static jint platform_api(JNIEnv *environment);

static int qualified_platform(JNIEnv *e) {
    const jint api ← platform_api(e);
    const int supported ← !(*e)->ExceptionCheck(e) && (api == 29 || api == 34) &&
        getuid() >= 10000u && getuid() < 100000u;
    if ((*e)->ExceptionCheck(e)) (*e)->ExceptionClear(e);
    if (!supported) {
        (void)pthread_mutex_lock(&app_mutex);
        (void)snprintf(app_status, sizeof(app_status), "%s", "Automatic capture unavailable on this Android version or user profile");
        (void)pthread_mutex_unlock(&app_mutex);
    }
    return supported;
}

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
static int load_capability(int create, char output[65], int64_t *capture_since) {
    *capture_since ← 0;
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
    if (valid && offset == 64u && information.st_mtim.tv_sec > 0)
        *capture_since ← (int64_t)information.st_mtim.tv_sec * INT64_C(1000) + information.st_mtim.tv_nsec ÷ INT64_C(1000000);
    return valid && offset == 64u && *capture_since > 0;
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
    const int64_t capture_since ← app_capture_since;
    (void)pthread_mutex_unlock(&app_mutex);
    JNIEnv *environment ← NULL;
    if (vm && attach_vm(vm, &environment)) {
        (void)pb_shizuku_observer_connect(environment, token, capture_since);
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
    int64_t capture_since ← 0;
    const int enabled ← load_capability(0, token, &capture_since);
    (void)pthread_mutex_lock(&app_mutex);
    app_vm ← vm;
    if (enabled) { memcpy(app_token, token, sizeof(token)); app_capture_since ← capture_since; }
    (void)pthread_mutex_unlock(&app_mutex);
}

void pb_observer_try_start(JNIEnv *e) {
    if (!pb_history_capture_enabled() || !pb_history_automatic_requested()) return;
    if (!qualified_platform(e)) return;
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
    int64_t capture_since ← 0;
    if (!load_capability(2, token, &capture_since)) return false;
    (void)pthread_mutex_lock(&app_mutex);
    memcpy(app_token, token, sizeof(token));
    app_capture_since ← capture_since;
    connected ← 0;
    (void)pthread_mutex_unlock(&app_mutex);
    return true;
}

int pb_observer_enable(JNIEnv *e) {
    if (!qualified_platform(e)) return -2;
    char token[65];
    int64_t capture_since ← 0;
    if (!load_capability(1, token, &capture_since)) return -1;
    (void)pthread_mutex_lock(&app_mutex);
    memcpy(app_token, token, sizeof(token));
    app_capture_since ← capture_since;
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
    char delivery[128];
    char timestamp[24];
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
    int64_t capture_since;
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

/* Platform event identity only. The checked reducer owns replay and text
 * deduplication. Metadata/classification callbacks share the SDK copy time and
 * exact text/origin digest; distinct same-millisecond texts have distinct IDs. */
static int event_fingerprint(JNIEnv *e, PBText text, PBText origin, char output[65]) {
    if (text.length > INT32_MAX || origin.length > INT32_MAX ||
        (*e)->PushLocalFrame(e, 24) != JNI_OK) return 0;
    jclass type ← (*e)->FindClass(e, "java/security/MessageDigest");
    jmethodID create ← type ? (*e)->GetStaticMethodID(e, type, "getInstance", "(Ljava/lang/String;)Ljava/security/MessageDigest;") : NULL;
    jstring algorithm ← create ? (*e)->NewStringUTF(e, "SHA-256") : NULL;
    jobject digest ← algorithm ? (*e)->CallStaticObjectMethod(e, type, create, algorithm) : NULL;
    jmethodID update ← sdk_method(e, "java/security/MessageDigest", "update", "([B)V");
    jmethodID finish ← sdk_method(e, "java/security/MessageDigest", "digest", "()[B");
    unsigned char lengths[16];
    const uint64_t sizes[2] ← {(uint64_t)text.length, (uint64_t)origin.length};
    for (size_t field ← 0u; field < 2u; ++field)
        for (unsigned index ← 0u; index < 8u; ++index)
            lengths[field * 8u + index] ← (unsigned char)(sizes[field] >> (index * 8u));
    PBText pieces[3] ← {{(char *)lengths, sizeof(lengths)}, text, origin};
    int valid ← digest && update && finish && !(*e)->ExceptionCheck(e);
    for (size_t index ← 0u; valid && index < 3u; ++index) {
        jbyteArray bytes ← (*e)->NewByteArray(e, (jsize)pieces[index].length);
        if (bytes && pieces[index].length != 0u)
            (*e)->SetByteArrayRegion(e, bytes, 0, (jsize)pieces[index].length, (const jbyte *)pieces[index].bytes);
        valid ← bytes && !(*e)->ExceptionCheck(e);
        if (valid) (*e)->CallVoidMethod(e, digest, update, bytes);
        valid ← valid && !(*e)->ExceptionCheck(e);
        if (bytes) (*e)->DeleteLocalRef(e, bytes);
    }
    jbyteArray value ← valid ? (*e)->CallObjectMethod(e, digest, finish) : NULL;
    valid ← value && !(*e)->ExceptionCheck(e) && (*e)->GetArrayLength(e, value) == 32;
    unsigned char bytes[32];
    if (valid) {
        (*e)->GetByteArrayRegion(e, value, 0, 32, (jbyte *)bytes);
        valid ← !(*e)->ExceptionCheck(e);
        if (valid) hex_bytes(output, bytes, sizeof(bytes));
    }
    (*e)->PopLocalFrame(e, NULL);
    return valid;
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

static jint platform_api(JNIEnv *e) {
    jclass version ← (*e)->FindClass(e, "android/os/Build$VERSION");
    jfieldID sdk ← version ? (*e)->GetStaticFieldID(e, version, "SDK_INT", "I") : NULL;
    return sdk ? (*e)->GetStaticIntField(e, version, sdk) : -1;
}

/* createPackageContext carries its container's operation package. Recreate
 * this genuine shell LoadedApk without a container, as AOSP createAppContext
 * does, so both public clipboard package arguments match actual UID 2000. */
static jobject shell_application_context(JNIEnv *e, jobject context) {
    const jint api ← platform_api(e);
    if (getuid() != 2000u || (api != 29 && api != 34) || (*e)->ExceptionCheck(e)) return NULL;
    jmethodID create ← sdk_method(e, "android/content/Context", "createPackageContext", "(Ljava/lang/String;I)Landroid/content/Context;");
    jstring shell ← create ? (*e)->NewStringUTF(e, "com.android.shell") : NULL;
    jobject carried ← shell ? (*e)->CallObjectMethod(e, context, create, shell, 0) : NULL;
    if (!carried || (*e)->ExceptionCheck(e)) return NULL;
    jclass implementation ← (*e)->FindClass(e, "android/app/ContextImpl");
    jfieldID field ← implementation ? (*e)->GetFieldID(e, implementation, "mPackageInfo", "Landroid/app/LoadedApk;") : NULL;
    jobject loaded ← field ? (*e)->GetObjectField(e, carried, field) : NULL;
    if (!loaded || (*e)->ExceptionCheck(e)) return NULL;
    jclass thread_type ← (*e)->FindClass(e, "android/app/ActivityThread");
    jmethodID current ← thread_type ? (*e)->GetStaticMethodID(e, thread_type, "currentActivityThread", "()Landroid/app/ActivityThread;") : NULL;
    jobject thread ← current ? (*e)->CallStaticObjectMethod(e, thread_type, current) : NULL;
    if (!thread || (*e)->ExceptionCheck(e)) return NULL;
    jmethodID fresh ← (*e)->GetStaticMethodID(e, implementation, "createAppContext",
        "(Landroid/app/ActivityThread;Landroid/app/LoadedApk;)Landroid/app/ContextImpl;");
    return fresh ? (*e)->CallStaticObjectMethod(e, implementation, fresh, thread, loaded) : NULL;
}

/* A Shizuku user service has no AMS application record. Its Context resolver
 * cannot acquire providers. Follow AOSP's shell content command with an
 * explicitly bounded framework interface, not a guessed transaction number.
 * See observer-boundary.md for exact API 29/34 source and ABI signatures. */
static jobject shell_provider_call(JNIEnv *e, jstring method, jstring token, jobject extras) {
    const jint api ← platform_api(e);
    if (getuid() != 2000u || (api != 29 && api != 34) || (*e)->ExceptionCheck(e)) return NULL;
    (void)pthread_mutex_lock(&service.mutex);
    const uid_t owner ← service.owner;
    (void)pthread_mutex_unlock(&service.mutex);
    if (owner < 10000u || owner >= 100000u) return NULL;
    jclass activity ← (*e)->FindClass(e, "android/app/ActivityManager");
    jmethodID get ← activity ? (*e)->GetStaticMethodID(e, activity, "getService", "()Landroid/app/IActivityManager;") : NULL;
    jobject manager ← get ? (*e)->CallStaticObjectMethod(e, activity, get) : NULL;
    if (!manager || (*e)->ExceptionCheck(e)) return NULL;
    jclass binder_type ← (*e)->FindClass(e, "android/os/Binder");
    jmethodID construct ← binder_type ? (*e)->GetMethodID(e, binder_type, "<init>", "()V") : NULL;
    jobject lifetime ← construct ? (*e)->NewObject(e, binder_type, construct) : NULL;
    if (!lifetime || (*e)->ExceptionCheck(e)) return NULL;
    jstring authority ← (*e)->NewStringUTF(e, "com.isomorphisms.pasteboard.shizuku");
    if (!authority) return NULL;
    jstring tag ← (*e)->NewStringUTF(e, "pasteboard-observer");
    if (!tag) return NULL;
    jmethodID acquire ← sdk_method(e, "android/app/IActivityManager", "getContentProviderExternal",
        "(Ljava/lang/String;ILandroid/os/IBinder;Ljava/lang/String;)Landroid/app/ContentProviderHolder;");
    jmethodID release ← sdk_method(e, "android/app/IActivityManager", "removeContentProviderExternalAsUser",
        "(Ljava/lang/String;Landroid/os/IBinder;I)V");
    jobject holder ← manager && lifetime && authority && tag && acquire && release && !(*e)->ExceptionCheck(e)
        ? (*e)->CallObjectMethod(e, manager, acquire, authority, 0, lifetime, tag) : NULL;
    jclass holder_type ← holder ? (*e)->FindClass(e, "android/app/ContentProviderHolder") : NULL;
    jfieldID provider_field ← holder_type ? (*e)->GetFieldID(e, holder_type, "provider", "Landroid/content/IContentProvider;") : NULL;
    jobject provider ← provider_field && !(*e)->ExceptionCheck(e) ? (*e)->GetObjectField(e, holder, provider_field) : NULL;
    jstring package ← provider ? (*e)->NewStringUTF(e, "com.android.shell") : NULL;
    jobject result ← NULL;
    if (provider && package && !(*e)->ExceptionCheck(e) && api == 29) {
        jmethodID call ← sdk_method(e, "android/content/IContentProvider", "call",
            "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Landroid/os/Bundle;)Landroid/os/Bundle;");
        if (call) result ← (*e)->CallObjectMethod(e, provider, call, package, authority, method, token, extras);
    } else if (provider && package && !(*e)->ExceptionCheck(e) && api == 34) {
        jclass attribution ← (*e)->FindClass(e, "android/content/AttributionSource");
        jmethodID create ← attribution ? (*e)->GetMethodID(e, attribution, "<init>", "(ILjava/lang/String;Ljava/lang/String;)V") : NULL;
        jobject source ← create ? (*e)->NewObject(e, attribution, create, 2000, package, NULL) : NULL;
        jmethodID call ← sdk_method(e, "android/content/IContentProvider", "call",
            "(Landroid/content/AttributionSource;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Landroid/os/Bundle;)Landroid/os/Bundle;");
        if (source && call && !(*e)->ExceptionCheck(e))
            result ← (*e)->CallObjectMethod(e, provider, call, source, authority, method, token, extras);
    }
    /* Release the external reference even if the writer throws or dies.
     * Preserve only the throwable class for the caller's content-free trace. */
    jthrowable failure ← (*e)->ExceptionOccurred(e);
    if (failure) (*e)->ExceptionClear(e);
    if (holder && manager && release) (*e)->CallVoidMethod(e, manager, release, authority, lifetime, 0);
    if (failure) {
        if ((*e)->ExceptionCheck(e)) (*e)->ExceptionClear(e);
        (void)(*e)->Throw(e, failure);
    }
    return result;
}

/* The version-bounded provider call is actual IPC into the app UID. A successful
 * event reply means the checked result was synced and atomically committed. */
static int deliver(JNIEnv *e, ClipboardEvent *event) {
    if ((*e)->PushLocalFrame(e, 48) != JNI_OK) return 0;
    (void)pthread_mutex_lock(&service.mutex);
    char token[65];
    memcpy(token, event ? event->token : service.token, sizeof(token));
    (void)pthread_mutex_unlock(&service.mutex);
    jobject extras ← sdk_bundle(e);
    int valid ← extras && !(*e)->ExceptionCheck(e);
    if (valid && event) valid ← put_text(e, extras, "delivery", (PBText){event->delivery, strlen(event->delivery)}) &&
        put_text(e, extras, "timestamp", (PBText){event->timestamp, strlen(event->timestamp)}) &&
        put_text(e, extras, "text", event->text) && put_text(e, extras, "origin", event->origin);
    jstring name ← valid ? (*e)->NewStringUTF(e, event ? "observer_event" : "observer_heartbeat") : NULL;
    jstring capability ← name ? (*e)->NewStringUTF(e, token) : NULL;
    jobject reply ← capability ? shell_provider_call(e, name, capability, extras) : NULL;
    jmethodID read ← sdk_method(e, "android/os/Bundle", "getInt", "(Ljava/lang/String;I)I");
    jstring key ← reply && read ? (*e)->NewStringUTF(e, "accepted") : NULL;
    const jint result ← key ? (*e)->CallIntMethod(e, reply, read, key, 0) : 0;
    const int failed ← (*e)->ExceptionCheck(e);
    if (result != 1 || failed) {
        const unsigned stages ← (extras ? 1u : 0u) | (name ? 2u : 0u) | (capability ? 4u : 0u) |
            (reply ? 8u : 0u) | (key ? 16u : 0u);
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
    jobject shell_context ← valid ? shell_application_context(e, context) : NULL;
    jstring shell_package ← shell_context && package ? (*e)->CallObjectMethod(e, shell_context, package) : NULL;
    jmethodID operation ← sdk_method(e, "android/content/Context", "getOpPackageName", "()Ljava/lang/String;");
    jstring operation_package ← shell_context && operation ? (*e)->CallObjectMethod(e, shell_context, operation) : NULL;
    jobject shell_application ← shell_context && info ? (*e)->CallObjectMethod(e, shell_context, info) : NULL;
    const jint shell_uid ← shell_application && uid ? (*e)->GetIntField(e, shell_application, uid) : -1;
    const int package_matches ← ascii_equals(e, shell_package, "com.android.shell");
    const int operation_matches ← ascii_equals(e, operation_package, "com.android.shell");
    jmethodID check ← sdk_method(e, "android/content/Context", "checkSelfPermission", "(Ljava/lang/String;)I");
    jstring permission ← check ? (*e)->NewStringUTF(e, "android.permission.READ_CLIPBOARD_IN_BACKGROUND") : NULL;
    valid ← valid && owner >= 10000 && owner < 100000 && shell_context && permission && shell_uid == 2000 &&
        package_matches && operation_matches &&
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
        jstring descriptor ← attach ? (*e)->NewStringUTF(e, "com.isomorphisms.pasteboard.ClipboardObserver.v3") : NULL;
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
    __android_log_print(ANDROID_LOG_INFO, "PasteboardObserver", "companion initialize uid=%ld owner=%d shell_uid=%d package=%d operation=%d bridge=%d exception=%d accepted=%d",
        (long)getuid(), owner, shell_uid, package_matches, operation_matches, (int)bridge,
        (*e)->ExceptionCheck(e) ? 1 : 0, valid);
    if ((*e)->ExceptionCheck(e)) (*e)->ExceptionClear(e);
    (*e)->PopLocalFrame(e, NULL);
}

static int start_listener(JNIEnv *e, jstring token, jlong capture_since) {
    PBText capability ← {0};
    if (!service.initialized || capture_since <= 0 || !pb_history_native_text(e, token, &capability) || capability.length != 64u) {
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
        service.capture_since ← capture_since;
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
    __android_log_print(ANDROID_LOG_INFO, "PasteboardObserver", "clipboard listener registration accepted=%d caller_uid=%d", valid, caller_uid(e));
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
        jstring descriptor ← write ? (*e)->NewStringUTF(e, "com.isomorphisms.pasteboard.ClipboardObserver.v3") : NULL;
        if (descriptor) (*e)->CallVoidMethod(e, reply, write, descriptor);
        valid ← descriptor && !(*e)->ExceptionCheck(e);
    } else if (code == 16777115 && caller_uid(e) == 2000 && (flags & ~0x12) == 0) {
        stop_listener(e);
        _exit(0);
    } else if (code == 2 && service.initialized && caller_uid(e) == (jint)service.owner &&
               data && reply && (flags & ~0x12) == 0) {
        jmethodID size ← sdk_method(e, "android/os/Parcel", "dataSize", "()I");
        jmethodID descriptors ← sdk_method(e, "android/os/Parcel", "hasFileDescriptors", "()Z");
        const jint bytes ← size ? (*e)->CallIntMethod(e, data, size) : -1;
        if (!size || !descriptors || bytes <= 0 || bytes > 512 ||
            (*e)->CallBooleanMethod(e, data, descriptors) || (*e)->ExceptionCheck(e)) {
            if ((*e)->ExceptionCheck(e)) (*e)->ExceptionClear(e);
            (*e)->PopLocalFrame(e, NULL);
            return JNI_FALSE;
        }
        jmethodID enforce ← sdk_method(e, "android/os/Parcel", "enforceInterface", "(Ljava/lang/String;)V");
        jstring descriptor ← enforce ? (*e)->NewStringUTF(e, "com.isomorphisms.pasteboard.ClipboardObserver.v3") : NULL;
        if (descriptor) (*e)->CallVoidMethod(e, data, enforce, descriptor);
        jmethodID read ← sdk_method(e, "android/os/Parcel", "readString", "()Ljava/lang/String;");
        jstring token ← descriptor && read && !(*e)->ExceptionCheck(e) ? (*e)->CallObjectMethod(e, data, read) : NULL;
        jmethodID read_time ← sdk_method(e, "android/os/Parcel", "readLong", "()J");
        const jlong capture_since ← token && read_time ? (*e)->CallLongMethod(e, data, read_time) : 0;
        jmethodID available ← sdk_method(e, "android/os/Parcel", "dataAvail", "()I");
        valid ← token && read_time && capture_since > 0 && available && (*e)->CallIntMethod(e, data, available) == 0 && !(*e)->ExceptionCheck(e);
        jclass binder ← (*e)->FindClass(e, "android/os/Binder");
        jmethodID clear ← binder ? (*e)->GetStaticMethodID(e, binder, "clearCallingIdentity", "()J") : NULL;
        jmethodID restore ← binder ? (*e)->GetStaticMethodID(e, binder, "restoreCallingIdentity", "(J)V") : NULL;
        valid ← valid && clear && restore && !(*e)->ExceptionCheck(e);
        const jlong identity ← valid ? (*e)->CallStaticLongMethod(e, binder, clear) : 0;
        const int cleared ← valid && !(*e)->ExceptionCheck(e);
        int started ← cleared && start_listener(e, token, capture_since);
        if ((*e)->ExceptionCheck(e)) (*e)->ExceptionClear(e);
        if (cleared) (*e)->CallStaticVoidMethod(e, binder, restore, identity);
        if ((*e)->ExceptionCheck(e)) started ← 0;
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
    const int registered ← service.registered;
    const int same ← service.listener && (*e)->IsSameObject(e, self, service.listener);
    const int64_t capture_since ← service.capture_since;
    const int current ← registered && same;
    (void)pthread_mutex_unlock(&service.mutex);
    __android_log_print(ANDROID_LOG_INFO, "PasteboardObserver", "clipboard callback entry registered=%d current_listener=%d", registered, same);
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
    if (details.copied_at_millis <= 0 || details.copied_at_millis < capture_since) {
        clipboard_free(text); clipboard_free(details.origin_utf8);
        if (details.copied_at_millis <= 0) stop_listener(e);
        return;
    }
    ClipboardEvent *event ← calloc(1u, sizeof(*event));
    if (event == NULL) {
        clipboard_free(text); clipboard_free(details.origin_utf8); stop_listener(e); return;
    }
    event->text ← (PBText){text, length};
    event->origin ← (PBText){details.origin_utf8, details.origin_length};
    char fingerprint[65];
    if (!event_fingerprint(e, event->text, event->origin, fingerprint)) {
        if ((*e)->ExceptionCheck(e)) (*e)->ExceptionClear(e);
        free_event(event); stop_listener(e); return;
    }
    (void)snprintf(event->timestamp, sizeof(event->timestamp), "%llu", (unsigned long long)details.copied_at_millis);
    (void)pthread_mutex_lock(&service.mutex);
    if (!service.registered || service.epoch != epoch) {
        (void)pthread_mutex_unlock(&service.mutex);
        free_event(event);
        return;
    }
    (void)snprintf(event->delivery, sizeof(event->delivery), "%s:%s:%s", service.session, event->timestamp, fingerprint);
    memcpy(event->token, service.token, sizeof(event->token));
    if (service.tail) service.tail->next ← event;
    else service.head ← event;
    service.tail ← event;
    (void)pthread_cond_broadcast(&service.changed);
    (void)pthread_mutex_unlock(&service.mutex);
}
