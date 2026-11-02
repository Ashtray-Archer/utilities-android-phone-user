/* Shizuku API 13 platform wire, pinned at a27f6e4151. Authenticated SDK delivery
 * is retained from IB a0130375d15a46d72f8e4262385315a12a005b03. This boundary
 * owns no clipboard history semantics. */
#include "pb_shizuku_client.h"
#include "pb_binder_flags.h"
#include "pb_shizuku_worker.h"
#include "pb_observer.h"
#include "../pasteboard_history.h"
#include <android/log.h>

#include <errno.h>
#include <pthread.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define PB_PACKAGE "com.isomorphisms.pasteboard"
#define PB_PROVIDER "com.isomorphisms.pasteboard.ShizukuProvider"
#define PB_AUTHORITY PB_PACKAGE ".shizuku"
#define SHIZUKU_MANAGER "moe.shizuku.privileged.api"
#define SHIZUKU_DESCRIPTOR "moe.shizuku.server.IShizukuService"
#define APPLICATION_DESCRIPTOR "moe.shizuku.server.IShizukuApplication"
#define BINDER_EXTRA "moe.shizuku.privileged.api.intent.extra.BINDER"
#define REQUEST_PERMISSION_CODE 151
#define INTERFACE_TRANSACTION 0x5f4e5446
#define WORKER_CONNECTION_DESCRIPTOR "moe.shizuku.server.IShizukuServiceConnection"
#define WORKER_DESCRIPTOR "com.isomorphisms.pasteboard.ClipboardObserver.v3"

static struct {
    pthread_mutex_t mutex;
    pthread_cond_t handlers_finished;
    jobject context, server, application, death;
    uid_t sender_uid;
    uint64_t epoch;
    int attached;
    pb_shizuku_capability_state state;
    pb_shizuku_revocation_handler handler;
    void *handler_owner;
    unsigned handlers_running;
} client = {
    .mutex = PTHREAD_MUTEX_INITIALIZER,
    .handlers_finished = PTHREAD_COND_INITIALIZER
};
static pthread_mutex_t delivery_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t worker_request_mutex = PTHREAD_MUTEX_INITIALIZER;
static struct {
    pthread_mutex_t mutex;
    pthread_cond_t changed;
    jobject connection, service;
    uint64_t epoch;
    uid_t uid;
    int dead;
} worker_connection = {
    .mutex = PTHREAD_MUTEX_INITIALIZER,
    .changed = PTHREAD_COND_INITIALIZER
};

static void unlink_death(JNIEnv *e, jobject server, jobject death) {
    /* Cleanup cannot change the authority decision or replace its exception.
     * DeleteGlobalRef is permitted with a pending exception; other JNI calls
     * need that exception temporarily saved and restored. */
    jthrowable failure = (*e)->ExceptionOccurred(e);
    if (failure) (*e)->ExceptionClear(e);
    if (server && death) {
        jclass type = (*e)->FindClass(e, "android/os/IBinder");
        jmethodID unlink = type ? (*e)->GetMethodID(e, type, "unlinkToDeath",
            "(Landroid/os/IBinder$DeathRecipient;I)Z") : NULL;
        if (unlink) (void)(*e)->CallBooleanMethod(e, server, unlink, death, 0);
        /* A dead Binder or an already removed registration needs no retry. */
        if ((*e)->ExceptionCheck(e)) (*e)->ExceptionClear(e);
    }
    if (failure) (*e)->Throw(e, failure);
}

static jmethodID method(JNIEnv *e, const char *type, const char *name, const char *signature) {
    if ((*e)->ExceptionCheck(e)) return NULL;
    jclass klass = (*e)->FindClass(e, type);
    return klass ? (*e)->GetMethodID(e, klass, name, signature) : NULL;
}

static jobject construct(JNIEnv *e, const char *type) {
    if ((*e)->ExceptionCheck(e)) return NULL;
    jclass klass = NULL;
    if (strncmp(type, "com/isomorphisms/pasteboard/", sizeof("com/isomorphisms/pasteboard/") - 1u) == 0) {
        pthread_mutex_lock(&client.mutex);
        jobject context = client.context ? (*e)->NewLocalRef(e, client.context) : NULL;
        pthread_mutex_unlock(&client.mutex);
        jmethodID get = method(e, "android/content/Context", "getClassLoader", "()Ljava/lang/ClassLoader;");
        jobject loader = context && get ? (*e)->CallObjectMethod(e, context, get) : NULL;
        jmethodID load = method(e, "java/lang/ClassLoader", "loadClass", "(Ljava/lang/String;)Ljava/lang/Class;");
        char dotted[192];
        size_t length = strlen(type);
        if (length >= sizeof(dotted)) return NULL;
        for (size_t index = 0u; index <= length; ++index)
            dotted[index] = type[index] == '/' ? '.' : type[index];
        jstring name = loader && load ? (*e)->NewStringUTF(e, dotted) : NULL;
        if (name) klass = (*e)->CallObjectMethod(e, loader, load, name);
    } else klass = (*e)->FindClass(e, type);
    if (!klass) return NULL;
    jmethodID constructor = (*e)->GetMethodID(e, klass, "<init>", "()V");
    return constructor ? (*e)->NewObject(e, klass, constructor) : NULL;
}

static int text_equals(JNIEnv *e, jstring text, const char *expected) {
    if (!text || (*e)->ExceptionCheck(e)) return 0;
    jsize size = (*e)->GetStringUTFLength(e, text);
    if (size < 0 || (size_t)size != strlen(expected)) return 0;
    const char *bytes = (*e)->GetStringUTFChars(e, text, NULL);
    if (!bytes) return 0;
    int equal = memcmp(bytes, expected, (size_t)size) == 0;
    (*e)->ReleaseStringUTFChars(e, text, bytes);
    return equal;
}

static jint calling_identity(JNIEnv *e, const char *name) {
    if ((*e)->ExceptionCheck(e)) return -1;
    jclass binder = (*e)->FindClass(e, "android/os/Binder");
    if (!binder) return -1;
    jmethodID getter = (*e)->GetStaticMethodID(e, binder, name, "()I");
    return getter ? (*e)->CallStaticIntMethod(e, binder, getter) : -1;
}

static jobject new_bundle(JNIEnv *e) { return construct(e, "android/os/Bundle"); }

static int bundle_int(JNIEnv *e, jobject bundle, const char *key, jint value) {
    jmethodID put = method(e, "android/os/Bundle", "putInt", "(Ljava/lang/String;I)V");
    if (!put) return 0;
    jstring name = (*e)->NewStringUTF(e, key);
    if (!name) return 0;
    (*e)->CallVoidMethod(e, bundle, put, name, value);
    return !(*e)->ExceptionCheck(e);
}

static jint get_bundle_int(JNIEnv *e, jobject bundle, const char *key, jint fallback) {
    jmethodID get = method(e, "android/os/Bundle", "getInt", "(Ljava/lang/String;I)I");
    if (!get) return fallback;
    jstring name = (*e)->NewStringUTF(e, key);
    return name ? (*e)->CallIntMethod(e, bundle, get, name, fallback) : fallback;
}

