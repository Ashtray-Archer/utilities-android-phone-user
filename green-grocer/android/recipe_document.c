#include "recipe_document.h"

#include <android/log.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

enum { RECIPE_DOCUMENT_REQUEST = 412, DOCUMENT_BYTE_BUDGET = 1048576 };
static pthread_mutex_t document_mutex = PTHREAD_MUTEX_INITIALIZER;
static struct {
    ALooper *looper;
    jobject launching_activity;
    bool waiting, working;
    gg_document_result result;
    char *path;
} document;

typedef struct { JavaVM *vm; jobject activity, uri; } copy_job;

static bool exception(JNIEnv *env) {
    if ((*env)->ExceptionCheck(env) == JNI_FALSE) return false;
    __android_log_print(ANDROID_LOG_ERROR, "GGRecipeDocument", "Document SDK operation failed");
    (*env)->ExceptionDescribe(env);
    (*env)->ExceptionClear(env);
    return true;
}

static void finish(gg_document_result result, char *path) {
    (void)pthread_mutex_lock(&document_mutex);
    document.working = false;
    document.waiting = false;
    document.result = result;
    document.path = path;
    if (document.looper != NULL) ALooper_wake(document.looper);
    (void)pthread_mutex_unlock(&document_mutex);
}

void gg_recipe_document_listen(ALooper *looper) {
    (void)pthread_mutex_lock(&document_mutex);
    ALooper_acquire(looper);
    if (document.looper != NULL) ALooper_release(document.looper);
    document.looper = looper;
    (void)pthread_mutex_unlock(&document_mutex);
}

void gg_recipe_document_unlisten(ALooper *looper) {
    (void)pthread_mutex_lock(&document_mutex);
    if (document.looper == looper) {
        ALooper_release(document.looper);
        document.looper = NULL;
    }
    (void)pthread_mutex_unlock(&document_mutex);
}

gg_document_result gg_recipe_document_take(char **path) {
    (void)pthread_mutex_lock(&document_mutex);
    gg_document_result result = document.result;
    *path = document.path;
    document.path = NULL;
    document.result = GG_DOCUMENT_NONE;
    (void)pthread_mutex_unlock(&document_mutex);
    return result;
}

bool gg_recipe_document_request(ANativeActivity *activity, JNIEnv *env) {
    (void)pthread_mutex_lock(&document_mutex);
    if (document.waiting || document.working || document.result != GG_DOCUMENT_NONE) {
        (void)pthread_mutex_unlock(&document_mutex);
        return false;
    }
    document.waiting = true;
    document.launching_activity = (*env)->NewGlobalRef(env, activity->clazz);
    (void)pthread_mutex_unlock(&document_mutex);
    bool success = false;
    if ((*env)->PushLocalFrame(env, 24) != JNI_OK) goto failed;
    jclass activity_type = (*env)->GetObjectClass(env, activity->clazz);
    jclass loader_type = (*env)->FindClass(env, "java/lang/ClassLoader");
    if (exception(env) || activity_type == NULL || loader_type == NULL) goto popped;
    jmethodID get_loader = (*env)->GetMethodID(env, activity_type, "getClassLoader", "()Ljava/lang/ClassLoader;");
    jmethodID load_class = (*env)->GetMethodID(env, loader_type, "loadClass", "(Ljava/lang/String;)Ljava/lang/Class;");
    jmethodID post = (*env)->GetMethodID(env, activity_type, "runOnUiThread", "(Ljava/lang/Runnable;)V");
    if (exception(env) || get_loader == NULL || load_class == NULL || post == NULL) goto popped;
    jobject loader = (*env)->CallObjectMethod(env, activity->clazz, get_loader);
    jstring name = (*env)->NewStringUTF(env, "org.isomorphisms.greengrocer.RecipeDocumentRequest");
    if (exception(env) || loader == NULL || name == NULL) goto popped;
    jclass request_type = (*env)->CallObjectMethod(env, loader, load_class, name);
    if (exception(env) || request_type == NULL) goto popped;
    jmethodID constructor = (*env)->GetMethodID(env, request_type, "<init>", "()V");
    if (exception(env) || constructor == NULL) goto popped;
    jobject request = (*env)->NewObject(env, request_type, constructor);
    if (exception(env) || request == NULL) goto popped;
    (*env)->CallVoidMethod(env, activity->clazz, post, request);
    success = !exception(env);
popped:
    (*env)->PopLocalFrame(env, NULL);
failed:
    if (!success) {
        (void)pthread_mutex_lock(&document_mutex);
        if (document.launching_activity != NULL) (*env)->DeleteGlobalRef(env, document.launching_activity);
        document.launching_activity = NULL;
        document.waiting = false;
        (void)pthread_mutex_unlock(&document_mutex);
    }
    return success;
}

