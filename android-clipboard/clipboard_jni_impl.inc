#include "clipboard.h"
#include "utf8.h"

#include <limits.h>
#include <stdint.h>
#include <stdlib.h>

_Static_assert(sizeof(jchar) == sizeof(uint16_t), "JNI jchar must be UTF-16 sized");

static JavaVM *clipboard_vm = NULL;
static jobject clipboard_manager = NULL;

typedef struct {
    JNIEnv *environment;
    int detach;
} AttachedEnvironment;

static int take_exception(JNIEnv *environment) {
    if ((*environment)->ExceptionCheck(environment) == JNI_FALSE) {
        return 0;
    }
    (*environment)->ExceptionClear(environment);
    return 1;
}

static int object_failed(JNIEnv *environment, const void *object) {
    const int exception = take_exception(environment);
    return exception || object == NULL;
}

/* NewStringUTF is used only for fixed ASCII framework names/keys. */
static jstring new_ascii(JNIEnv *environment, const char *ascii) {
    jstring string = (*environment)->NewStringUTF(environment, ascii);
    return object_failed(environment, string) ? NULL : string;
}

static ClipboardStatus acquire_environment(AttachedEnvironment *attached) {
    attached->environment = NULL;
    attached->detach = 0;
    if (clipboard_vm == NULL || clipboard_manager == NULL) {
        return CLIPBOARD_NOT_INITIALIZED;
    }

    const jint status = (*clipboard_vm)->GetEnv(
        clipboard_vm, (void **)&attached->environment, JNI_VERSION_1_6);
    if (status == JNI_OK) {
        return CLIPBOARD_OK;
    }
    if (status != JNI_EDETACHED) {
        return CLIPBOARD_JNI_ERROR;
    }
    if ((*clipboard_vm)->AttachCurrentThread(
            clipboard_vm, (void **)&attached->environment, NULL) != JNI_OK) {
        attached->environment = NULL;
        return CLIPBOARD_JNI_ERROR;
    }
    attached->detach = 1;
    return CLIPBOARD_OK;
}

static void release_environment(AttachedEnvironment *attached) {
    if (attached->detach && clipboard_vm != NULL) {
        (void)(*clipboard_vm)->DetachCurrentThread(clipboard_vm);
    }
    attached->environment = NULL;
    attached->detach = 0;
}

static ClipboardStatus get_api_level(JNIEnv *environment, jint *out_api_level) {
    jclass version_class = (*environment)->FindClass(
        environment, "android/os/Build$VERSION");
    if (object_failed(environment, version_class)) {
        return CLIPBOARD_JNI_ERROR;
    }

    const jfieldID sdk_int = (*environment)->GetStaticFieldID(
        environment, version_class, "SDK_INT", "I");
    if (object_failed(environment, (const void *)sdk_int)) {
        (*environment)->DeleteLocalRef(environment, version_class);
        return CLIPBOARD_JNI_ERROR;
    }

    const jint api_level = (*environment)->GetStaticIntField(
        environment, version_class, sdk_int);
    if (take_exception(environment)) {
        (*environment)->DeleteLocalRef(environment, version_class);
        return CLIPBOARD_JNI_ERROR;
    }

    (*environment)->DeleteLocalRef(environment, version_class);
    *out_api_level = api_level;
    return CLIPBOARD_OK;
}

static ClipboardStatus map_utf_status(ClipboardUtfStatus status) {
    switch (status) {
        case CLIPBOARD_UTF_OK:
            return CLIPBOARD_OK;
        case CLIPBOARD_UTF_INVALID:
            return CLIPBOARD_INVALID_UTF8;
        case CLIPBOARD_UTF_OUT_OF_MEMORY:
            return CLIPBOARD_OUT_OF_MEMORY;
    }
    return CLIPBOARD_JNI_ERROR;
}

