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

static ClipboardStatus clip_sensitive(JNIEnv *environment, jobject clip,
                                       jclass clip_class, ClipboardDetails *details) {
    details->sensitive ← 0;
    if ((*environment)->PushLocalFrame(environment, 24) != JNI_OK)
        return CLIPBOARD_OUT_OF_MEMORY;
    ClipboardStatus status ← CLIPBOARD_JNI_ERROR;
    const jmethodID get_description ← (*environment)->GetMethodID(environment,
        clip_class, "getDescription", "()Landroid/content/ClipDescription;");
    jobject description ← get_description
        ? (*environment)->CallObjectMethod(environment, clip, get_description) : NULL;
    if (take_exception(environment) || description == NULL) goto details_finished;
    jclass description_class ← (*environment)->GetObjectClass(environment, description);
    jint api_level ← 0;
    if (get_api_level(environment, &api_level) != CLIPBOARD_OK) goto details_finished;
    if (api_level >= 26) {
        const jmethodID timestamp ← description_class ? (*environment)->GetMethodID(environment,
            description_class, "getTimestamp", "()J") : NULL;
        if (timestamp == NULL || take_exception(environment)) goto details_finished;
        details->copied_at_millis ← (*environment)->CallLongMethod(environment, description, timestamp);
        if (take_exception(environment)) goto details_finished;
    }
    const jmethodID get_extras ← description_class ? (*environment)->GetMethodID(environment,
        description_class, "getExtras", "()Landroid/os/PersistableBundle;") : NULL;
    jobject extras ← get_extras
        ? (*environment)->CallObjectMethod(environment, description, get_extras) : NULL;
    if (take_exception(environment) || get_extras == NULL) goto details_finished;
    /* No extras is an ordinary plain-text clip. Sensitive/origin metadata is
     * optional, so its absence cannot reject otherwise valid copied text. */
    if (extras == NULL) {
        status ← CLIPBOARD_OK;
        goto details_finished;
    }
    if (extras != NULL) {
        jclass bundle_class ← (*environment)->GetObjectClass(environment, extras);
        const jmethodID get_boolean ← bundle_class ? (*environment)->GetMethodID(environment,
            bundle_class, "getBoolean", "(Ljava/lang/String;Z)Z") : NULL;
        jstring key ← get_boolean ? new_ascii(environment, "android.content.extra.IS_SENSITIVE") : NULL;
        if (key == NULL || take_exception(environment)) goto details_finished;
        const jboolean indicated ← (*environment)->CallBooleanMethod(environment, extras,
            get_boolean, key, JNI_FALSE);
        if (take_exception(environment)) goto details_finished;
        details->sensitive ← indicated == JNI_TRUE ? 1 : 0;
        const jmethodID get_string ← (*environment)->GetMethodID(environment,
            bundle_class, "getString", "(Ljava/lang/String;)Ljava/lang/String;");
        jstring origin_key ← get_string ? new_ascii(environment, "com.isomorphisms.clipboard.ORIGIN") : NULL;
        if (origin_key == NULL || take_exception(environment)) goto details_finished;
        jstring origin ← (*environment)->CallObjectMethod(environment, extras, get_string, origin_key);
        if (take_exception(environment)) goto details_finished;
        if (origin != NULL) {
            const jsize count ← (*environment)->GetStringLength(environment, origin);
            const jchar *units ← (*environment)->GetStringChars(environment, origin, NULL);
            if (units == NULL || take_exception(environment)) goto details_finished;
            status ← map_utf_status(clipboard_utf16_to_utf8((const uint16_t *)units, (size_t)count,
                &details->origin_utf8, &details->origin_length));
            (*environment)->ReleaseStringChars(environment, origin, units);
            /* An invalid advisory hint cannot discard otherwise valid text. */
            if (status == CLIPBOARD_INVALID_UTF8) {
                free(details->origin_utf8);
                details->origin_utf8 ← NULL;
                details->origin_length ← 0u;
                status ← CLIPBOARD_OK;
            }
            if (status != CLIPBOARD_OK) goto details_finished;
        }
    }
    status ← CLIPBOARD_OK;
details_finished:
    (void)take_exception(environment);
    (*environment)->PopLocalFrame(environment, NULL);
    return status;
}