static int bundle_text(JNIEnv *e, jobject bundle, const char *key, const char *value) {
    jmethodID put = method(e, "android/os/Bundle", "putString", "(Ljava/lang/String;Ljava/lang/String;)V");
    if (!put) return 0;
    jstring name = (*e)->NewStringUTF(e, key);
    if (!name) return 0;
    jstring text = (*e)->NewStringUTF(e, value);
    if (!text) return 0;
    (*e)->CallVoidMethod(e, bundle, put, name, text);
    return !(*e)->ExceptionCheck(e);
}

static void notify_revocation(uint64_t revoked_through) {
    pthread_mutex_lock(&client.mutex);
    pb_shizuku_revocation_handler handler = client.handler;
    void *owner = client.handler_owner;
    if (handler) ++client.handlers_running;
    pthread_mutex_unlock(&client.mutex);
    if (handler) {
        handler(owner, revoked_through);
        pthread_mutex_lock(&client.mutex);
        --client.handlers_running;
        pthread_cond_broadcast(&client.handlers_finished);
        pthread_mutex_unlock(&client.mutex);
    }
}

void pb_shizuku_client_set_revocation_handler(pb_shizuku_revocation_handler handler, void *owner) {
    pthread_mutex_lock(&client.mutex);
    client.handler = NULL;
    client.handler_owner = NULL;
    while (client.handlers_running) pthread_cond_wait(&client.handlers_finished, &client.mutex);
    client.handler = handler;
    client.handler_owner = owner;
    pthread_mutex_unlock(&client.mutex);
}

static int invalidate_at(JNIEnv *e, pb_shizuku_capability_state state, int detach,
                          uint64_t expected_epoch) {
    jobject server = NULL, application = NULL, death = NULL;
    pthread_mutex_lock(&client.mutex);
    if (expected_epoch != UINT64_MAX && client.epoch != expected_epoch) {
        pthread_mutex_unlock(&client.mutex);
        return 0;
    }
    uint64_t revoked_through = client.epoch;
    client.state = state;
    if (client.epoch == UINT64_MAX) client.state = PB_SHIZUKU_PLATFORM_ERROR;
    else ++client.epoch;
    if (detach) {
        client.attached = 0;
        server = client.server;
        application = client.application;
        death = client.death;
        client.server = client.application = client.death = NULL;
    }
    pthread_mutex_unlock(&client.mutex);
    pthread_mutex_lock(&worker_connection.mutex);
    if (worker_connection.connection && worker_connection.epoch <= revoked_through) {
        worker_connection.dead = 1;
        pthread_cond_broadcast(&worker_connection.changed);
    }
    pthread_mutex_unlock(&worker_connection.mutex);
    unlink_death(e, server, death);
    if (server) (*e)->DeleteGlobalRef(e, server);
    if (application) (*e)->DeleteGlobalRef(e, application);
    if (death) (*e)->DeleteGlobalRef(e, death);
    notify_revocation(revoked_through);
    return 1;
}

static int consume_platform_failure(JNIEnv *e) {
    if (!(*e)->ExceptionCheck(e)) return 0;
    /* Expected capability/SDK failures become an explicit recoverable state,
     * never successful acquisition or silently cached authorization. */
    (*e)->ExceptionClear(e);
    return 1;
}

static jobject parcel(JNIEnv *e) {
    if ((*e)->ExceptionCheck(e)) return NULL;
    jclass klass = (*e)->FindClass(e, "android/os/Parcel");
    if (!klass) return NULL;
    jmethodID obtain = (*e)->GetStaticMethodID(e, klass, "obtain", "()Landroid/os/Parcel;");
    return obtain ? (*e)->CallStaticObjectMethod(e, klass, obtain) : NULL;
}

static void recycle(JNIEnv *e, jobject data, jobject reply) {
    /* Preserve failure status while permitting checked JNI cleanup calls. */
    jthrowable failure = (*e)->ExceptionOccurred(e);
    if (failure) (*e)->ExceptionClear(e);
    jmethodID release = method(e, "android/os/Parcel", "recycle", "()V");
    if (release && data) (*e)->CallVoidMethod(e, data, release);
    if ((*e)->ExceptionCheck(e)) {
        if (!failure) failure = (*e)->ExceptionOccurred(e);
        (*e)->ExceptionClear(e);
    }
    if (release && reply) (*e)->CallVoidMethod(e, reply, release);
    if (failure) {
        if ((*e)->ExceptionCheck(e)) (*e)->ExceptionClear(e);
        (*e)->Throw(e, failure);
    }
}

static int transact(JNIEnv *e, jobject server, jint code, jobject data, jobject reply) {
    jmethodID send = method(e, "android/os/IBinder", "transact", "(ILandroid/os/Parcel;Landroid/os/Parcel;I)Z");
    if (!send || !(*e)->CallBooleanMethod(e, server, send, code, data, reply, 0) ||
        (*e)->ExceptionCheck(e)) return 0;
    jmethodID exception = method(e, "android/os/Parcel", "readException", "()V");
    if (!exception) return 0;
    (*e)->CallVoidMethod(e, reply, exception);
    return !(*e)->ExceptionCheck(e);
}

static int interface_token(JNIEnv *e, jobject data, const char *descriptor) {
    jmethodID write = method(e, "android/os/Parcel", "writeInterfaceToken", "(Ljava/lang/String;)V");
    if (!write) return 0;
    jstring text = (*e)->NewStringUTF(e, descriptor);
    if (!text) return 0;
    (*e)->CallVoidMethod(e, data, write, text);
    return !(*e)->ExceptionCheck(e);
}

static int parcel_consumed(JNIEnv *e, jobject data) {
    jmethodID available = method(e, "android/os/Parcel", "dataAvail", "()I");
    return available && (*e)->CallIntMethod(e, data, available) == 0 &&
        !(*e)->ExceptionCheck(e);
}

static int bounded_bundle(JNIEnv *e, jobject bundle, jint maximum_keys) {
    jmethodID descriptors = method(e, "android/os/Bundle", "hasFileDescriptors", "()Z");
    jmethodID size = method(e, "android/os/Bundle", "size", "()I");
    if (!descriptors || !size || (*e)->CallBooleanMethod(e, bundle, descriptors) ||
        (*e)->ExceptionCheck(e)) return 0;
    jint keys = (*e)->CallIntMethod(e, bundle, size);
    return !(*e)->ExceptionCheck(e) && keys >= 0 && keys <= maximum_keys;
}

static int rpc_integer(JNIEnv *e, jobject server, jint code, jint *value) {
    jobject data = parcel(e);
    if (!data) return 0;
    jobject reply = parcel(e);
    if (!reply) { recycle(e, data, NULL); return 0; }
    int ok = interface_token(e, data, SHIZUKU_DESCRIPTOR) && transact(e, server, code, data, reply);
    if (ok) {
        jmethodID read = method(e, "android/os/Parcel", "readInt", "()I");
        jmethodID available = method(e, "android/os/Parcel", "dataAvail", "()I");
        ok = read && available;
        if (ok) {
            *value = (*e)->CallIntMethod(e, reply, read);
            ok = !(*e)->ExceptionCheck(e) && (*e)->CallIntMethod(e, reply, available) == 0;
        }
    }
    recycle(e, data, reply);
    return ok && !(*e)->ExceptionCheck(e);
}