JNIEXPORT void JNICALL
Java_org_isomorphisms_greengrocer_RecipeDocumentRequest_run(JNIEnv *env, jobject receiver);
JNIEXPORT void JNICALL
Java_org_isomorphisms_greengrocer_RecipeDocumentRequest_run(JNIEnv *env, jobject receiver) {
    (void)receiver;
    (void)pthread_mutex_lock(&document_mutex);
    jobject activity = document.launching_activity;
    document.launching_activity = NULL;
    (void)pthread_mutex_unlock(&document_mutex);
    bool success = false;
    if (activity == NULL) return;
    if ((*env)->PushLocalFrame(env, 24) != JNI_OK) goto done;
    jclass intent_type = (*env)->FindClass(env, "android/content/Intent");
    jclass activity_type = (*env)->GetObjectClass(env, activity);
    if (exception(env) || intent_type == NULL || activity_type == NULL) goto popped;
    jmethodID constructor = (*env)->GetMethodID(env, intent_type, "<init>", "(Ljava/lang/String;)V");
    jmethodID category = (*env)->GetMethodID(env, intent_type, "addCategory", "(Ljava/lang/String;)Landroid/content/Intent;");
    jmethodID type = (*env)->GetMethodID(env, intent_type, "setType", "(Ljava/lang/String;)Landroid/content/Intent;");
    jmethodID start = (*env)->GetMethodID(env, activity_type, "startActivityForResult", "(Landroid/content/Intent;I)V");
    if (exception(env) || constructor == NULL || category == NULL || type == NULL || start == NULL) goto popped;
    jstring action = (*env)->NewStringUTF(env, "android.intent.action.OPEN_DOCUMENT");
    jstring openable = (*env)->NewStringUTF(env, "android.intent.category.OPENABLE");
    jstring mime = (*env)->NewStringUTF(env, "*/*");
    if (exception(env) || action == NULL || openable == NULL || mime == NULL) goto popped;
    jobject intent = (*env)->NewObject(env, intent_type, constructor, action);
    if (exception(env) || intent == NULL) goto popped;
    (void)(*env)->CallObjectMethod(env, intent, category, openable);
    (void)(*env)->CallObjectMethod(env, intent, type, mime);
    if (exception(env)) goto popped;
    (*env)->CallVoidMethod(env, activity, start, intent, RECIPE_DOCUMENT_REQUEST);
    success = !exception(env);
popped:
    (*env)->PopLocalFrame(env, NULL);
done:
    (*env)->DeleteGlobalRef(env, activity);
    if (!success) finish(GG_DOCUMENT_FAILED, NULL);
}

static bool copy_bounded(int source, int destination) {
    unsigned char bytes[8192];
    size_t total = 0;
    for (;;) {
        ssize_t count = read(source, bytes, sizeof(bytes));
        if (count < 0 && errno == EINTR) continue;
        if (count < 0) return false;
        if (count == 0) return true;
        total += (size_t)count;
        if (total > DOCUMENT_BYTE_BUDGET) return false;
        size_t written = 0;
        while (written != (size_t)count) {
            ssize_t next = write(destination, bytes + written, (size_t)count - written);
            if (next < 0 && errno == EINTR) continue;
            if (next <= 0) return false;
            written += (size_t)next;
        }
    }
}

