#include "recipe_engine.h"

#include <android/asset_manager.h>
#include <android/log.h>
#include <dirent.h>
#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

/* One opaque checked session survives disposable NativeActivity windows.
 * JNIEnv is thread-local; only global references and method IDs are retained. */
static pthread_mutex_t runtime_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t effect_mutex = PTHREAD_MUTEX_INITIALIZER;
static struct {
    JavaVM *vm;
    jobject loader;
    jclass generated;
    jstring session;
    jmethodID start, valid, view, action, restore, import, hash, can_go_back;
    jmethodID effectful, merge, input, snapshot, replace;
    ALooper *looper;
    uint64_t revision;
    char directory[PATH_MAX];
} runtime;

static bool exception(JNIEnv *env, const char *operation) {
    if ((*env)->ExceptionCheck(env) == JNI_FALSE) return false;
    __android_log_print(ANDROID_LOG_ERROR, "GGRecipe", "%s failed", operation);
    (*env)->ExceptionDescribe(env);
    (*env)->ExceptionClear(env);
    return true;
}

static bool present(JNIEnv *env, jstring session, gg_recipe_view *view, bool commit) {
    if (session == NULL || exception(env, "session result")) return false;
    /* Opaque transport budget, independent of exact domain numbers. */
    if ((*env)->GetStringLength(env, session) > 8388608) return false;
    jboolean valid = (*env)->CallStaticBooleanMethod(env, runtime.generated, runtime.valid, session);
    if (exception(env, "validate checked session") || valid != JNI_TRUE) return false;
    jstring encoded = (*env)->CallStaticObjectMethod(env, runtime.generated, runtime.view, session);
    if (exception(env, "project checked session") || encoded == NULL) return false;
    jsize units = (*env)->GetStringLength(env, encoded);
    const jchar *text = (*env)->GetStringChars(env, encoded, NULL);
    if (text == NULL || exception(env, "view UTF-16")) return false;
    gg_recipe_view candidate = {0};
    bool decoded = gg_recipe_view_decode(text, (size_t)units, &candidate);
    (*env)->ReleaseStringChars(env, encoded, text);
    if (!decoded) return false;
    jstring retained = NULL;
    if (commit) {
        retained = (*env)->NewGlobalRef(env, session);
        if (retained == NULL || exception(env, "retain checked session")) {
            gg_recipe_view_dispose(&candidate);
            return false;
        }
        if (runtime.session != NULL) (*env)->DeleteGlobalRef(env, runtime.session);
        runtime.session = retained;
        ++runtime.revision;
        if (runtime.looper != NULL) ALooper_wake(runtime.looper);
    }
    gg_recipe_view_dispose(view);
    *view = candidate;
#ifdef GG_RECIPE_OBSERVE
    jint hash = (*env)->CallIntMethod(env, encoded, runtime.hash);
    if (!exception(env, "view observation"))
        __android_log_print(ANDROID_LOG_INFO, "GGRecipeView", "hash=%08x rows=%zu",
            (unsigned int)hash, view->row_count);
#endif
    return true;
}