static ClipboardStatus get_utf8_snapshot(char **out_bytes, size_t *out_len,
                                         ClipboardDetails *details) {
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

    if (details != NULL) {
        details->item_count ← item_count;
        if (item_count != 1) {
            status ← CLIPBOARD_UNSUPPORTED_ITEMS;
            goto cleanup;
        }
        status ← clip_sensitive(environment, clip, clip_class, details);
        if (status != CLIPBOARD_OK) goto cleanup;
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
    if (status != CLIPBOARD_OK && details != NULL) {
        free(details->origin_utf8);
        details->origin_utf8 ← NULL;
        details->origin_length ← 0u;
    }
    return status;
}

ClipboardStatus clipboard_get_utf8(char **out_bytes, size_t *out_len) {
    return get_utf8_snapshot(out_bytes, out_len, NULL);
}

ClipboardStatus clipboard_get_utf8_details(char **out_bytes, size_t *out_len,
                                          ClipboardDetails *out_details) {
    if (out_details == NULL) return CLIPBOARD_INVALID_ARGUMENT;
    out_details->item_count ← 0;
    out_details->sensitive ← 0;
    out_details->origin_utf8 ← NULL;
    out_details->origin_length ← 0u;
    out_details->copied_at_millis ← 0;
    return get_utf8_snapshot(out_bytes, out_len, out_details);
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

static ClipboardStatus attach_origin(JNIEnv *e, jobject clip, jstring origin) {
    if ((*e)->PushLocalFrame(e, 16) != JNI_OK) return CLIPBOARD_OUT_OF_MEMORY;
    ClipboardStatus status ← CLIPBOARD_JNI_ERROR;
    jclass type ← (*e)->GetObjectClass(e, clip);
    jmethodID description ← type ? (*e)->GetMethodID(e, type, "getDescription", "()Landroid/content/ClipDescription;") : NULL;
    jobject detail ← description ? (*e)->CallObjectMethod(e, clip, description) : NULL;
    jclass detail_type ← detail ? (*e)->GetObjectClass(e, detail) : NULL;
    jmethodID get ← detail_type ? (*e)->GetMethodID(e, detail_type, "getExtras", "()Landroid/os/PersistableBundle;") : NULL;
    jobject bundle ← get ? (*e)->CallObjectMethod(e, detail, get) : NULL;
    if (take_exception(e) || detail == NULL || get == NULL) goto origin_finished;
    jclass bundle_type ← (*e)->FindClass(e, "android/os/PersistableBundle");
    jmethodID constructor ← bundle_type ? (*e)->GetMethodID(e, bundle_type, "<init>", "()V") : NULL;
    if (bundle == NULL && constructor) bundle ← (*e)->NewObject(e, bundle_type, constructor);
    jmethodID put ← bundle_type ? (*e)->GetMethodID(e, bundle_type, "putString", "(Ljava/lang/String;Ljava/lang/String;)V") : NULL;
    jmethodID set ← detail_type ? (*e)->GetMethodID(e, detail_type, "setExtras", "(Landroid/os/PersistableBundle;)V") : NULL;
    jstring key ← put ? new_ascii(e, "com.isomorphisms.clipboard.ORIGIN") : NULL;
    if (bundle && key && set && !take_exception(e)) {
        (*e)->CallVoidMethod(e, bundle, put, key, origin);
        if (take_exception(e)) goto origin_finished;
        (*e)->CallVoidMethod(e, detail, set, bundle);
        if (!take_exception(e)) status ← CLIPBOARD_OK;
    }
origin_finished:
    (void)take_exception(e);
    (*e)->PopLocalFrame(e, NULL);
    return status;
}

ClipboardStatus clipboard_set_utf8_with_origin(const char *bytes, size_t len, int sensitive,
                                              const char *origin, size_t origin_length) {
    if ((bytes == NULL && len != 0) || (origin == NULL && origin_length != 0)) {
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
    if (status == CLIPBOARD_OK && origin != NULL) {
        uint16_t *origin_units ← NULL;
        size_t count ← 0u;
        status ← map_utf_status(clipboard_utf8_to_utf16(origin, origin_length, &origin_units, &count));
        if (status == CLIPBOARD_OK && count > (size_t)INT_MAX) status ← CLIPBOARD_OUT_OF_MEMORY;
        jstring value ← status == CLIPBOARD_OK
            ? (*environment)->NewString(environment, (const jchar *)origin_units, (jsize)count) : NULL;
        free(origin_units);
        if (status == CLIPBOARD_OK)
            status ← value && !take_exception(environment) ? attach_origin(environment, clip, value) : CLIPBOARD_JNI_ERROR;
        if (value) (*environment)->DeleteLocalRef(environment, value);
    }
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

ClipboardStatus clipboard_set_utf8(const char *bytes, size_t len, int sensitive) {
    return clipboard_set_utf8_with_origin(bytes, len, sensitive, NULL, 0u);
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
        case CLIPBOARD_UNSUPPORTED_ITEMS:
            return "unsupported_multiple_items";
    }
    return "unknown";
}