ClipboardStatus clipboard_bridge_init(JNIEnv *environment, jobject context) {
    if (environment == NULL || context == NULL) {
        return CLIPBOARD_INVALID_ARGUMENT;
    }
    if (clipboard_vm != NULL || clipboard_manager != NULL) {
        return CLIPBOARD_ALREADY_INITIALIZED;
    }

    JavaVM *vm = NULL;
    if ((*environment)->GetJavaVM(environment, &vm) != JNI_OK || vm == NULL) {
        return CLIPBOARD_JNI_ERROR;
    }

    ClipboardStatus status = CLIPBOARD_JNI_ERROR;
    jclass context_class = NULL;
    jclass manager_class = NULL;
    jstring service_name = NULL;
    jobject manager = NULL;
    jobject global_manager = NULL;

    context_class = (*environment)->GetObjectClass(environment, context);
    if (object_failed(environment, context_class)) {
        goto cleanup;
    }
    const jmethodID get_system_service = (*environment)->GetMethodID(
        environment, context_class, "getSystemService",
        "(Ljava/lang/String;)Ljava/lang/Object;");
    if (object_failed(environment, (const void *)get_system_service)) {
        goto cleanup;
    }

    service_name = new_ascii(environment, "clipboard");
    if (service_name == NULL) {
        goto cleanup;
    }
    manager = (*environment)->CallObjectMethod(
        environment, context, get_system_service, service_name);
    if (object_failed(environment, manager)) {
        goto cleanup;
    }

    manager_class = (*environment)->FindClass(
        environment, "android/content/ClipboardManager");
    if (object_failed(environment, manager_class)) {
        goto cleanup;
    }
    const jboolean is_manager = (*environment)->IsInstanceOf(
        environment, manager, manager_class);
    if (take_exception(environment) || is_manager != JNI_TRUE) {
        goto cleanup;
    }

    global_manager = (*environment)->NewGlobalRef(environment, manager);
    if (object_failed(environment, global_manager)) {
        goto cleanup;
    }

    clipboard_vm = vm;
    clipboard_manager = global_manager;
    global_manager = NULL;
    status = CLIPBOARD_OK;

cleanup:
    (void)take_exception(environment);
    if (global_manager != NULL) {
        (*environment)->DeleteGlobalRef(environment, global_manager);
    }
    if (manager_class != NULL) {
        (*environment)->DeleteLocalRef(environment, manager_class);
    }
    if (manager != NULL) {
        (*environment)->DeleteLocalRef(environment, manager);
    }
    if (service_name != NULL) {
        (*environment)->DeleteLocalRef(environment, service_name);
    }
    if (context_class != NULL) {
        (*environment)->DeleteLocalRef(environment, context_class);
    }
    return status;
}

ClipboardStatus clipboard_bridge_shutdown(void) {
    if (clipboard_vm == NULL || clipboard_manager == NULL) {
        return CLIPBOARD_NOT_INITIALIZED;
    }

    AttachedEnvironment attached;
    const ClipboardStatus status = acquire_environment(&attached);
    if (status != CLIPBOARD_OK) {
        return status;
    }

    jobject manager = clipboard_manager;
    (*attached.environment)->DeleteGlobalRef(attached.environment, manager);
    clipboard_manager = NULL;
    release_environment(&attached);
    clipboard_vm = NULL;
    return CLIPBOARD_OK;
}

ClipboardStatus clipboard_has_text(int *out_has_text) {
    if (out_has_text == NULL) {
        return CLIPBOARD_INVALID_ARGUMENT;
    }
    *out_has_text = 0;

    AttachedEnvironment attached;
    ClipboardStatus status = acquire_environment(&attached);
    if (status != CLIPBOARD_OK) {
        return status;
    }
    JNIEnv *environment = attached.environment;
    jclass manager_class = NULL;
    jobject description = NULL;
    jclass description_class = NULL;
    jstring text_pattern = NULL;

    manager_class = (*environment)->GetObjectClass(environment, clipboard_manager);
    if (object_failed(environment, manager_class)) {
        status = CLIPBOARD_JNI_ERROR;
        goto cleanup;
    }
    const jmethodID get_description = (*environment)->GetMethodID(
        environment, manager_class, "getPrimaryClipDescription",
        "()Landroid/content/ClipDescription;");
    if (object_failed(environment, (const void *)get_description)) {
        status = CLIPBOARD_JNI_ERROR;
        goto cleanup;
    }

    description = (*environment)->CallObjectMethod(
        environment, clipboard_manager, get_description);
    if (take_exception(environment)) {
        status = CLIPBOARD_JNI_ERROR;
        goto cleanup;
    }
    if (description == NULL) {
        status = CLIPBOARD_NO_VISIBLE_CLIP;
        goto cleanup;
    }

    description_class = (*environment)->GetObjectClass(environment, description);
    if (object_failed(environment, description_class)) {
        status = CLIPBOARD_JNI_ERROR;
        goto cleanup;
    }
    const jmethodID has_mime_type = (*environment)->GetMethodID(
        environment, description_class, "hasMimeType", "(Ljava/lang/String;)Z");
    if (object_failed(environment, (const void *)has_mime_type)) {
        status = CLIPBOARD_JNI_ERROR;
        goto cleanup;
    }

    text_pattern = new_ascii(environment, "text/*");
    if (text_pattern == NULL) {
        status = CLIPBOARD_JNI_ERROR;
        goto cleanup;
    }
    const jboolean has_text = (*environment)->CallBooleanMethod(
        environment, description, has_mime_type, text_pattern);
    if (take_exception(environment)) {
        status = CLIPBOARD_JNI_ERROR;
        goto cleanup;
    }

    *out_has_text = has_text == JNI_TRUE ? 1 : 0;
    status = CLIPBOARD_OK;

cleanup:
    (void)take_exception(environment);
    if (text_pattern != NULL) {
        (*environment)->DeleteLocalRef(environment, text_pattern);
    }
    if (description_class != NULL) {
        (*environment)->DeleteLocalRef(environment, description_class);
    }
    if (description != NULL) {
        (*environment)->DeleteLocalRef(environment, description);
    }
    if (manager_class != NULL) {
        (*environment)->DeleteLocalRef(environment, manager_class);
    }
    release_environment(&attached);
    return status;
}