static bool load_asset(JNIEnv *env, ANativeActivity *activity) {
    AAsset *asset = AAssetManager_open(activity->assetManager, "recipes.dex", AASSET_MODE_BUFFER);
    if (asset == NULL) return false;
    off64_t length = AAsset_getLength64(asset);
    bool success = false;
    if (length <= 0 || length > 16777216) goto done;
    const void *bytes = AAsset_getBuffer(asset);
    if (bytes == NULL) goto done;
    jbyteArray data = (*env)->NewByteArray(env, (jsize)length);
    if (data == NULL || exception(env, "DEX bytes")) goto done;
    (*env)->SetByteArrayRegion(env, data, 0, (jsize)length, bytes);
    if (exception(env, "DEX byte copy")) goto done;
    jclass buffer_type = (*env)->FindClass(env, "java/nio/ByteBuffer");
    jclass loader_type = (*env)->FindClass(env, "dalvik/system/InMemoryDexClassLoader");
    jclass activity_type = (*env)->FindClass(env, "android/app/Activity");
    jclass class_loader = (*env)->FindClass(env, "java/lang/ClassLoader");
    if (exception(env, "DEX SDK classes") || buffer_type == NULL || loader_type == NULL ||
        activity_type == NULL || class_loader == NULL) goto done;
    jmethodID wrap = (*env)->GetStaticMethodID(env, buffer_type, "wrap", "([B)Ljava/nio/ByteBuffer;");
    jmethodID parent_method = (*env)->GetMethodID(env, activity_type, "getClassLoader", "()Ljava/lang/ClassLoader;");
    jmethodID construct = (*env)->GetMethodID(env, loader_type, "<init>",
        "(Ljava/nio/ByteBuffer;Ljava/lang/ClassLoader;)V");
    jmethodID load_class = (*env)->GetMethodID(env, class_loader, "loadClass", "(Ljava/lang/String;)Ljava/lang/Class;");
    if (exception(env, "DEX SDK descriptors") || wrap == NULL || parent_method == NULL ||
        construct == NULL || load_class == NULL) goto done;
    jobject buffer = (*env)->CallStaticObjectMethod(env, buffer_type, wrap, data);
    jobject parent = (*env)->CallObjectMethod(env, activity->clazz, parent_method);
    if (exception(env, "DEX loader inputs") || buffer == NULL || parent == NULL) goto done;
    jobject loader = (*env)->NewObject(env, loader_type, construct, buffer, parent);
    if (exception(env, "DEX loader") || loader == NULL) goto done;
    jstring name = (*env)->NewStringUTF(env, "Idric.Generated");
    if (name == NULL || exception(env, "checked class name")) goto done;
    jclass generated = (*env)->CallObjectMethod(env, loader, load_class, name);
    if (exception(env, "checked recipe class") || generated == NULL) goto done;
    runtime.loader = (*env)->NewGlobalRef(env, loader);
    runtime.generated = (*env)->NewGlobalRef(env, generated);
    if (exception(env, "retain recipe runtime") || runtime.loader == NULL || runtime.generated == NULL) goto done;
    runtime.start = (*env)->GetStaticMethodID(env, generated, "recipe_session_start", "()Ljava/lang/String;");
    runtime.valid = (*env)->GetStaticMethodID(env, generated, "recipe_session_valid", "(Ljava/lang/String;)Z");
    runtime.view = (*env)->GetStaticMethodID(env, generated, "recipe_session_view", "(Ljava/lang/String;)Ljava/lang/String;");
    runtime.action = (*env)->GetStaticMethodID(env, generated, "recipe_session_action",
        "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)Ljava/lang/String;");
    runtime.restore = (*env)->GetStaticMethodID(env, generated, "recipe_session_restore",
        "(Ljava/lang/String;Ljava/lang/String;)Ljava/lang/String;");
    runtime.import = (*env)->GetStaticMethodID(env, generated, "recipe_session_import",
        "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)Ljava/lang/String;");
    runtime.can_go_back = (*env)->GetStaticMethodID(env, generated, "recipe_session_can_go_back", "(Ljava/lang/String;)Z");
    runtime.effectful = (*env)->GetStaticMethodID(env, generated, "recipe_session_effectful",
        "(Ljava/lang/String;Ljava/lang/String;)Z");
    runtime.merge = (*env)->GetStaticMethodID(env, generated, "recipe_session_merge_effect",
        "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)Ljava/lang/String;");
    runtime.snapshot = (*env)->GetStaticMethodID(env, generated, "recipe_session_input_snapshot",
        "(Ljava/lang/String;)Ljava/lang/String;");
    runtime.input = (*env)->GetStaticMethodID(env, generated, "recipe_session_input",
        "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;II)Ljava/lang/String;");
    runtime.replace = (*env)->GetStaticMethodID(env, generated, "recipe_session_replace",
        "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;III)Ljava/lang/String;");
    jclass string_type = (*env)->FindClass(env, "java/lang/String");
    if (exception(env, "recipe export descriptors") || string_type == NULL) goto done;
    runtime.hash = (*env)->GetMethodID(env, string_type, "hashCode", "()I");
    success = !exception(env, "view hash descriptor") && runtime.start != NULL && runtime.valid != NULL &&
        runtime.view != NULL && runtime.action != NULL && runtime.restore != NULL && runtime.import != NULL &&
        runtime.hash != NULL && runtime.can_go_back != NULL && runtime.effectful != NULL &&
        runtime.merge != NULL && runtime.input != NULL && runtime.snapshot != NULL && runtime.replace != NULL;
done:
    AAsset_close(asset);
    return success;
}