static int has_descriptor(JNIEnv *e, jobject binder, const char *expected) {
    jmethodID get = method(e, "android/os/IBinder", "getInterfaceDescriptor", "()Ljava/lang/String;");
    if (!get) return 0;
    jstring descriptor = (*e)->CallObjectMethod(e, binder, get);
    return text_equals(e, descriptor, expected);
}

static int provider_identity(JNIEnv *e, jobject context) {
    jmethodID package = method(e, "android/content/Context", "getPackageName", "()Ljava/lang/String;");
    if (!package || !text_equals(e, (*e)->CallObjectMethod(e, context, package), PB_PACKAGE)) return 0;
    jmethodID manager = method(e, "android/content/Context", "getPackageManager", "()Landroid/content/pm/PackageManager;");
    if (!manager) return 0;
    jobject pm = (*e)->CallObjectMethod(e, context, manager);
    if (!pm || (*e)->ExceptionCheck(e)) return 0;
    jmethodID resolve = method(e, "android/content/pm/PackageManager", "resolveContentProvider", "(Ljava/lang/String;I)Landroid/content/pm/ProviderInfo;");
    if (!resolve) return 0;
    jstring authority = (*e)->NewStringUTF(e, PB_AUTHORITY);
    if (!authority) return 0;
    jobject info = (*e)->CallObjectMethod(e, pm, resolve, authority, 0);
    if (!info || (*e)->ExceptionCheck(e)) return 0;
    jclass type = (*e)->FindClass(e, "android/content/pm/ProviderInfo");
    if (!type) return 0;
    const char *names[] = {"name", "packageName", "authority", "readPermission", "writePermission"};
    const char *values[] = {PB_PROVIDER, PB_PACKAGE, PB_AUTHORITY,
        "android.permission.INTERACT_ACROSS_USERS_FULL", "android.permission.INTERACT_ACROSS_USERS_FULL"};
    for (unsigned i = 0; i < 5; ++i) {
        jfieldID field = (*e)->GetFieldID(e, type, names[i], "Ljava/lang/String;");
        if (!field || !text_equals(e, (*e)->GetObjectField(e, info, field), values[i])) return 0;
    }
    jfieldID exported = (*e)->GetFieldID(e, type, "exported", "Z");
    if (!exported || !(*e)->GetBooleanField(e, info, exported)) return 0;
    jfieldID enabled = (*e)->GetFieldID(e, type, "enabled", "Z");
    if (!enabled || !(*e)->GetBooleanField(e, info, enabled)) return 0;
    jfieldID multiprocess = (*e)->GetFieldID(e, type, "multiprocess", "Z");
    if (!multiprocess || (*e)->GetBooleanField(e, info, multiprocess)) return 0;
    jfieldID application = (*e)->GetFieldID(e, type, "applicationInfo", "Landroid/content/pm/ApplicationInfo;");
    if (!application) return 0;
    jobject app = (*e)->GetObjectField(e, info, application);
    if (!app || (*e)->ExceptionCheck(e)) return 0;
    jclass app_type = (*e)->FindClass(e, "android/content/pm/ApplicationInfo");
    if (!app_type) return 0;
    jfieldID uid = (*e)->GetFieldID(e, app_type, "uid", "I");
    return uid && (uid_t)(*e)->GetIntField(e, app, uid) == getuid() && !(*e)->ExceptionCheck(e);
}

static int trusted_manager(JNIEnv *e) {
    /* Official Rikka certificate, observed from the pinned upstream release.
     * hasSigningCertificate also accepts an Android-verified signing lineage.
     * A package name alone cannot identify the authorization UI owner. */
    static const jbyte certificate[32] = {
        0x26, (jbyte)0x8b, 0x55, (jbyte)0x90, (jbyte)0xe8, 0x68, (jbyte)0xfb, 0x08,
        (jbyte)0xba, (jbyte)0xe7, (jbyte)0xe0, (jbyte)0xac, 0x41, 0x35, 0x64, (jbyte)0xcd,
        0x1f, (jbyte)0xf8, (jbyte)0x8f, 0x5c, (jbyte)0xcf, (jbyte)0xf7, 0x4a, (jbyte)0xf9,
        (jbyte)0xdb, (jbyte)0xd0, (jbyte)0xdc, (jbyte)0x91, (jbyte)0x8e, 0x30, (jbyte)0xdb, 0x30
    };
    pthread_mutex_lock(&client.mutex);
    jobject context = client.context ? (*e)->NewLocalRef(e, client.context) : NULL;
    pthread_mutex_unlock(&client.mutex);
    if (!context || (*e)->ExceptionCheck(e)) return 0;
    jmethodID get = method(e, "android/content/Context", "getPackageManager", "()Landroid/content/pm/PackageManager;");
    if (!get) return 0;
    jobject pm = (*e)->CallObjectMethod(e, context, get);
    if (!pm || (*e)->ExceptionCheck(e)) return 0;
    jmethodID signing = method(e, "android/content/pm/PackageManager", "hasSigningCertificate", "(Ljava/lang/String;[BI)Z");
    if (!signing) return 0;
    jstring manager = (*e)->NewStringUTF(e, SHIZUKU_MANAGER);
    if (!manager) return 0;
    jbyteArray digest = (*e)->NewByteArray(e, 32);
    if (!digest) return 0;
    (*e)->SetByteArrayRegion(e, digest, 0, 32, certificate);
    if ((*e)->ExceptionCheck(e) || !(*e)->CallBooleanMethod(e, pm, signing, manager, digest, 1) ||
        (*e)->ExceptionCheck(e)) return 0;
    jmethodID permission = method(e, "android/content/pm/PackageManager", "getPermissionInfo",
        "(Ljava/lang/String;I)Landroid/content/pm/PermissionInfo;");
    if (!permission) return 0;
    jstring name = (*e)->NewStringUTF(e, "moe.shizuku.manager.permission.API_V23");
    if (!name) return 0;
    jobject info = (*e)->CallObjectMethod(e, pm, permission, name, 0);
    if (!info || (*e)->ExceptionCheck(e)) return 0;
    jclass type = (*e)->FindClass(e, "android/content/pm/PermissionInfo");
    if (!type) return 0;
    jfieldID owner = (*e)->GetFieldID(e, type, "packageName", "Ljava/lang/String;");
    return owner && text_equals(e, (*e)->GetObjectField(e, info, owner), SHIZUKU_MANAGER);
}

static int attach(JNIEnv *e, jobject server, jobject application) {
    jobject data = parcel(e);
    if (!data) return 0;
    jobject reply = parcel(e);
    if (!reply) { recycle(e, data, NULL); return 0; }
    jobject args = new_bundle(e);
    int ok = args && bundle_int(e, args, "shizuku:attach-api-version", 13) &&
        bundle_text(e, args, "shizuku:attach-package-name", PB_PACKAGE) &&
        interface_token(e, data, SHIZUKU_DESCRIPTOR);
    jmethodID binder = method(e, "android/os/Parcel", "writeStrongBinder", "(Landroid/os/IBinder;)V");
    jmethodID integer = method(e, "android/os/Parcel", "writeInt", "(I)V");
    jmethodID bundle = method(e, "android/os/Bundle", "writeToParcel", "(Landroid/os/Parcel;I)V");
    if (ok && binder && integer && bundle) {
        (*e)->CallVoidMethod(e, data, binder, application);
        if (!(*e)->ExceptionCheck(e)) (*e)->CallVoidMethod(e, data, integer, 1);
        if (!(*e)->ExceptionCheck(e)) (*e)->CallVoidMethod(e, args, bundle, data, 0);
        ok = !(*e)->ExceptionCheck(e) && transact(e, server, 18, data, reply) &&
            parcel_consumed(e, reply);
    } else ok = 0;
    recycle(e, data, reply);
    return ok && !(*e)->ExceptionCheck(e);
}