ClipboardStatus clipboard_get_utf8(char **out_bytes, size_t *out_len) {
    if (out_bytes == NULL || out_len == NULL) {
        return CLIPBOARD_INVALID_ARGUMENT;
    }
    *out_bytes = NULL;
    *out_len = 0;

    AttachedEnvironment attached;
    ClipboardStatus status = acquire_environment(&attached);
    if (status != CLIPBOARD_OK) {
        return status;
    }
    JNIEnv *environment = attached.environment;
    jclass manager_class = NULL;
    jobject clip = NULL;
    jclass clip_class = NULL;
    jobject item = NULL;
    jclass item_class = NULL;
    jobject text = NULL;
    jclass text_class = NULL;
    jstring string = NULL;
    const jchar *characters = NULL;

    manager_class = (*environment)->GetObjectClass(environment, clipboard_manager);
    if (object_failed(environment, manager_class)) {
        status = CLIPBOARD_JNI_ERROR;
        goto cleanup;
    }
    const jmethodID get_primary_clip = (*environment)->GetMethodID(
        environment, manager_class, "getPrimaryClip", "()Landroid/content/ClipData;");
    if (object_failed(environment, (const void *)get_primary_clip)) {
        status = CLIPBOARD_JNI_ERROR;
        goto cleanup;
    }

    clip = (*environment)->CallObjectMethod(
        environment, clipboard_manager, get_primary_clip);
    if (take_exception(environment)) {
        status = CLIPBOARD_JNI_ERROR;
        goto cleanup;
    }
    if (clip == NULL) {
        status = CLIPBOARD_NO_VISIBLE_CLIP;
        goto cleanup;
    }

    clip_class = (*environment)->GetObjectClass(environment, clip);
    if (object_failed(environment, clip_class)) {
        status = CLIPBOARD_JNI_ERROR;
        goto cleanup;
    }
    const jmethodID get_item_count = (*environment)->GetMethodID(
        environment, clip_class, "getItemCount", "()I");
    if (object_failed(environment, (const void *)get_item_count)) {
        status = CLIPBOARD_JNI_ERROR;
        goto cleanup;
    }
    const jmethodID get_item_at = (*environment)->GetMethodID(
        environment, clip_class, "getItemAt", "(I)Landroid/content/ClipData$Item;");
    if (object_failed(environment, (const void *)get_item_at)) {
        status = CLIPBOARD_JNI_ERROR;
        goto cleanup;
    }

    const jint item_count = (*environment)->CallIntMethod(
        environment, clip, get_item_count);
    if (take_exception(environment)) {
        status = CLIPBOARD_JNI_ERROR;
        goto cleanup;
    }
    if (item_count <= 0) {
        status = CLIPBOARD_NO_TEXT;
        goto cleanup;
    }

    item = (*environment)->CallObjectMethod(environment, clip, get_item_at, 0);
    if (object_failed(environment, item)) {
        status = CLIPBOARD_JNI_ERROR;
        goto cleanup;
    }
    item_class = (*environment)->GetObjectClass(environment, item);
    if (object_failed(environment, item_class)) {
        status = CLIPBOARD_JNI_ERROR;
        goto cleanup;
    }
    const jmethodID get_text = (*environment)->GetMethodID(
        environment, item_class, "getText", "()Ljava/lang/CharSequence;");
    if (object_failed(environment, (const void *)get_text)) {
        status = CLIPBOARD_JNI_ERROR;
        goto cleanup;
    }

    text = (*environment)->CallObjectMethod(environment, item, get_text);
    if (take_exception(environment)) {
        status = CLIPBOARD_JNI_ERROR;
        goto cleanup;
    }
    if (text == NULL) {
        status = CLIPBOARD_NO_TEXT;
        goto cleanup;
    }

    text_class = (*environment)->GetObjectClass(environment, text);
    if (object_failed(environment, text_class)) {
        status = CLIPBOARD_JNI_ERROR;
        goto cleanup;
    }
    const jmethodID to_string = (*environment)->GetMethodID(
        environment, text_class, "toString", "()Ljava/lang/String;");
    if (object_failed(environment, (const void *)to_string)) {
        status = CLIPBOARD_JNI_ERROR;
        goto cleanup;
    }

    string = (jstring)(*environment)->CallObjectMethod(environment, text, to_string);
    if (object_failed(environment, string)) {
        status = CLIPBOARD_JNI_ERROR;
        goto cleanup;
    }
    const jsize character_count = (*environment)->GetStringLength(environment, string);
    if (take_exception(environment)) {
        status = CLIPBOARD_JNI_ERROR;
        goto cleanup;
    }
    characters = (*environment)->GetStringChars(environment, string, NULL);
    if (object_failed(environment, characters)) {
        characters = NULL;
        status = CLIPBOARD_JNI_ERROR;
        goto cleanup;
    }

    status = map_utf_status(clipboard_utf16_to_utf8(
        (const uint16_t *)characters,
        (size_t)character_count,
        out_bytes,
        out_len));

cleanup:
    (void)take_exception(environment);
    if (characters != NULL) {
        (*environment)->ReleaseStringChars(environment, string, characters);
    }
    if (string != NULL) {
        (*environment)->DeleteLocalRef(environment, string);
    }
    if (text_class != NULL) {
        (*environment)->DeleteLocalRef(environment, text_class);
    }
    if (text != NULL) {
        (*environment)->DeleteLocalRef(environment, text);
    }
    if (item_class != NULL) {
        (*environment)->DeleteLocalRef(environment, item_class);
    }
    if (item != NULL) {
        (*environment)->DeleteLocalRef(environment, item);
    }
    if (clip_class != NULL) {
        (*environment)->DeleteLocalRef(environment, clip_class);
    }
    if (clip != NULL) {
        (*environment)->DeleteLocalRef(environment, clip);
    }
    if (manager_class != NULL) {
        (*environment)->DeleteLocalRef(environment, manager_class);
    }
    release_environment(&attached);
    return status;
}