static bool restore_directory(gg_recipe_engine *engine, gg_recipe_view *view) {
    struct dirent **entries = NULL;
    int count = scandir(engine->directory, &entries, NULL, alphasort);
    if (count < 0) return false;
    bool success = true;
    for (int index = 0; index < count; ++index) {
        char *name = entries[index]->d_name;
        size_t length = strlen(name);
        /* AtomicFile's old-API backup may temporarily be the only directory
         * entry. Feed its base path to the real production recovery reader. */
        if (length > 4 && strcmp(name + length - 4, ".bak") == 0) name[length -= 4] = '\0';
        if (length > 7 && strcmp(name + length - 7, ".recipe") == 0) {
            char path[PATH_MAX];
            int size = snprintf(path, sizeof(path), "%s/%s", engine->directory, name);
            if (size < 0 || (size_t)size >= sizeof(path)) { success = false; }
            else if ((*engine->env)->PushLocalFrame(engine->env, 16) != JNI_OK) { success = false; }
            else {
                jstring location = (*engine->env)->NewStringUTF(engine->env, path);
                jstring next = location == NULL ? NULL : (*engine->env)->CallStaticObjectMethod(
                    engine->env, runtime.generated, runtime.restore, runtime.session, location);
                if (exception(engine->env, "restore recipe file") || !present(engine->env, next, view, true)) success = false;
                (*engine->env)->PopLocalFrame(engine->env, NULL);
            }
        }
        free(entries[index]);
    }
    free(entries);
    return success;
}

bool gg_recipe_engine_open(gg_recipe_engine *engine, ANativeActivity *activity,
                           gg_recipe_view *view) {
    memset(engine, 0, sizeof(*engine));
    engine->vm = activity->vm;
    jint status = (*engine->vm)->GetEnv(engine->vm, (void **)&engine->env, JNI_VERSION_1_6);
    if (status == JNI_EDETACHED) {
        if ((*engine->vm)->AttachCurrentThread(engine->vm, &engine->env, NULL) != JNI_OK) return false;
        engine->detach = true;
    } else if (status != JNI_OK) return false;
    int length = snprintf(engine->directory, sizeof(engine->directory), "%s/recipes", activity->internalDataPath);
    if (length < 0 || (size_t)length >= sizeof(engine->directory)) return false;
    if (mkdir(engine->directory, 0700) != 0 && errno != EEXIST) return false;
    struct stat directory;
    if (lstat(engine->directory, &directory) != 0 || !S_ISDIR(directory.st_mode)) return false;
    bool success = false;
    (void)pthread_mutex_lock(&runtime_mutex);
    if ((*engine->env)->PushLocalFrame(engine->env, 48) != JNI_OK) goto unlocked;
    if (runtime.vm != NULL) {
        if (runtime.vm == engine->vm && strcmp(runtime.directory, engine->directory) == 0)
            success = present(engine->env, runtime.session, view, false);
    } else if (load_asset(engine->env, activity)) {
        jstring initial = (*engine->env)->CallStaticObjectMethod(engine->env, runtime.generated, runtime.start);
        if (!exception(engine->env, "new recipe session") && present(engine->env, initial, view, true)) {
            runtime.vm = engine->vm;
            memcpy(runtime.directory, engine->directory, (size_t)length + 1);
            success = restore_directory(engine, view);
        }
    }
    if (!success && runtime.vm == NULL) {
        if (runtime.loader != NULL) (*engine->env)->DeleteGlobalRef(engine->env, runtime.loader);
        if (runtime.generated != NULL) (*engine->env)->DeleteGlobalRef(engine->env, runtime.generated);
        if (runtime.session != NULL) (*engine->env)->DeleteGlobalRef(engine->env, runtime.session);
        memset(&runtime, 0, sizeof(runtime));
    }
    (*engine->env)->PopLocalFrame(engine->env, NULL);
    engine->revision = runtime.revision;
unlocked:
    (void)pthread_mutex_unlock(&runtime_mutex);
    return success;
}

void gg_recipe_engine_close(gg_recipe_engine *engine) {
    if (engine->detach) (*engine->vm)->DetachCurrentThread(engine->vm);
    memset(engine, 0, sizeof(*engine));
}