static int receive_server(JNIEnv *e, jobject extras, uid_t uid) {
    if (!extras || (uid != 0 && uid != 2000) || !trusted_manager(e)) return 0;
    jclass container_type = (*e)->FindClass(e, "moe/shizuku/api/BinderContainer");
    if (!container_type) return 0;
    jmethodID loader = method(e, "java/lang/Class", "getClassLoader", "()Ljava/lang/ClassLoader;");
    if (!loader) return 0;
    jobject class_loader = (*e)->CallObjectMethod(e, container_type, loader);
    if ((*e)->ExceptionCheck(e)) return 0;
    jmethodID set_loader = method(e, "android/os/Bundle", "setClassLoader", "(Ljava/lang/ClassLoader;)V");
    if (!set_loader) return 0;
    (*e)->CallVoidMethod(e, extras, set_loader, class_loader);
    if ((*e)->ExceptionCheck(e)) return 0;
    if (!bounded_bundle(e, extras, 1)) return 0;
    jmethodID size = method(e, "android/os/Bundle", "size", "()I");
    if (!size || (*e)->CallIntMethod(e, extras, size) != 1 || (*e)->ExceptionCheck(e)) return 0;
    jmethodID get = method(e, "android/os/Bundle", "getParcelable", "(Ljava/lang/String;)Landroid/os/Parcelable;");
    if (!get) return 0;
    jstring key = (*e)->NewStringUTF(e, BINDER_EXTRA);
    if (!key) return 0;
    jobject container = (*e)->CallObjectMethod(e, extras, get, key);
    if (!container || (*e)->ExceptionCheck(e) || !(*e)->IsInstanceOf(e, container, container_type)) return 0;
    jfieldID field = (*e)->GetFieldID(e, container_type, "binder", "Landroid/os/IBinder;");
    if (!field) return 0;
    jobject server = (*e)->GetObjectField(e, container, field);
    if (!server || !has_descriptor(e, server, SHIZUKU_DESCRIPTOR)) return 0;
    pthread_mutex_lock(&client.mutex);
    jobject previous = client.server ? (*e)->NewLocalRef(e, client.server) : NULL;
    int same = previous && uid == client.sender_uid && (*e)->IsSameObject(e, previous, server);
    int exhausted = client.epoch == UINT64_MAX;
    pthread_mutex_unlock(&client.mutex);
    if (exhausted || (*e)->ExceptionCheck(e)) return 0;
    if (previous) {
        jmethodID ping = method(e, "android/os/IBinder", "pingBinder", "()Z");
        if (!ping) return 0;
        jboolean alive = (*e)->CallBooleanMethod(e, previous, ping);
        if ((*e)->ExceptionCheck(e)) return 0;
        if (alive) return same;
    }
    /* API 13 protects getVersion/getUid behind application attachment and
     * permission. The authenticated bindApplication reply supplies these
     * observations until authorization permits those independent queries. */
    jobject application = construct(e, "com/isomorphisms/pasteboard/ShizukuApplicationCallback");
    if (!application) return 0;
    jobject death = construct(e, "com/isomorphisms/pasteboard/ShizukuDeathRecipient");
    if (!death) return 0;
    jmethodID attach_interface = method(e, "android/os/Binder", "attachInterface", "(Landroid/os/IInterface;Ljava/lang/String;)V");
    if (!attach_interface) return 0;
    jstring descriptor = (*e)->NewStringUTF(e, APPLICATION_DESCRIPTOR);
    if (!descriptor) return 0;
    (*e)->CallVoidMethod(e, application, attach_interface, NULL, descriptor);
    if ((*e)->ExceptionCheck(e)) return 0;
    jobject server_ref = (*e)->NewGlobalRef(e, server);
    if (!server_ref) return 0;
    jobject application_ref = (*e)->NewGlobalRef(e, application);
    if (!application_ref) { (*e)->DeleteGlobalRef(e, server_ref); return 0; }
    jobject death_ref = (*e)->NewGlobalRef(e, death);
    if (!death_ref) { (*e)->DeleteGlobalRef(e, server_ref); (*e)->DeleteGlobalRef(e, application_ref); return 0; }
    (void)invalidate_at(e, PB_SHIZUKU_ATTACHING, 1, UINT64_MAX);
    pthread_mutex_lock(&client.mutex);
    client.server = server_ref;
    client.application = application_ref;
    client.death = death_ref;
    client.sender_uid = uid;
    uint64_t adopted_epoch = client.epoch;
    pthread_mutex_unlock(&client.mutex);
    jmethodID link = method(e, "android/os/IBinder", "linkToDeath", "(Landroid/os/IBinder$DeathRecipient;I)V");
    int accepted = link != NULL;
    if (accepted) (*e)->CallVoidMethod(e, server, link, death, 0);
    accepted = accepted && !(*e)->ExceptionCheck(e) && attach(e, server, application);
    if (!accepted) {
        (void)consume_platform_failure(e);
        (void)invalidate_at(e, PB_SHIZUKU_PLATFORM_ERROR, 1, adopted_epoch);
    }
    return accepted;
}

pb_shizuku_capability_state pb_shizuku_client_authorization(JNIEnv *e, uid_t *uid, uint64_t *epoch) {
    if (uid) *uid = (uid_t)-1;
    if (epoch) *epoch = 0;
    pthread_mutex_lock(&client.mutex);
    jobject server = client.server && client.attached ? (*e)->NewLocalRef(e, client.server) : NULL;
    uint64_t before = client.epoch;
    pb_shizuku_capability_state state = client.state;
    pthread_mutex_unlock(&client.mutex);
    if (!server) {
        if (consume_platform_failure(e)) {
            (void)invalidate_at(e, PB_SHIZUKU_PLATFORM_ERROR, 1, before);
            return PB_SHIZUKU_PLATFORM_ERROR;
        }
        return state;
    }
    jint allowed = 0;
    if (!rpc_integer(e, server, 16, &allowed) || (allowed != 0 && allowed != 1)) {
        (void)consume_platform_failure(e);
        (void)invalidate_at(e, PB_SHIZUKU_PLATFORM_ERROR, 1, before);
        return PB_SHIZUKU_PLATFORM_ERROR;
    }
    if (!allowed) {
        if (state != PB_SHIZUKU_PERMISSION_REQUIRED)
            (void)invalidate_at(e, PB_SHIZUKU_PERMISSION_REQUIRED, 0, before);
        return PB_SHIZUKU_PERMISSION_REQUIRED;
    }
    jint version = -1, observed_uid = -1;
    if (!rpc_integer(e, server, 3, &version) || version != 13 ||
        !rpc_integer(e, server, 4, &observed_uid)) {
        (void)consume_platform_failure(e);
        (void)invalidate_at(e, PB_SHIZUKU_PLATFORM_ERROR, 1, before);
        return PB_SHIZUKU_PLATFORM_ERROR;
    }
    pthread_mutex_lock(&client.mutex);
    int wrong_identity = before == client.epoch && observed_uid != (jint)client.sender_uid;
    int current = before == client.epoch && client.attached && client.server &&
        (*e)->IsSameObject(e, server, client.server) && observed_uid == (jint)client.sender_uid;
    if (current) {
        client.state = PB_SHIZUKU_AUTHORIZED;
        if (uid) *uid = client.sender_uid;
        if (epoch) *epoch = client.epoch;
    }
    pthread_mutex_unlock(&client.mutex);
    if (wrong_identity) {
        (void)invalidate_at(e, PB_SHIZUKU_PLATFORM_ERROR, 1, before);
        return PB_SHIZUKU_PLATFORM_ERROR;
    }
    return current ? PB_SHIZUKU_AUTHORIZED : PB_SHIZUKU_UNAVAILABLE;
}