static ClipboardStatus make_plain_clip(
    JNIEnv *environment,
    jstring text,
    int sensitive,
    jobject *out_clip) {
    ClipboardStatus status = CLIPBOARD_JNI_ERROR;
    jclass clip_class = NULL;
    jstring label = NULL;
    jobject clip = NULL;
    jobject description = NULL;
    jclass description_class = NULL;
    jclass bundle_class = NULL;
    jobject bundle = NULL;
    jstring sensitive_key = NULL;
    const jchar empty_unit = 0;

    *out_clip = NULL;
    clip_class = (*environment)->FindClass(environment, "android/content/ClipData");
    if (object_failed(environment, clip_class)) {
        goto cleanup;
    }
    const jmethodID new_plain_text = (*environment)->GetStaticMethodID(
        environment, clip_class, "newPlainText",
        "(Ljava/lang/CharSequence;Ljava/lang/CharSequence;)Landroid/content/ClipData;");
    if (object_failed(environment, (const void *)new_plain_text)) {
        goto cleanup;
    }

    label = (*environment)->NewString(environment, &empty_unit, 0);
    if (object_failed(environment, label)) {
        goto cleanup;
    }
    clip = (*environment)->CallStaticObjectMethod(
        environment, clip_class, new_plain_text, label, text);
    if (object_failed(environment, clip)) {
        goto cleanup;
    }

    if (sensitive) {
        jint api_level = 0;
        status = get_api_level(environment, &api_level);
        if (status != CLIPBOARD_OK) {
            goto cleanup;
        }
        if (api_level >= 24) {
            const jmethodID get_description = (*environment)->GetMethodID(
                environment, clip_class, "getDescription",
                "()Landroid/content/ClipDescription;");
            if (object_failed(environment, (const void *)get_description)) {
                status = CLIPBOARD_JNI_ERROR;
                goto cleanup;
            }
            description = (*environment)->CallObjectMethod(
                environment, clip, get_description);
            if (object_failed(environment, description)) {
                status = CLIPBOARD_JNI_ERROR;
                goto cleanup;
            }

            description_class = (*environment)->GetObjectClass(environment, description);
            if (object_failed(environment, description_class)) {
                status = CLIPBOARD_JNI_ERROR;
                goto cleanup;
            }
            bundle_class = (*environment)->FindClass(
                environment, "android/os/PersistableBundle");
            if (object_failed(environment, bundle_class)) {
                status = CLIPBOARD_JNI_ERROR;
                goto cleanup;
            }

            const jmethodID bundle_constructor = (*environment)->GetMethodID(
                environment, bundle_class, "<init>", "()V");
            if (object_failed(environment, (const void *)bundle_constructor)) {
                status = CLIPBOARD_JNI_ERROR;
                goto cleanup;
            }
            const jmethodID put_boolean = (*environment)->GetMethodID(
                environment, bundle_class, "putBoolean", "(Ljava/lang/String;Z)V");
            if (object_failed(environment, (const void *)put_boolean)) {
                status = CLIPBOARD_JNI_ERROR;
                goto cleanup;
            }
            const jmethodID set_extras = (*environment)->GetMethodID(
                environment, description_class, "setExtras",
                "(Landroid/os/PersistableBundle;)V");
            if (object_failed(environment, (const void *)set_extras)) {
                status = CLIPBOARD_JNI_ERROR;
                goto cleanup;
            }

            bundle = (*environment)->NewObject(
                environment, bundle_class, bundle_constructor);
            if (object_failed(environment, bundle)) {
                status = CLIPBOARD_JNI_ERROR;
                goto cleanup;
            }
            sensitive_key = new_ascii(
                environment, "android.content.extra.IS_SENSITIVE");
            if (sensitive_key == NULL) {
                status = CLIPBOARD_JNI_ERROR;
                goto cleanup;
            }

            (*environment)->CallVoidMethod(
                environment, bundle, put_boolean, sensitive_key, JNI_TRUE);
            if (take_exception(environment)) {
                status = CLIPBOARD_JNI_ERROR;
                goto cleanup;
            }
            (*environment)->CallVoidMethod(
                environment, description, set_extras, bundle);
            if (take_exception(environment)) {
                status = CLIPBOARD_JNI_ERROR;
                goto cleanup;
            }
        }
    }

    *out_clip = clip;
    clip = NULL;
    status = CLIPBOARD_OK;

cleanup:
    (void)take_exception(environment);
    if (sensitive_key != NULL) {
        (*environment)->DeleteLocalRef(environment, sensitive_key);
    }
    if (bundle != NULL) {
        (*environment)->DeleteLocalRef(environment, bundle);
    }
    if (bundle_class != NULL) {
        (*environment)->DeleteLocalRef(environment, bundle_class);
    }
    if (description_class != NULL) {
        (*environment)->DeleteLocalRef(environment, description_class);
    }
    if (description != NULL) {
        (*environment)->DeleteLocalRef(environment, description);
    }
    if (clip != NULL) {
        (*environment)->DeleteLocalRef(environment, clip);
    }
    if (label != NULL) {
        (*environment)->DeleteLocalRef(environment, label);
    }
    if (clip_class != NULL) {
        (*environment)->DeleteLocalRef(environment, clip_class);
    }
    return status;
}