static bool operation(gg_recipe_engine *engine, const gg_text *token, const char *path,
                      gg_recipe_view *view) {
    bool success = false;
    JNIEnv *env = engine->env;
    (void)pthread_mutex_lock(&effect_mutex);
    (void)pthread_mutex_lock(&runtime_mutex);
    if (runtime.vm != engine->vm || runtime.session == NULL || (*env)->PushLocalFrame(env, 32) != JNI_OK) goto done;
    jstring directory = (*env)->NewStringUTF(env, engine->directory);
    jstring argument = token == NULL ? (*env)->NewStringUTF(env, path) :
        (*env)->NewString(env, token->characters, (jsize)token->length);
    if (!exception(env, "recipe operation inputs") && directory != NULL && argument != NULL) {
        jstring before = (*env)->NewLocalRef(env, runtime.session);
        bool effect = token == NULL;
        if (!effect) effect = (*env)->CallStaticBooleanMethod(env, runtime.generated,
            runtime.effectful, before, argument) == JNI_TRUE;
        if (!exception(env, "classify checked effect") && before != NULL) {
            /* No filesystem work holds the mutex used by keyboard callbacks. */
            if (effect) (void)pthread_mutex_unlock(&runtime_mutex);
            jstring next = (*env)->CallStaticObjectMethod(env, runtime.generated,
                token == NULL ? runtime.import : runtime.action, before, argument, directory);
            bool failed = exception(env, "checked recipe operation");
            if (effect) {
                (void)pthread_mutex_lock(&runtime_mutex);
                if (!failed && next != NULL) {
                    next = (*env)->CallStaticObjectMethod(env, runtime.generated, runtime.merge,
                        before, runtime.session, next);
                    failed = exception(env, "merge late file effect");
                }
            }
            if (!failed) success = present(env, next, view, true);
            engine->revision = runtime.revision;
        }
    }
    (*env)->PopLocalFrame(env, NULL);
done:
    (void)pthread_mutex_unlock(&runtime_mutex);
    (void)pthread_mutex_unlock(&effect_mutex);
    return success;
}

bool gg_recipe_engine_action(gg_recipe_engine *engine, const gg_text *token,
                             gg_recipe_view *view) {
    return operation(engine, token, NULL, view);
}

bool gg_recipe_engine_import(gg_recipe_engine *engine, const char *path,
                             gg_recipe_view *view) {
    return operation(engine, NULL, path, view);
}

int gg_recipe_engine_back(gg_recipe_engine *engine, gg_recipe_view *view) {
    (void)pthread_mutex_lock(&runtime_mutex);
    jboolean handled = (*engine->env)->CallStaticBooleanMethod(engine->env, runtime.generated,
        runtime.can_go_back, runtime.session);
    bool failed = exception(engine->env, "checked back navigation");
    (void)pthread_mutex_unlock(&runtime_mutex);
    if (failed) return -1;
    if (handled != JNI_TRUE) return 0;
    static uint16_t token[] = {'s','y','s','t','e','m',':','b','a','c','k'};
    gg_text action = {token, sizeof(token) / sizeof(token[0])};
    return gg_recipe_engine_action(engine, &action, view) ? 1 : -1;
}

void gg_recipe_engine_listen(ALooper *looper) {
    (void)pthread_mutex_lock(&runtime_mutex);
    ALooper_acquire(looper);
    if (runtime.looper != NULL) ALooper_release(runtime.looper);
    runtime.looper = looper;
    (void)pthread_mutex_unlock(&runtime_mutex);
}

void gg_recipe_engine_unlisten(ALooper *looper) {
    (void)pthread_mutex_lock(&runtime_mutex);
    if (runtime.looper == looper) { ALooper_release(looper); runtime.looper = NULL; }
    (void)pthread_mutex_unlock(&runtime_mutex);
}

int gg_recipe_engine_refresh(gg_recipe_engine *engine, gg_recipe_view *view) {
    int result = 0;
    (void)pthread_mutex_lock(&runtime_mutex);
    if (engine->revision != runtime.revision) {
        if ((*engine->env)->PushLocalFrame(engine->env, 24) != JNI_OK) result = -1;
        else {
            result = present(engine->env, runtime.session, view, false) ? 1 : -1;
            if (result > 0) engine->revision = runtime.revision;
            (*engine->env)->PopLocalFrame(engine->env, NULL);
        }
    }
    (void)pthread_mutex_unlock(&runtime_mutex);
    return result;
}

static bool field_is(gg_recipe_fields value, const char *literal) {
    size_t length = strlen(literal);
    if (value.units != length) return false;
    for (size_t index = 0; index < length; ++index)
        if (value.text[index] != (unsigned char)literal[index]) return false;
    return true;
}

static bool text_offset(gg_recipe_fields value, bool optional, jint *output) {
    if (optional && field_is(value, "-1")) { *output = -1; return true; }
    if (value.units == 0) return false;
    jint number = 0;
    for (size_t index = 0; index < value.units; ++index) {
        uint16_t digit = value.text[index];
        if (digit < '0' || digit > '9' || number > (262144 - (digit - '0')) / 10) return false;
        number = number * 10 + digit - '0';
    }
    *output = number;
    return true;
}