static void *copy_document(void *argument) {
    copy_job *job = argument;
    JNIEnv *env = NULL;
    int source = -1, output = -1;
    char temporary[PATH_MAX] = {0};
    bool success = false, frame = false;
    jobject descriptor = NULL;
    jmethodID close_descriptor = NULL;
    if ((*job->vm)->AttachCurrentThreadAsDaemon(job->vm, &env, NULL) != JNI_OK) goto done;
    if ((*env)->PushLocalFrame(env, 32) != JNI_OK) goto attached;
    frame = true;
    jclass activity_type = (*env)->GetObjectClass(env, job->activity);
    jclass resolver_type = (*env)->FindClass(env, "android/content/ContentResolver");
    jclass descriptor_type = (*env)->FindClass(env, "android/os/ParcelFileDescriptor");
    jclass file_type = (*env)->FindClass(env, "java/io/File");
    if (exception(env) || activity_type == NULL || resolver_type == NULL || descriptor_type == NULL || file_type == NULL) goto attached;
    jmethodID get_resolver = (*env)->GetMethodID(env, activity_type, "getContentResolver", "()Landroid/content/ContentResolver;");
    jmethodID get_cache = (*env)->GetMethodID(env, activity_type, "getCacheDir", "()Ljava/io/File;");
    jmethodID get_path = (*env)->GetMethodID(env, file_type, "getAbsolutePath", "()Ljava/lang/String;");
    jmethodID open = (*env)->GetMethodID(env, resolver_type, "openFileDescriptor", "(Landroid/net/Uri;Ljava/lang/String;)Landroid/os/ParcelFileDescriptor;");
    jmethodID get_descriptor = (*env)->GetMethodID(env, descriptor_type, "getFd", "()I");
    close_descriptor = (*env)->GetMethodID(env, descriptor_type, "close", "()V");
    if (exception(env) || get_resolver == NULL || get_cache == NULL || get_path == NULL || open == NULL || get_descriptor == NULL || close_descriptor == NULL) goto attached;
    jobject resolver = (*env)->CallObjectMethod(env, job->activity, get_resolver);
    jobject cache = (*env)->CallObjectMethod(env, job->activity, get_cache);
    if (exception(env) || resolver == NULL || cache == NULL) goto attached;
    jstring path = (*env)->CallObjectMethod(env, cache, get_path);
    if (exception(env) || path == NULL) goto attached;
    const char *directory = (*env)->GetStringUTFChars(env, path, NULL);
    if (exception(env) || directory == NULL) goto attached;
    int length = snprintf(temporary, sizeof(temporary), "%s/recipe-import-XXXXXX", directory);
    (*env)->ReleaseStringUTFChars(env, path, directory);
    if (length < 0 || (size_t)length >= sizeof(temporary)) { temporary[0] = '\0'; goto attached; }
    output = mkstemp(temporary);
    if (output < 0) { temporary[0] = '\0'; goto attached; }
    jstring mode = (*env)->NewStringUTF(env, "r");
    if (exception(env) || mode == NULL) goto attached;
    descriptor = (*env)->CallObjectMethod(env, resolver, open, job->uri, mode);
    if (exception(env) || descriptor == NULL) goto attached;
    jint provider_fd = (*env)->CallIntMethod(env, descriptor, get_descriptor);
    if (exception(env) || provider_fd < 0) goto attached;
    source = dup(provider_fd);
    if (source < 0) goto attached;
    success = copy_bounded(source, output);
attached:
    if (source >= 0) (void)close(source);
    if (output >= 0 && close(output) != 0) success = false;
    if (descriptor != NULL && close_descriptor != NULL) {
        (*env)->CallVoidMethod(env, descriptor, close_descriptor);
        if (exception(env)) success = false;
    }
    (*env)->DeleteGlobalRef(env, job->activity);
    (*env)->DeleteGlobalRef(env, job->uri);
    if (frame) (*env)->PopLocalFrame(env, NULL);
    (*job->vm)->DetachCurrentThread(job->vm);
done:
    if (env == NULL) {
        /* VM teardown cannot produce a successful import or a recipe write. */
        __android_log_print(ANDROID_LOG_ERROR, "GGRecipeDocument", "Document worker could not attach");
    }
    char *ready = success ? strdup(temporary) : NULL;
    if (ready == NULL && temporary[0] != '\0') (void)unlink(temporary);
    finish(ready == NULL ? GG_DOCUMENT_FAILED : GG_DOCUMENT_READY, ready);
    free(job);
    return NULL;
}

JNIEXPORT void JNICALL
Java_org_isomorphisms_greengrocer_RecipeActivity_onActivityResult(JNIEnv *env, jobject activity,
    jint request, jint result, jobject intent);
JNIEXPORT void JNICALL
Java_org_isomorphisms_greengrocer_RecipeActivity_onActivityResult(JNIEnv *env, jobject activity,
    jint request, jint result, jobject intent) {
    if (request != RECIPE_DOCUMENT_REQUEST) return;
    (void)pthread_mutex_lock(&document_mutex);
    bool expected = document.waiting && !document.working;
    if (expected) document.working = true;
    (void)pthread_mutex_unlock(&document_mutex);
    if (!expected) return;
    if (result != -1 || intent == NULL) { finish(GG_DOCUMENT_CANCELLED, NULL); return; }
    jclass intent_type = (*env)->GetObjectClass(env, intent);
    if (exception(env) || intent_type == NULL) { finish(GG_DOCUMENT_FAILED, NULL); return; }
    jmethodID get_uri = (*env)->GetMethodID(env, intent_type, "getData", "()Landroid/net/Uri;");
    if (exception(env) || get_uri == NULL) { finish(GG_DOCUMENT_FAILED, NULL); return; }
    jobject uri = (*env)->CallObjectMethod(env, intent, get_uri);
    if (exception(env) || uri == NULL) { finish(GG_DOCUMENT_FAILED, NULL); return; }
    copy_job *job = calloc(1, sizeof(*job));
    if (job == NULL) { finish(GG_DOCUMENT_FAILED, NULL); return; }
    (void)(*env)->GetJavaVM(env, &job->vm);
    job->activity = (*env)->NewGlobalRef(env, activity);
    job->uri = (*env)->NewGlobalRef(env, uri);
    pthread_t thread;
    if (exception(env) || job->vm == NULL || job->activity == NULL || job->uri == NULL ||
        pthread_create(&thread, NULL, copy_document, job) != 0) {
        if (job->activity != NULL) (*env)->DeleteGlobalRef(env, job->activity);
        if (job->uri != NULL) (*env)->DeleteGlobalRef(env, job->uri);
        free(job);
        finish(GG_DOCUMENT_FAILED, NULL);
    } else (void)pthread_detach(thread);
}