static ClipboardStatus set_primary_clip(JNIEnv *environment, jobject clip) {
    jclass manager_class = (*environment)->GetObjectClass(
        environment, clipboard_manager);
    if (object_failed(environment, manager_class)) {
        return CLIPBOARD_JNI_ERROR;
    }
    const jmethodID set_primary = (*environment)->GetMethodID(
        environment, manager_class, "setPrimaryClip", "(Landroid/content/ClipData;)V");
    if (object_failed(environment, (const void *)set_primary)) {
        (*environment)->DeleteLocalRef(environment, manager_class);
        return CLIPBOARD_JNI_ERROR;
    }

    (*environment)->CallVoidMethod(
        environment, clipboard_manager, set_primary, clip);
    const int failed = take_exception(environment);
    (*environment)->DeleteLocalRef(environment, manager_class);
    return failed ? CLIPBOARD_JNI_ERROR : CLIPBOARD_OK;
}

ClipboardStatus clipboard_set_utf8(const char *bytes, size_t len, int sensitive) {
    if (bytes == NULL && len != 0) {
        return CLIPBOARD_INVALID_ARGUMENT;
    }

    uint16_t *units = NULL;
    size_t unit_count = 0;
    ClipboardStatus status = map_utf_status(
        clipboard_utf8_to_utf16(bytes, len, &units, &unit_count));
    if (status != CLIPBOARD_OK) {
        return status;
    }
    if (unit_count > (size_t)INT_MAX) {
        free(units);
        return CLIPBOARD_OUT_OF_MEMORY;
    }

    AttachedEnvironment attached;
    status = acquire_environment(&attached);
    if (status != CLIPBOARD_OK) {
        free(units);
        return status;
    }
    JNIEnv *environment = attached.environment;

    jstring text = (*environment)->NewString(
        environment, (const jchar *)units, (jsize)unit_count);
    free(units);
    if (object_failed(environment, text)) {
        release_environment(&attached);
        return CLIPBOARD_JNI_ERROR;
    }

    jobject clip = NULL;
    status = make_plain_clip(environment, text, sensitive != 0, &clip);
    if (status == CLIPBOARD_OK) {
        status = set_primary_clip(environment, clip);
    }
    (void)take_exception(environment);
    if (clip != NULL) {
        (*environment)->DeleteLocalRef(environment, clip);
    }
    (*environment)->DeleteLocalRef(environment, text);
    release_environment(&attached);
    return status;
}