bool gg_recipe_engine_snapshot(JNIEnv *env, gg_recipe_input_snapshot *snapshot) {
    bool success = false;
    (void)pthread_mutex_lock(&runtime_mutex);
    if (runtime.session == NULL) goto done;
    jstring encoded = (*env)->CallStaticObjectMethod(env, runtime.generated, runtime.snapshot, runtime.session);
    if (encoded == NULL || exception(env, "keyboard snapshot")) goto done;
    jsize units = (*env)->GetStringLength(env, encoded);
    if (units > 8388608) goto done;
    const jchar *text = (*env)->GetStringChars(env, encoded, NULL);
    if (text == NULL || exception(env, "keyboard snapshot UTF-16")) goto done;
    gg_recipe_fields top = {text, (size_t)units, 0}, values[7];
    success = true;
    for (size_t index = 0; index < 7; ++index)
        if (!gg_recipe_field(&top, &values[index])) { success = false; break; }
    if (success) success = top.cursor == top.units && field_is(values[0], "green-grocer-input-v1") &&
        values[2].units <= 262144 && text_offset(values[3], false, &snapshot->first) &&
        text_offset(values[4], false, &snapshot->last) &&
        text_offset(values[5], true, &snapshot->composing_first) &&
        text_offset(values[6], true, &snapshot->composing_last);
    if (success) {
        snapshot->target = (*env)->NewString(env, values[1].text, (jsize)values[1].units);
        snapshot->text = (*env)->NewString(env, values[2].text, (jsize)values[2].units);
        success = snapshot->target != NULL && snapshot->text != NULL && !exception(env, "keyboard snapshot copy");
    }
    (*env)->ReleaseStringChars(env, encoded, text);
done:
    (void)pthread_mutex_unlock(&runtime_mutex);
    return success;
}

static bool apply_input_reply(JNIEnv *env, jstring encoded) {
    bool accepted = false;
    if (encoded != NULL && !exception(env, "checked keyboard edit")) {
        jsize units = (*env)->GetStringLength(env, encoded);
        const jchar *characters = units <= 8388608 ? (*env)->GetStringChars(env, encoded, NULL) : NULL;
        if (characters != NULL) {
            gg_recipe_fields top = {characters, (size_t)units, 0}, status, session;
            if (gg_recipe_field(&top, &status) && gg_recipe_field(&top, &session) &&
                top.cursor == top.units && (field_is(status, "yes") || field_is(status, "no"))) {
                jstring next = (*env)->NewString(env, session.text, (jsize)session.units);
                gg_recipe_view projected = {0};
                bool committed = present(env, next, &projected, true);
                gg_recipe_view_dispose(&projected);
                accepted = committed && field_is(status, "yes");
            }
            (*env)->ReleaseStringChars(env, encoded, characters);
        }
    }
    return accepted;
}

bool gg_recipe_engine_input(JNIEnv *env, jstring expected, const char *operation,
                            jstring text, jint first, jint last) {
    bool accepted = false;
    (void)pthread_mutex_lock(&runtime_mutex);
    if (runtime.session == NULL || expected == NULL || (*env)->PushLocalFrame(env, 24) != JNI_OK) goto done;
    jstring command = (*env)->NewStringUTF(env, operation);
    jstring payload = text == NULL ? (*env)->NewStringUTF(env, "") : text;
    jstring encoded = command == NULL || payload == NULL ? NULL :
        (*env)->CallStaticObjectMethod(env, runtime.generated, runtime.input,
            runtime.session, expected, command, payload, first, last);
    accepted = apply_input_reply(env, encoded);
    (*env)->PopLocalFrame(env, NULL);
done:
    (void)pthread_mutex_unlock(&runtime_mutex);
    return accepted;
}

bool gg_recipe_engine_replace(JNIEnv *env, jstring expected, jstring text,
                              jint first, jint last, jint cursor) {
    bool accepted = false;
    (void)pthread_mutex_lock(&runtime_mutex);
    if (runtime.session != NULL && expected != NULL && text != NULL && (*env)->PushLocalFrame(env, 24) == JNI_OK) {
        jstring encoded = (*env)->CallStaticObjectMethod(env, runtime.generated, runtime.replace,
            runtime.session, expected, text, first, last, cursor);
        accepted = apply_input_reply(env, encoded);
        (*env)->PopLocalFrame(env, NULL);
    }
    (void)pthread_mutex_unlock(&runtime_mutex);
    return accepted;
}