jobject pb_shizuku_client_authorized_server(JNIEnv *e, uid_t *uid, uint64_t *epoch) {
    uint64_t observed = 0;
    if (pb_shizuku_client_authorization(e, uid, &observed) != PB_SHIZUKU_AUTHORIZED) return NULL;
    pthread_mutex_lock(&client.mutex);
    jobject server = client.epoch == observed && client.server ? (*e)->NewLocalRef(e, client.server) : NULL;
    if (epoch) *epoch = server ? observed : 0;
    pthread_mutex_unlock(&client.mutex);
    if (consume_platform_failure(e)) {
        (void)invalidate_at(e, PB_SHIZUKU_PLATFORM_ERROR, 1, observed);
        if (uid) *uid = (uid_t)-1;
        if (epoch) *epoch = 0;
        return NULL;
    }
    return server;
}

int pb_shizuku_client_request_permission(JNIEnv *e) {
    pthread_mutex_lock(&client.mutex);
    jobject server = client.server && client.attached ? (*e)->NewLocalRef(e, client.server) : NULL;
    uint64_t before = client.epoch;
    pthread_mutex_unlock(&client.mutex);
    if (!server) {
        if (consume_platform_failure(e))
            (void)invalidate_at(e, PB_SHIZUKU_PLATFORM_ERROR, 1, before);
        return -1;
    }
    jobject data = parcel(e);
    if (!data) {
        (void)consume_platform_failure(e);
        (void)invalidate_at(e, PB_SHIZUKU_PLATFORM_ERROR, 1, before);
        return -1;
    }
    jobject reply = parcel(e);
    if (!reply) {
        recycle(e, data, NULL);
        (void)consume_platform_failure(e);
        (void)invalidate_at(e, PB_SHIZUKU_PLATFORM_ERROR, 1, before);
        return -1;
    }
    jmethodID write = method(e, "android/os/Parcel", "writeInt", "(I)V");
    int ok = write && interface_token(e, data, SHIZUKU_DESCRIPTOR);
    if (ok) {
        (*e)->CallVoidMethod(e, data, write, REQUEST_PERMISSION_CODE);
        ok = !(*e)->ExceptionCheck(e) && transact(e, server, 15, data, reply) &&
            parcel_consumed(e, reply);
    }
    recycle(e, data, reply);
    if (consume_platform_failure(e) || !ok) {
        (void)invalidate_at(e, PB_SHIZUKU_PLATFORM_ERROR, 1, before);
        return -1;
    }
    return ok ? 0 : -1;
}