ClipboardStatus clipboard_clear(void) {
    AttachedEnvironment attached;
    ClipboardStatus status = acquire_environment(&attached);
    if (status != CLIPBOARD_OK) {
        return status;
    }
    JNIEnv *environment = attached.environment;

    jint api_level = 0;
    status = get_api_level(environment, &api_level);
    if (status != CLIPBOARD_OK) {
        release_environment(&attached);
        return status;
    }

    if (api_level >= 28) {
        jclass manager_class = (*environment)->GetObjectClass(
            environment, clipboard_manager);
        if (object_failed(environment, manager_class)) {
            release_environment(&attached);
            return CLIPBOARD_JNI_ERROR;
        }
        const jmethodID clear_primary = (*environment)->GetMethodID(
            environment, manager_class, "clearPrimaryClip", "()V");
        if (object_failed(environment, (const void *)clear_primary)) {
            (*environment)->DeleteLocalRef(environment, manager_class);
            release_environment(&attached);
            return CLIPBOARD_JNI_ERROR;
        }
        (*environment)->CallVoidMethod(
            environment, clipboard_manager, clear_primary);
        const int failed = take_exception(environment);
        (*environment)->DeleteLocalRef(environment, manager_class);
        release_environment(&attached);
        return failed ? CLIPBOARD_JNI_ERROR : CLIPBOARD_OK;
    }

    const jchar empty_unit = 0;
    jstring empty = (*environment)->NewString(environment, &empty_unit, 0);
    if (object_failed(environment, empty)) {
        release_environment(&attached);
        return CLIPBOARD_JNI_ERROR;
    }
    jobject clip = NULL;
    status = make_plain_clip(environment, empty, 0, &clip);
    if (status == CLIPBOARD_OK) {
        status = set_primary_clip(environment, clip);
    }
    (void)take_exception(environment);
    if (clip != NULL) {
        (*environment)->DeleteLocalRef(environment, clip);
    }
    (*environment)->DeleteLocalRef(environment, empty);
    release_environment(&attached);
    return status;
}

void clipboard_free(void *allocation) {
    free(allocation);
}

const char *clipboard_status_name(ClipboardStatus status) {
    switch (status) {
        case CLIPBOARD_OK:
            return "ok";
        case CLIPBOARD_NO_VISIBLE_CLIP:
            return "no_visible_clip";
        case CLIPBOARD_NO_TEXT:
            return "no_text";
        case CLIPBOARD_NOT_INITIALIZED:
            return "not_initialized";
        case CLIPBOARD_INVALID_ARGUMENT:
            return "invalid_argument";
        case CLIPBOARD_INVALID_UTF8:
            return "invalid_utf8";
        case CLIPBOARD_OUT_OF_MEMORY:
            return "out_of_memory";
        case CLIPBOARD_JNI_ERROR:
            return "jni_error";
        case CLIPBOARD_ALREADY_INITIALIZED:
            return "already_initialized";
    }
    return "unknown";
}