JNIEXPORT jboolean JNICALL
Java_com_isomorphisms_pasteboard_ShizukuProvider_onCreate(JNIEnv *e, jobject provider) {
    if (getuid() < 10000 || (*e)->PushLocalFrame(e, 128) != 0) return JNI_FALSE;
    jmethodID get = method(e, "android/content/ContentProvider", "getContext", "()Landroid/content/Context;");
    jobject context = get ? (*e)->CallObjectMethod(e, provider, get) : NULL;
    int valid = context && provider_identity(e, context);
    if (valid) {
        jobject saved = (*e)->NewGlobalRef(e, context);
        if (!saved) valid = 0;
        else {
            pthread_mutex_lock(&client.mutex);
            if (client.context) (*e)->DeleteGlobalRef(e, client.context);
            client.context = saved;
            pthread_mutex_unlock(&client.mutex);
        }
    }
    if (consume_platform_failure(e)) valid = 0;
    if (valid && pb_history_open(e, context)) {
        PBHistoryView view = {0};
        if (pb_history_apply(e, "validate", NULL, 0u, &view)) pb_observer_app_ready(e, context);
        pb_history_view_free(&view);
    }
    if (!valid) (void)invalidate_at(e, PB_SHIZUKU_PLATFORM_ERROR, 1, UINT64_MAX);
    (*e)->PopLocalFrame(e, NULL);
    return valid ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jobject JNICALL
Java_com_isomorphisms_pasteboard_ShizukuProvider_call(JNIEnv *e, jobject provider,
                                              jstring name, jstring argument, jobject extras) {
    (void)provider;
    (void)argument;
    if ((*e)->PushLocalFrame(e, 128) != 0) return NULL;
    jint uid = calling_identity(e, "getCallingUid");
    pthread_mutex_lock(&client.mutex);
    int initialized = client.context != NULL;
    pthread_mutex_unlock(&client.mutex);
    int accepted = 0;
    if (initialized && text_equals(e, name, "sendBinder")) {
        pthread_mutex_lock(&delivery_mutex);
        accepted = receive_server(e, extras, (uid_t)uid);
        pthread_mutex_unlock(&delivery_mutex);
    } else if (initialized) {
        int handled = 0;
        jobject result = pb_observer_provider_request(e, name, argument, extras, (uid_t)uid, &handled);
        if (handled) return (*e)->PopLocalFrame(e, result);
    }
    if (consume_platform_failure(e) || !accepted) {
        (*e)->PopLocalFrame(e, NULL);
        return NULL;
    }
    jobject reply = new_bundle(e);
    if (consume_platform_failure(e)) reply = NULL;
    return (*e)->PopLocalFrame(e, reply);
}

static jobject read_bundle(JNIEnv *e, jobject data) {
    jmethodID read = method(e, "android/os/Parcel", "readInt", "()I");
    if (!read || (*e)->CallIntMethod(e, data, read) != 1 || (*e)->ExceptionCheck(e)) return NULL;
    jclass type = (*e)->FindClass(e, "android/os/Bundle");
    if (!type) return NULL;
    jfieldID field = (*e)->GetStaticFieldID(e, type, "CREATOR", "Landroid/os/Parcelable$Creator;");
    if (!field) return NULL;
    jobject creator = (*e)->GetStaticObjectField(e, type, field);
    if (!creator || (*e)->ExceptionCheck(e)) return NULL;
    jmethodID create = method(e, "android/os/Parcelable$Creator", "createFromParcel", "(Landroid/os/Parcel;)Ljava/lang/Object;");
    if (!create) return NULL;
    jobject bundle = (*e)->CallObjectMethod(e, creator, create, data);
    if (!bundle || (*e)->ExceptionCheck(e) || !(*e)->IsInstanceOf(e, bundle, type)) return NULL;
    return bundle;
}

JNIEXPORT jboolean JNICALL
Java_com_isomorphisms_pasteboard_ShizukuApplicationCallback_onTransact(JNIEnv *e, jobject application,
                                                               jint code, jobject data,
                                                               jobject reply, jint flags) {
    if ((*e)->PushLocalFrame(e, 128) != 0) return JNI_FALSE;
    if (code == INTERFACE_TRANSACTION && reply) {
        jmethodID write = method(e, "android/os/Parcel", "writeString", "(Ljava/lang/String;)V");
        jstring descriptor = write ? (*e)->NewStringUTF(e, APPLICATION_DESCRIPTOR) : NULL;
        if (descriptor) (*e)->CallVoidMethod(e, reply, write, descriptor);
        int ok = !consume_platform_failure(e) && descriptor;
        (*e)->PopLocalFrame(e, NULL);
        return ok ? JNI_TRUE : JNI_FALSE;
    }
    /* One-way SDK callbacks have no reliable PID. Authenticate the kernel
     * UID, this private callback object and its current connection epoch. */
    jint uid = calling_identity(e, "getCallingUid");
    pthread_mutex_lock(&client.mutex);
    int current = client.application && (*e)->IsSameObject(e, application, client.application) &&
        uid == (jint)client.sender_uid;
    uint64_t before = client.epoch;
    pthread_mutex_unlock(&client.mutex);
    int known_callback = code == 2 || code == 3;
    int ok = current && data && known_callback && pb_binder_oneway(flags);
    jmethodID size = method(e, "android/os/Parcel", "dataSize", "()I");
    jmethodID available = method(e, "android/os/Parcel", "dataAvail", "()I");
    jmethodID enforce = method(e, "android/os/Parcel", "enforceInterface", "(Ljava/lang/String;)V");
    if (ok && size && available && enforce) {
        jint bytes = (*e)->CallIntMethod(e, data, size);
        ok = !(*e)->ExceptionCheck(e) && bytes > 0 && bytes <= 16384;
        jstring descriptor = ok ? (*e)->NewStringUTF(e, APPLICATION_DESCRIPTOR) : NULL;
        if (descriptor) (*e)->CallVoidMethod(e, data, enforce, descriptor);
        ok = ok && descriptor && !(*e)->ExceptionCheck(e);
    } else ok = 0;
    if (ok && code == 3) {
        jmethodID read = method(e, "android/os/Parcel", "readInt", "()I");
        ok = read && (*e)->CallIntMethod(e, data, read) == REQUEST_PERMISSION_CODE && !(*e)->ExceptionCheck(e);
    }
    jobject bundle = ok ? read_bundle(e, data) : NULL;
    ok = ok && bundle && !(*e)->ExceptionCheck(e) &&
        (*e)->CallIntMethod(e, data, available) == 0 && bounded_bundle(e, bundle, 8);
    if (ok && code == 2) {
        jint server_uid = get_bundle_int(e, bundle, "shizuku:attach-reply-uid", -1);
        jint version = get_bundle_int(e, bundle, "shizuku:attach-reply-version", -1);
        ok = !(*e)->ExceptionCheck(e) && server_uid == uid && version == 13;
        if (ok) {
            pthread_mutex_lock(&client.mutex);
            ok = before == client.epoch && client.application &&
                (*e)->IsSameObject(e, application, client.application);
            if (ok) { client.attached = 1; client.state = PB_SHIZUKU_PERMISSION_REQUIRED; }
            pthread_mutex_unlock(&client.mutex);
        }
    }
    if (ok && code == 3) {
        /* The reply wakes the app's state check; only checkSelfPermission
         * grants current authority, never the claimed Boolean in this Bundle. */
        uid_t observed_uid;
        uint64_t observed_epoch;
        (void)pb_shizuku_client_authorization(e, &observed_uid, &observed_epoch);
    }
    if (consume_platform_failure(e)) ok = 0;
    if (current && known_callback && !ok)
        (void)invalidate_at(e, PB_SHIZUKU_PLATFORM_ERROR, 1, before);
    if (ok) pb_observer_try_start(e);
    (*e)->PopLocalFrame(e, NULL);
    return ok ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT void JNICALL
Java_com_isomorphisms_pasteboard_ShizukuDeathRecipient_binderDied(JNIEnv *e, jobject recipient) {
    if ((*e)->PushLocalFrame(e, 16) != 0) return;
    pthread_mutex_lock(&client.mutex);
    int current = client.death && (*e)->IsSameObject(e, recipient, client.death);
    uint64_t before = client.epoch;
    pthread_mutex_unlock(&client.mutex);
    if (current) (void)invalidate_at(e, PB_SHIZUKU_UNAVAILABLE, 1, before);
    (*e)->PopLocalFrame(e, NULL);
}

JNIEXPORT jint JNICALL
Java_com_isomorphisms_pasteboard_ShizukuClient_state(JNIEnv *e, jclass type) {
    (void)type;
    if ((*e)->PushLocalFrame(e, 128) != 0) return PB_SHIZUKU_PLATFORM_ERROR;
    pb_shizuku_capability_state state = pb_shizuku_client_authorization(e, NULL, NULL);
    (*e)->PopLocalFrame(e, NULL);
    return state;
}

JNIEXPORT jint JNICALL
Java_com_isomorphisms_pasteboard_ShizukuClient_requestPermission(JNIEnv *e, jclass type) {
    (void)type;
    if ((*e)->PushLocalFrame(e, 128) != 0) return -1;
    int result = pb_shizuku_client_request_permission(e);
    (*e)->PopLocalFrame(e, NULL);
    return result;
}

/* This provider is capability delivery only. Other content operations have
 * no data surface and cannot read or mutate source/history storage. */
JNIEXPORT jobject JNICALL
Java_com_isomorphisms_pasteboard_ShizukuProvider_query(JNIEnv *e, jobject self, jobject uri,
    jobjectArray projection, jstring selection, jobjectArray arguments, jstring order) {
    (void)e; (void)self; (void)uri; (void)projection; (void)selection; (void)arguments; (void)order;
    return NULL;
}
JNIEXPORT jstring JNICALL
Java_com_isomorphisms_pasteboard_ShizukuProvider_getType(JNIEnv *e, jobject self, jobject uri) {
    (void)e; (void)self; (void)uri; return NULL;
}
JNIEXPORT jobject JNICALL
Java_com_isomorphisms_pasteboard_ShizukuProvider_insert(JNIEnv *e, jobject self, jobject uri, jobject values) {
    (void)e; (void)self; (void)uri; (void)values; return NULL;
}
JNIEXPORT jint JNICALL
Java_com_isomorphisms_pasteboard_ShizukuProvider_delete(JNIEnv *e, jobject self, jobject uri,
    jstring selection, jobjectArray arguments) {
    (void)e; (void)self; (void)uri; (void)selection; (void)arguments; return 0;
}
JNIEXPORT jint JNICALL
Java_com_isomorphisms_pasteboard_ShizukuProvider_update(JNIEnv *e, jobject self, jobject uri,
    jobject values, jstring selection, jobjectArray arguments) {
    (void)e; (void)self; (void)uri; (void)values; (void)selection; (void)arguments; return 0;
}

static int bundle_boolean(JNIEnv *e, jobject bundle, const char *key, jboolean value) {
    jmethodID put = method(e, "android/os/Bundle", "putBoolean", "(Ljava/lang/String;Z)V");
    jstring name = put ? (*e)->NewStringUTF(e, key) : NULL;
    if (name) (*e)->CallVoidMethod(e, bundle, put, name, value);
    return name && !(*e)->ExceptionCheck(e);
}

static jobject worker_options(JNIEnv *e, jobject context) {
    jmethodID get_name = method(e, "android/content/Context", "getPackageName", "()Ljava/lang/String;");
    jstring name = get_name ? (*e)->CallObjectMethod(e, context, get_name) : NULL;
    if (!text_equals(e, name, PB_PACKAGE)) return NULL;
    jmethodID get_pm = method(e, "android/content/Context", "getPackageManager", "()Landroid/content/pm/PackageManager;");
    jobject pm = get_pm ? (*e)->CallObjectMethod(e, context, get_pm) : NULL;
    jmethodID get_info = method(e, "android/content/pm/PackageManager", "getPackageInfo", "(Ljava/lang/String;I)Landroid/content/pm/PackageInfo;");
    jobject info = pm && get_info ? (*e)->CallObjectMethod(e, pm, get_info, name, 0) : NULL;
    jclass info_type = (*e)->FindClass(e, "android/content/pm/PackageInfo");
    jfieldID version_field = info_type ? (*e)->GetFieldID(e, info_type, "versionCode", "I") : NULL;
    jint version = info && version_field ? (*e)->GetIntField(e, info, version_field) : 0;
    if ((*e)->ExceptionCheck(e) || version < 1) return NULL;
    jclass component_type = (*e)->FindClass(e, "android/content/ComponentName");
    jmethodID constructor = component_type ? (*e)->GetMethodID(e, component_type, "<init>", "(Ljava/lang/String;Ljava/lang/String;)V") : NULL;
    jstring worker_name = constructor ? (*e)->NewStringUTF(e, "com.isomorphisms.pasteboard.ClipboardObserver") : NULL;
    jobject component = worker_name ? (*e)->NewObject(e, component_type, constructor, name, worker_name) : NULL;
    jobject options = component ? new_bundle(e) : NULL;
    jmethodID put = method(e, "android/os/Bundle", "putParcelable", "(Ljava/lang/String;Landroid/os/Parcelable;)V");
    jstring key = options && put ? (*e)->NewStringUTF(e, "shizuku:user-service-arg-component") : NULL;
    if (key) (*e)->CallVoidMethod(e, options, put, key, component);
    int ok = key && !(*e)->ExceptionCheck(e) &&
        bundle_int(e, options, "shizuku:user-service-arg-version-code", version) &&
        bundle_text(e, options, "shizuku:user-service-arg-tag", "pasteboard-observer-v1") &&
        bundle_text(e, options, "shizuku:user-service-arg-process-name", "pasteboard_observer") &&
        bundle_boolean(e, options, "shizuku:user-service-arg-debuggable", JNI_FALSE) &&
        bundle_boolean(e, options, "shizuku:user-service-arg-daemon", JNI_TRUE) &&
        bundle_boolean(e, options, "shizuku:user-service-arg-use-32-bit-app-process", sizeof(void *) == 4u ? JNI_TRUE : JNI_FALSE) &&
        bundle_boolean(e, options, "shizuku:user-service-remove", JNI_FALSE);
    return ok ? options : NULL;
}

static int user_service_rpc(JNIEnv *e, jobject server, jint code, jobject connection, jobject options) {
    jobject data = parcel(e), reply = parcel(e);
    jmethodID binder = method(e, "android/os/Parcel", "writeStrongBinder", "(Landroid/os/IBinder;)V");
    jmethodID integer = method(e, "android/os/Parcel", "writeInt", "(I)V");
    jmethodID bundle = method(e, "android/os/Bundle", "writeToParcel", "(Landroid/os/Parcel;I)V");
    int ok = data && reply && binder && integer && bundle && interface_token(e, data, SHIZUKU_DESCRIPTOR);
    if (ok) (*e)->CallVoidMethod(e, data, binder, connection);
    if (ok && !(*e)->ExceptionCheck(e)) (*e)->CallVoidMethod(e, data, integer, 1);
    if (ok && !(*e)->ExceptionCheck(e)) (*e)->CallVoidMethod(e, options, bundle, data, 0);
    ok = ok && !(*e)->ExceptionCheck(e) && transact(e, server, code, data, reply);
    jmethodID read = method(e, "android/os/Parcel", "readInt", "()I");
    jint result = ok && read ? (*e)->CallIntMethod(e, reply, read) : -1;
    ok = ok && read && result == 0 && !(*e)->ExceptionCheck(e) && parcel_consumed(e, reply);
    __android_log_print(ANDROID_LOG_INFO, "PasteboardObserver", "user-service RPC code=%d result=%d accepted=%d", code, result, ok);
    recycle(e, data, reply);
    return ok;
}

JNIEXPORT jboolean JNICALL
Java_com_isomorphisms_pasteboard_ShizukuWorkerConnection_onTransact(JNIEnv *e, jobject self,
    jint code, jobject data, jobject reply, jint flags) {
    if ((*e)->PushLocalFrame(e, 64) != 0) return JNI_FALSE;
    if (code == INTERFACE_TRANSACTION && pb_binder_synchronous(flags) && reply) {
        jmethodID write = method(e, "android/os/Parcel", "writeString", "(Ljava/lang/String;)V");
        jstring descriptor = write ? (*e)->NewStringUTF(e, WORKER_CONNECTION_DESCRIPTOR) : NULL;
        if (descriptor) (*e)->CallVoidMethod(e, reply, write, descriptor);
        int described = descriptor && !(*e)->ExceptionCheck(e);
        (*e)->PopLocalFrame(e, NULL);
        return described ? JNI_TRUE : JNI_FALSE;
    }
    int uid = calling_identity(e, "getCallingUid");
    pthread_mutex_lock(&worker_connection.mutex);
    uint64_t epoch = worker_connection.epoch;
    int current = worker_connection.connection && !worker_connection.dead &&
        (*e)->IsSameObject(e, self, worker_connection.connection) && uid == (jint)worker_connection.uid;
    pthread_mutex_unlock(&worker_connection.mutex);
    pthread_mutex_lock(&client.mutex);
    current = current && epoch == client.epoch && client.attached && client.state == PB_SHIZUKU_AUTHORIZED;
    pthread_mutex_unlock(&client.mutex);
    jmethodID size = method(e, "android/os/Parcel", "dataSize", "()I");
    jmethodID fds = method(e, "android/os/Parcel", "hasFileDescriptors", "()Z");
    jint bytes = current && data && size ? (*e)->CallIntMethod(e, data, size) : -1;
    int ok = current && pb_binder_oneway(flags) && (code == 1 || code == 2) && fds && bytes > 0 && bytes <= 512 &&
        !(*e)->CallBooleanMethod(e, data, fds) && !(*e)->ExceptionCheck(e);
    jmethodID enforce = method(e, "android/os/Parcel", "enforceInterface", "(Ljava/lang/String;)V");
    jstring descriptor = ok && enforce ? (*e)->NewStringUTF(e, WORKER_CONNECTION_DESCRIPTOR) : NULL;
    if (descriptor) (*e)->CallVoidMethod(e, data, enforce, descriptor);
    ok = ok && descriptor && !(*e)->ExceptionCheck(e);
    jobject service = NULL;
    if (ok && code == 1) {
        jmethodID read = method(e, "android/os/Parcel", "readStrongBinder", "()Landroid/os/IBinder;");
        if (read) service = (*e)->CallObjectMethod(e, data, read);
        ok = service && !(*e)->ExceptionCheck(e);
    }
    ok = ok && parcel_consumed(e, data);
    jobject global = ok && service ? (*e)->NewGlobalRef(e, service) : NULL;
    ok = ok && (code == 2 || global) && !(*e)->ExceptionCheck(e);
    if (ok) {
        pthread_mutex_lock(&worker_connection.mutex);
        if (worker_connection.connection && worker_connection.epoch == epoch && !worker_connection.dead &&
            (*e)->IsSameObject(e, self, worker_connection.connection)) {
            if (worker_connection.service) (*e)->DeleteGlobalRef(e, worker_connection.service);
            worker_connection.service = global;
            global = NULL;
            worker_connection.dead = code == 2;
            pthread_cond_broadcast(&worker_connection.changed);
        } else ok = 0;
        pthread_mutex_unlock(&worker_connection.mutex);
    }
    if (global) (*e)->DeleteGlobalRef(e, global);
    __android_log_print(ANDROID_LOG_INFO, "PasteboardObserver", "worker connection callback code=%d flags=%d uid=%d current=%d bytes=%d accepted=%d", code, flags, uid, current, bytes, ok);
    if ((*e)->ExceptionCheck(e)) (*e)->ExceptionClear(e);
    (*e)->PopLocalFrame(e, NULL);
    return ok ? JNI_TRUE : JNI_FALSE;
}

int pb_shizuku_observer_connect(JNIEnv *e, const char *token, int64_t capture_since) {
    if (e == NULL || token == NULL || strlen(token) != 64u || capture_since <= 0 ||
        pthread_mutex_trylock(&worker_request_mutex) != 0) return -1;
    if ((*e)->PushLocalFrame(e, 128) != 0) {
        pthread_mutex_unlock(&worker_request_mutex);
        return -1;
    }
    uid_t uid = (uid_t)-1;
    uint64_t epoch = 0u;
    jobject server = pb_shizuku_client_authorized_server(e, &uid, &epoch);
    pthread_mutex_lock(&client.mutex);
    jobject context = client.context ? (*e)->NewLocalRef(e, client.context) : NULL;
    pthread_mutex_unlock(&client.mutex);
    int ok = server && context && uid == 2000u;
    jobject options = ok ? worker_options(e, context) : NULL;
    jobject connection = options ? construct(e, "com/isomorphisms/pasteboard/ShizukuWorkerConnection") : NULL;
    jmethodID attach_interface = method(e, "android/os/Binder", "attachInterface", "(Landroid/os/IInterface;Ljava/lang/String;)V");
    jstring descriptor = connection && attach_interface ? (*e)->NewStringUTF(e, WORKER_CONNECTION_DESCRIPTOR) : NULL;
    if (descriptor) (*e)->CallVoidMethod(e, connection, attach_interface, NULL, descriptor);
    jobject global = connection ? (*e)->NewGlobalRef(e, connection) : NULL;
    ok = ok && global && descriptor && !(*e)->ExceptionCheck(e);
    pthread_mutex_lock(&worker_connection.mutex);
    jobject old_connection = worker_connection.connection;
    jobject old_service = worker_connection.service;
    worker_connection.connection = global;
    worker_connection.service = NULL;
    worker_connection.epoch = epoch;
    worker_connection.uid = uid;
    worker_connection.dead = 0;
    pthread_mutex_unlock(&worker_connection.mutex);
    if (old_connection) (*e)->DeleteGlobalRef(e, old_connection);
    if (old_service) (*e)->DeleteGlobalRef(e, old_service);
    int registered = ok && user_service_rpc(e, server, 12, connection, options);
    ok = registered;
    jobject service = NULL;
    if (ok) {
        struct timespec deadline = {0};
        ok = clock_gettime(CLOCK_REALTIME, &deadline) == 0;
        deadline.tv_sec += 32;
        pthread_mutex_lock(&worker_connection.mutex);
        while (ok && !worker_connection.dead && !worker_connection.service) {
            const int waited = pthread_cond_timedwait(&worker_connection.changed, &worker_connection.mutex, &deadline);
            if (waited != 0 && waited != EINTR) ok = 0;
        }
        if (ok && !worker_connection.dead && worker_connection.service)
            service = (*e)->NewLocalRef(e, worker_connection.service);
        pthread_mutex_unlock(&worker_connection.mutex);
        ok = ok && service && !(*e)->ExceptionCheck(e);
    }
    uid_t now_uid;
    uint64_t now_epoch;
    ok = ok && pb_shizuku_client_authorization(e, &now_uid, &now_epoch) == PB_SHIZUKU_AUTHORIZED &&
        now_uid == uid && now_epoch == epoch && has_descriptor(e, service, WORKER_DESCRIPTOR);
    jobject data = ok ? parcel(e) : NULL, reply = ok ? parcel(e) : NULL;
    jmethodID write = method(e, "android/os/Parcel", "writeString", "(Ljava/lang/String;)V");
    jmethodID write_time = method(e, "android/os/Parcel", "writeLong", "(J)V");
    jstring capability = ok && write ? (*e)->NewStringUTF(e, token) : NULL;
    ok = ok && data && reply && capability && write_time && interface_token(e, data, WORKER_DESCRIPTOR);
    if (ok) (*e)->CallVoidMethod(e, data, write, capability);
    if (ok && !(*e)->ExceptionCheck(e)) (*e)->CallVoidMethod(e, data, write_time, (jlong)capture_since);
    ok = ok && !(*e)->ExceptionCheck(e) && transact(e, service, 2, data, reply);
    jmethodID read = method(e, "android/os/Parcel", "readInt", "()I");
    const jint result = ok && read ? (*e)->CallIntMethod(e, reply, read) : -1;
    ok = ok && read && result == 0 && !(*e)->ExceptionCheck(e) && parcel_consumed(e, reply);
    recycle(e, data, reply);
    if ((*e)->ExceptionCheck(e)) { (*e)->ExceptionClear(e); ok = 0; }
    if (!ok) {
        pthread_mutex_lock(&worker_connection.mutex);
        if (worker_connection.connection == global) {
            if (worker_connection.service) (*e)->DeleteGlobalRef(e, worker_connection.service);
            worker_connection.connection = NULL;
            worker_connection.service = NULL;
            worker_connection.dead = 1;
        }
        pthread_mutex_unlock(&worker_connection.mutex);
        if (global) (*e)->DeleteGlobalRef(e, global);
    }
    pb_observer_connected(ok);
    (*e)->PopLocalFrame(e, NULL);
    pthread_mutex_unlock(&worker_request_mutex);
    return ok ? 0 : -1;
}

