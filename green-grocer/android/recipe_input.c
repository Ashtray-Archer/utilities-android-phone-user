#include "recipe_input.h"
#include "recipe_engine.h"

#include <android/log.h>
#include <pthread.h>
#include <string.h>

static pthread_mutex_t input_mutex = PTHREAD_MUTEX_INITIALIZER;
static struct {
    jobject activity, handler, anchor, native_view, connection;
    jclass view_type, connection_type, request_type;
    jstring target, connection_target;
    bool posted, show;
} input;

static bool exception(JNIEnv *env) {
    if ((*env)->ExceptionCheck(env) == JNI_FALSE) return false;
    __android_log_print(ANDROID_LOG_ERROR, "GGRecipeInput", "Input SDK operation failed");
    (*env)->ExceptionDescribe(env);
    (*env)->ExceptionClear(env);
    return true;
}

static bool same_string(JNIEnv *env, jstring left, jstring right) {
    if (left == NULL || right == NULL) return left == right;
    jsize length = (*env)->GetStringLength(env, left);
    if (length != (*env)->GetStringLength(env, right)) return false;
    const jchar *a = (*env)->GetStringChars(env, left, NULL);
    const jchar *b = (*env)->GetStringChars(env, right, NULL);
    bool same = a != NULL && b != NULL &&
        memcmp(a, b, (size_t)length * sizeof(*a)) == 0;
    if (a != NULL) (*env)->ReleaseStringChars(env, left, a);
    if (b != NULL) (*env)->ReleaseStringChars(env, right, b);
    return !exception(env) && same;
}

static void invalidate_connection(JNIEnv *env) {
    if (input.connection != NULL) (*env)->DeleteGlobalRef(env, input.connection);
    if (input.connection_target != NULL) (*env)->DeleteGlobalRef(env, input.connection_target);
    input.connection = NULL;
    input.connection_target = NULL;
}

static jclass app_class(JNIEnv *env, jobject loader, jmethodID load, const char *name) {
    jstring text = (*env)->NewStringUTF(env, name);
    return text == NULL ? NULL : (*env)->CallObjectMethod(env, loader, load, text);
}

bool gg_recipe_input_open(ANativeActivity *activity, JNIEnv *env) {
    bool success = false;
    if ((*env)->PushLocalFrame(env, 32) != JNI_OK) return false;
    jclass activity_type = (*env)->GetObjectClass(env, activity->clazz);
    jclass loader_type = (*env)->FindClass(env, "java/lang/ClassLoader");
    jclass looper_type = (*env)->FindClass(env, "android/os/Looper");
    jclass handler_type = (*env)->FindClass(env, "android/os/Handler");
    jclass version_type = (*env)->FindClass(env, "android/os/Build$VERSION");
    if (exception(env) || activity_type == NULL || loader_type == NULL || looper_type == NULL ||
        handler_type == NULL || version_type == NULL) goto done;
    jmethodID get_loader = (*env)->GetMethodID(env, activity_type, "getClassLoader", "()Ljava/lang/ClassLoader;");
    jmethodID load = (*env)->GetMethodID(env, loader_type, "loadClass", "(Ljava/lang/String;)Ljava/lang/Class;");
    jmethodID main_looper = (*env)->GetStaticMethodID(env, looper_type, "getMainLooper", "()Landroid/os/Looper;");
    jmethodID handler_constructor = (*env)->GetMethodID(env, handler_type, "<init>", "(Landroid/os/Looper;)V");
    jfieldID sdk_field = (*env)->GetStaticFieldID(env, version_type, "SDK_INT", "I");
    if (exception(env) || get_loader == NULL || load == NULL || main_looper == NULL ||
        handler_constructor == NULL || sdk_field == NULL) goto done;
    jobject loader = (*env)->CallObjectMethod(env, activity->clazz, get_loader);
    jobject looper = (*env)->CallStaticObjectMethod(env, looper_type, main_looper);
    if (exception(env) || loader == NULL || looper == NULL) goto done;
    jclass view_type = app_class(env, loader, load, "org.isomorphisms.greengrocer.RecipeInputView");
    jint api = (*env)->GetStaticIntField(env, version_type, sdk_field);
    jclass connection_type = app_class(env, loader, load, api >= 33 ?
        "org.isomorphisms.greengrocer.RecipeModernInputConnection" :
        "org.isomorphisms.greengrocer.RecipeInputConnection");
    jclass request_type = app_class(env, loader, load, "org.isomorphisms.greengrocer.RecipeInputRequest");
    jobject handler = (*env)->NewObject(env, handler_type, handler_constructor, looper);
    if (exception(env) || view_type == NULL || connection_type == NULL || request_type == NULL || handler == NULL) goto done;
    (void)pthread_mutex_lock(&input_mutex);
    input.activity = (*env)->NewGlobalRef(env, activity->clazz);
    input.handler = (*env)->NewGlobalRef(env, handler);
    input.view_type = (*env)->NewGlobalRef(env, view_type);
    input.connection_type = (*env)->NewGlobalRef(env, connection_type);
    input.request_type = (*env)->NewGlobalRef(env, request_type);
    success = input.activity != NULL && input.handler != NULL && input.view_type != NULL &&
        input.connection_type != NULL && input.request_type != NULL && !exception(env);
    (void)pthread_mutex_unlock(&input_mutex);
done:
    (*env)->PopLocalFrame(env, NULL);
    if (!success) gg_recipe_input_close(env);
    return success;
}

void gg_recipe_input_close(JNIEnv *env) {
    (void)pthread_mutex_lock(&input_mutex);
    invalidate_connection(env);
    jobject references[] = {input.activity, input.handler, input.anchor, input.native_view,
        input.view_type, input.connection_type, input.request_type, input.target};
    for (size_t index = 0; index < sizeof(references) / sizeof(references[0]); ++index)
        if (references[index] != NULL) (*env)->DeleteGlobalRef(env, references[index]);
    memset(&input, 0, sizeof(input));
    (void)pthread_mutex_unlock(&input_mutex);
}

bool gg_recipe_input_refresh(JNIEnv *env, bool show_keyboard) {
    bool success = false;
    if ((*env)->PushLocalFrame(env, 12) != JNI_OK) return false;
    (void)pthread_mutex_lock(&input_mutex);
    input.show = input.show || show_keyboard;
    if (input.posted) { success = true; (void)pthread_mutex_unlock(&input_mutex); goto done; }
    jobject handler = input.handler == NULL ? NULL : (*env)->NewLocalRef(env, input.handler);
    jclass request_type = input.request_type == NULL ? NULL : (*env)->NewLocalRef(env, input.request_type);
    input.posted = handler != NULL && request_type != NULL;
    (void)pthread_mutex_unlock(&input_mutex);
    if (handler == NULL || request_type == NULL || exception(env)) goto failed;
    jclass handler_type = (*env)->GetObjectClass(env, handler);
    jmethodID constructor = (*env)->GetMethodID(env, request_type, "<init>", "()V");
    jmethodID post = (*env)->GetMethodID(env, handler_type, "post", "(Ljava/lang/Runnable;)Z");
    if (exception(env) || constructor == NULL || post == NULL) goto failed;
    jobject request = (*env)->NewObject(env, request_type, constructor);
    if (exception(env) || request == NULL) goto failed;
    success = (*env)->CallBooleanMethod(env, handler, post, request) == JNI_TRUE && !exception(env);
failed:
    if (!success) {
        (void)pthread_mutex_lock(&input_mutex);
        input.posted = false;
        (void)pthread_mutex_unlock(&input_mutex);
    }
done:
    (*env)->PopLocalFrame(env, NULL);
    return success;
}

static jobject input_manager(JNIEnv *env, jobject activity) {
    jclass type = (*env)->GetObjectClass(env, activity);
    jmethodID get_service = (*env)->GetMethodID(env, type, "getSystemService", "(Ljava/lang/String;)Ljava/lang/Object;");
    jstring name = (*env)->NewStringUTF(env, "input_method");
    return exception(env) || get_service == NULL || name == NULL ? NULL :
        (*env)->CallObjectMethod(env, activity, get_service, name);
}

static jobject create_anchor(JNIEnv *env, jobject activity, jclass view_type) {
    jclass activity_type = (*env)->GetObjectClass(env, activity);
    jclass view_base = (*env)->FindClass(env, "android/view/View");
    jclass group_type = (*env)->FindClass(env, "android/view/ViewGroup");
    jclass layout_type = (*env)->FindClass(env, "android/view/ViewGroup$LayoutParams");
    if (exception(env) || activity_type == NULL || view_base == NULL || group_type == NULL || layout_type == NULL) return NULL;
    jmethodID constructor = (*env)->GetMethodID(env, view_type, "<init>", "(Landroid/content/Context;)V");
    jmethodID layout_constructor = (*env)->GetMethodID(env, layout_type, "<init>", "(II)V");
    jmethodID focusable = (*env)->GetMethodID(env, view_base, "setFocusable", "(Z)V");
    jmethodID touch_focusable = (*env)->GetMethodID(env, view_base, "setFocusableInTouchMode", "(Z)V");
    jmethodID accessibility = (*env)->GetMethodID(env, view_base, "setImportantForAccessibility", "(I)V");
    jmethodID find = (*env)->GetMethodID(env, activity_type, "findViewById", "(I)Landroid/view/View;");
    jmethodID child = (*env)->GetMethodID(env, group_type, "getChildAt", "(I)Landroid/view/View;");
    jmethodID add = (*env)->GetMethodID(env, activity_type, "addContentView", "(Landroid/view/View;Landroid/view/ViewGroup$LayoutParams;)V");
    if (exception(env) || constructor == NULL || layout_constructor == NULL || focusable == NULL ||
        touch_focusable == NULL || accessibility == NULL || find == NULL || child == NULL || add == NULL) return NULL;
    jobject root = (*env)->CallObjectMethod(env, activity, find, 16908290); /* android.R.id.content */
    jobject native_view = root == NULL ? NULL : (*env)->CallObjectMethod(env, root, child, 0);
    jobject anchor = (*env)->NewObject(env, view_type, constructor, activity);
    jobject layout = (*env)->NewObject(env, layout_type, layout_constructor, 1, 1);
    if (exception(env) || anchor == NULL || layout == NULL || native_view == NULL) return NULL;
    (*env)->CallVoidMethod(env, anchor, focusable, JNI_TRUE);
    (*env)->CallVoidMethod(env, anchor, touch_focusable, JNI_TRUE);
    (*env)->CallVoidMethod(env, anchor, accessibility, 2);
    (*env)->CallVoidMethod(env, activity, add, anchor, layout);
    if (exception(env)) return NULL;
    (void)pthread_mutex_lock(&input_mutex);
    if (input.activity != NULL && (*env)->IsSameObject(env, input.activity, activity)) {
        input.anchor = (*env)->NewGlobalRef(env, anchor);
        input.native_view = (*env)->NewGlobalRef(env, native_view);
    }
    (void)pthread_mutex_unlock(&input_mutex);
    return anchor;
}

JNIEXPORT void JNICALL
Java_org_isomorphisms_greengrocer_RecipeInputRequest_run(JNIEnv *env, jobject receiver) {
    (void)receiver;
    if ((*env)->PushLocalFrame(env, 48) != JNI_OK) return;
    (void)pthread_mutex_lock(&input_mutex);
    jobject activity = input.activity == NULL ? NULL : (*env)->NewLocalRef(env, input.activity);
    jobject anchor = input.anchor == NULL ? NULL : (*env)->NewLocalRef(env, input.anchor);
    jobject native_view = input.native_view == NULL ? NULL : (*env)->NewLocalRef(env, input.native_view);
    jclass view_type = input.view_type == NULL ? NULL : (*env)->NewLocalRef(env, input.view_type);
    bool show = input.show;
    input.show = false;
    input.posted = false;
    (void)pthread_mutex_unlock(&input_mutex);
    if (activity == NULL || view_type == NULL) goto done;
    gg_recipe_input_snapshot snapshot = {0};
    if (!gg_recipe_engine_snapshot(env, &snapshot)) goto done;
    bool editing = (*env)->GetStringLength(env, snapshot.target) != 0;
    (void)pthread_mutex_lock(&input_mutex);
    bool changed = !same_string(env, input.target, snapshot.target);
    if (changed) {
        invalidate_connection(env);
        if (input.target != NULL) (*env)->DeleteGlobalRef(env, input.target);
        input.target = (*env)->NewGlobalRef(env, snapshot.target);
    }
    (void)pthread_mutex_unlock(&input_mutex);
    if (!editing && anchor == NULL) goto done;
    if (anchor == NULL) anchor = create_anchor(env, activity, view_type);
    if (anchor == NULL) goto done;
    jobject manager = input_manager(env, activity);
    jclass manager_type = (*env)->FindClass(env, "android/view/inputmethod/InputMethodManager");
    jclass view_base = (*env)->FindClass(env, "android/view/View");
    if (exception(env) || manager == NULL || manager_type == NULL || view_base == NULL) goto done;
    jmethodID focus = (*env)->GetMethodID(env, view_base, "requestFocus", "()Z");
    jmethodID restart = (*env)->GetMethodID(env, manager_type, "restartInput", "(Landroid/view/View;)V");
    jmethodID open = (*env)->GetMethodID(env, manager_type, "showSoftInput", "(Landroid/view/View;I)Z");
    jmethodID update = (*env)->GetMethodID(env, manager_type, "updateSelection", "(Landroid/view/View;IIII)V");
    jmethodID hide = (*env)->GetMethodID(env, manager_type, "hideSoftInputFromWindow", "(Landroid/os/IBinder;I)Z");
    jmethodID window = (*env)->GetMethodID(env, view_base, "getWindowToken", "()Landroid/os/IBinder;");
    if (exception(env) || focus == NULL || restart == NULL || open == NULL || update == NULL || hide == NULL || window == NULL) goto done;
    if (editing) {
        if (changed || show) {
            (void)(*env)->CallBooleanMethod(env, anchor, focus);
            (*env)->CallVoidMethod(env, manager, restart, anchor);
            (void)(*env)->CallBooleanMethod(env, manager, open, anchor, 1);
        }
        (*env)->CallVoidMethod(env, manager, update, anchor, snapshot.first, snapshot.last,
            snapshot.composing_first, snapshot.composing_last);
    } else if (changed) {
        jobject token = (*env)->CallObjectMethod(env, anchor, window);
        if (token != NULL) (void)(*env)->CallBooleanMethod(env, manager, hide, token, 0);
        if (native_view != NULL) (void)(*env)->CallBooleanMethod(env, native_view, focus);
    }
    (void)exception(env);
done:
    (*env)->PopLocalFrame(env, NULL);
}

static jstring connection_target(JNIEnv *env, jobject receiver) {
    (void)pthread_mutex_lock(&input_mutex);
    jstring target = input.connection != NULL && (*env)->IsSameObject(env, receiver, input.connection) &&
        input.connection_target != NULL ? (*env)->NewLocalRef(env, input.connection_target) : NULL;
    (void)pthread_mutex_unlock(&input_mutex);
    return target;
}

static bool connection_snapshot(JNIEnv *env, jobject receiver, gg_recipe_input_snapshot *snapshot) {
    jstring expected = connection_target(env, receiver);
    return expected != NULL && gg_recipe_engine_snapshot(env, snapshot) &&
        same_string(env, expected, snapshot->target) && (*env)->GetStringLength(env, expected) != 0;
}

static bool command(JNIEnv *env, jobject receiver, const char *operation, jstring text, jint first, jint last) {
    jstring expected = connection_target(env, receiver);
    bool accepted = expected != NULL && gg_recipe_engine_input(env, expected, operation, text, first, last);
    if (accepted) (void)gg_recipe_input_refresh(env, false);
    return accepted;
}

static jstring as_text(JNIEnv *env, jobject sequence) {
    if (sequence == NULL) return NULL;
    jclass type = (*env)->FindClass(env, "java/lang/CharSequence");
    if (exception(env) || type == NULL) return NULL;
    jmethodID to_string = (*env)->GetMethodID(env, type, "toString", "()Ljava/lang/String;");
    return exception(env) || to_string == NULL ? NULL : (*env)->CallObjectMethod(env, sequence, to_string);
}

JNIEXPORT jboolean JNICALL
Java_org_isomorphisms_greengrocer_RecipeInputView_onCheckIsTextEditor(JNIEnv *env, jobject receiver) {
    (void)receiver;
    gg_recipe_input_snapshot snapshot = {0};
    return gg_recipe_engine_snapshot(env, &snapshot) &&
        (*env)->GetStringLength(env, snapshot.target) != 0 ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jobject JNICALL
Java_org_isomorphisms_greengrocer_RecipeInputView_onCreateInputConnection(JNIEnv *env, jobject receiver, jobject info) {
    gg_recipe_input_snapshot snapshot = {0};
    if (info == NULL || !gg_recipe_engine_snapshot(env, &snapshot) ||
        (*env)->GetStringLength(env, snapshot.target) == 0) return NULL;
    (void)pthread_mutex_lock(&input_mutex);
    bool active = input.anchor != NULL && (*env)->IsSameObject(env, receiver, input.anchor);
    jclass type = active ? (*env)->NewLocalRef(env, input.connection_type) : NULL;
    (void)pthread_mutex_unlock(&input_mutex);
    if (type == NULL) return NULL;
    jclass editor_type = (*env)->FindClass(env, "android/view/inputmethod/EditorInfo");
    if (exception(env) || editor_type == NULL) return NULL;
    jfieldID input_type = (*env)->GetFieldID(env, editor_type, "inputType", "I");
    jfieldID ime_options = (*env)->GetFieldID(env, editor_type, "imeOptions", "I");
    jfieldID selection_start = (*env)->GetFieldID(env, editor_type, "initialSelStart", "I");
    jfieldID selection_end = (*env)->GetFieldID(env, editor_type, "initialSelEnd", "I");
    jmethodID constructor = (*env)->GetMethodID(env, type, "<init>", "(Landroid/view/View;Z)V");
    if (exception(env) || input_type == NULL || ime_options == NULL || selection_start == NULL ||
        selection_end == NULL || constructor == NULL) return NULL;
    (*env)->SetIntField(env, info, input_type, 0x20001); /* text, multi-line */
    (*env)->SetIntField(env, info, ime_options, 0x10000006); /* Done, no extracted form */
    (*env)->SetIntField(env, info, selection_start, snapshot.first);
    (*env)->SetIntField(env, info, selection_end, snapshot.last);
    jobject connection = (*env)->NewObject(env, type, constructor, receiver, JNI_TRUE);
    if (exception(env) || connection == NULL) return NULL;
    (void)pthread_mutex_lock(&input_mutex);
    invalidate_connection(env);
    input.connection = (*env)->NewGlobalRef(env, connection);
    input.connection_target = (*env)->NewGlobalRef(env, snapshot.target);
    (void)pthread_mutex_unlock(&input_mutex);
#ifdef GG_RECIPE_OBSERVE
    __android_log_print(ANDROID_LOG_INFO, "GGRecipeInput", "connected utf16=%d", (*env)->GetStringLength(env, snapshot.text));
#endif
    return connection;
}

static jobject editable(JNIEnv *env, jobject receiver) {
    gg_recipe_input_snapshot snapshot = {0};
    if (!connection_snapshot(env, receiver, &snapshot)) return NULL;
    jclass builder = (*env)->FindClass(env, "android/text/SpannableStringBuilder");
    jclass selection = (*env)->FindClass(env, "android/text/Selection");
    if (exception(env) || builder == NULL || selection == NULL) return NULL;
    jmethodID constructor = (*env)->GetMethodID(env, builder, "<init>", "(Ljava/lang/CharSequence;)V");
    jmethodID select = (*env)->GetStaticMethodID(env, selection, "setSelection", "(Landroid/text/Spannable;II)V");
    if (exception(env) || constructor == NULL || select == NULL) return NULL;
    jobject result = (*env)->NewObject(env, builder, constructor, snapshot.text);
    if (result != NULL) (*env)->CallStaticVoidMethod(env, selection, select, result, snapshot.first, snapshot.last);
    return exception(env) ? NULL : result;
}

static jstring clipboard_read(JNIEnv *env, jobject activity) {
    jclass activity_type = (*env)->GetObjectClass(env, activity);
    jclass manager_type = (*env)->FindClass(env, "android/content/ClipboardManager");
    jclass clip_type = (*env)->FindClass(env, "android/content/ClipData");
    jclass item_type = (*env)->FindClass(env, "android/content/ClipData$Item");
    if (exception(env) || activity_type == NULL || manager_type == NULL || clip_type == NULL || item_type == NULL) return NULL;
    jmethodID service = (*env)->GetMethodID(env, activity_type, "getSystemService", "(Ljava/lang/String;)Ljava/lang/Object;");
    jmethodID primary = (*env)->GetMethodID(env, manager_type, "getPrimaryClip", "()Landroid/content/ClipData;");
    jmethodID count = (*env)->GetMethodID(env, clip_type, "getItemCount", "()I");
    jmethodID get = (*env)->GetMethodID(env, clip_type, "getItemAt", "(I)Landroid/content/ClipData$Item;");
    jmethodID coerce = (*env)->GetMethodID(env, item_type, "coerceToText", "(Landroid/content/Context;)Ljava/lang/CharSequence;");
    if (exception(env) || service == NULL || primary == NULL || count == NULL || get == NULL || coerce == NULL) return NULL;
    jstring name = (*env)->NewStringUTF(env, "clipboard");
    jobject manager = name == NULL ? NULL : (*env)->CallObjectMethod(env, activity, service, name);
    jobject clip = manager == NULL ? NULL : (*env)->CallObjectMethod(env, manager, primary);
    if (exception(env) || clip == NULL || (*env)->CallIntMethod(env, clip, count) <= 0) return NULL;
    jobject item = (*env)->CallObjectMethod(env, clip, get, 0);
    jobject text = item == NULL ? NULL : (*env)->CallObjectMethod(env, item, coerce, activity);
    return exception(env) ? NULL : as_text(env, text);
}

static bool clipboard_write(JNIEnv *env, jobject activity, jstring text) {
    jclass activity_type = (*env)->GetObjectClass(env, activity);
    jclass manager_type = (*env)->FindClass(env, "android/content/ClipboardManager");
    jclass clip_type = (*env)->FindClass(env, "android/content/ClipData");
    if (exception(env) || activity_type == NULL || manager_type == NULL || clip_type == NULL) return false;
    jmethodID service = (*env)->GetMethodID(env, activity_type, "getSystemService", "(Ljava/lang/String;)Ljava/lang/Object;");
    jmethodID make = (*env)->GetStaticMethodID(env, clip_type, "newPlainText", "(Ljava/lang/CharSequence;Ljava/lang/CharSequence;)Landroid/content/ClipData;");
    jmethodID set = (*env)->GetMethodID(env, manager_type, "setPrimaryClip", "(Landroid/content/ClipData;)V");
    if (exception(env) || service == NULL || make == NULL || set == NULL) return false;
    jstring name = (*env)->NewStringUTF(env, "clipboard");
    jstring label = (*env)->NewStringUTF(env, "Recipe text");
    jobject manager = name == NULL ? NULL : (*env)->CallObjectMethod(env, activity, service, name);
    jobject clip = label == NULL ? NULL : (*env)->CallStaticObjectMethod(env, clip_type, make, label, text);
    if (exception(env) || manager == NULL || clip == NULL) return false;
    (*env)->CallVoidMethod(env, manager, set, clip);
    return !exception(env);
}

static bool context_action(JNIEnv *env, jobject receiver, jint action) {
    gg_recipe_input_snapshot snapshot = {0};
    if (!connection_snapshot(env, receiver, &snapshot)) return false;
    (void)pthread_mutex_lock(&input_mutex);
    jobject activity = input.activity == NULL ? NULL : (*env)->NewLocalRef(env, input.activity);
    (void)pthread_mutex_unlock(&input_mutex);
    if (activity == NULL) return false;
    if (action == 16908319) return command(env, receiver, "select-all", NULL, 0, 0);
    if (action == 16908322 || action == 16908337) {
        jstring text = clipboard_read(env, activity);
        return text != NULL && command(env, receiver, "commit", text, 1, 0);
    }
    if (action != 16908320 && action != 16908321) return false;
    jint first = snapshot.first < snapshot.last ? snapshot.first : snapshot.last;
    jint last = snapshot.first < snapshot.last ? snapshot.last : snapshot.first;
    if (first == last) return false;
    const jchar *text = (*env)->GetStringChars(env, snapshot.text, NULL);
    if (text == NULL) return false;
    jstring selected = (*env)->NewString(env, text + first, last - first);
    (*env)->ReleaseStringChars(env, snapshot.text, text);
    if (selected == NULL || !clipboard_write(env, activity, selected)) return false;
    return action == 16908321 || command(env, receiver, "cut", selected, first, last);
}

bool gg_recipe_input_paste(JNIEnv *env) {
    if ((*env)->PushLocalFrame(env, 32) != JNI_OK) return false;
    (void)pthread_mutex_lock(&input_mutex);
    jobject activity = input.activity == NULL ? NULL : (*env)->NewLocalRef(env, input.activity);
    (void)pthread_mutex_unlock(&input_mutex);
    gg_recipe_input_snapshot snapshot = {0};
    bool success = false;
    if (activity != NULL && gg_recipe_engine_snapshot(env, &snapshot) &&
        (*env)->GetStringLength(env, snapshot.target) != 0) {
        jstring text = clipboard_read(env, activity);
        success = text != NULL && gg_recipe_engine_input(env, snapshot.target, "commit", text, 1, 0);
        if (success) (void)gg_recipe_input_refresh(env, false);
    }
    (*env)->PopLocalFrame(env, NULL);
    return success;
}

static bool key_event(JNIEnv *env, jobject receiver, jobject event) {
    if (event == NULL) return false;
    gg_recipe_input_snapshot snapshot = {0};
    if (!connection_snapshot(env, receiver, &snapshot)) return false;
    jclass type = (*env)->FindClass(env, "android/view/KeyEvent");
    if (exception(env) || type == NULL) return false;
    jmethodID get_action = (*env)->GetMethodID(env, type, "getAction", "()I");
    jmethodID get_code = (*env)->GetMethodID(env, type, "getKeyCode", "()I");
    jmethodID get_meta = (*env)->GetMethodID(env, type, "getMetaState", "()I");
    jmethodID unicode = (*env)->GetMethodID(env, type, "getUnicodeChar", "(I)I");
    jmethodID characters = (*env)->GetMethodID(env, type, "getCharacters", "()Ljava/lang/String;");
    if (exception(env) || get_action == NULL || get_code == NULL || get_meta == NULL || unicode == NULL || characters == NULL) return false;
    jint action = (*env)->CallIntMethod(env, event, get_action);
    jint code = (*env)->CallIntMethod(env, event, get_code);
    jint meta = (*env)->CallIntMethod(env, event, get_meta);
    if (exception(env)) return false;
    if (action == 1) return code != 4;
    if (action == 2) {
        jstring text = (*env)->CallObjectMethod(env, event, characters);
        return !exception(env) && text != NULL && command(env, receiver, "commit", text, 1, 0);
    }
    if (action != 0 || code == 4) return false;
    if ((meta & 0x1000) != 0) {
        if (code == 29) return context_action(env, receiver, 16908319);
        if (code == 31) return context_action(env, receiver, 16908321);
        if (code == 50) return context_action(env, receiver, 16908322);
        if (code == 52) return context_action(env, receiver, 16908320);
    }
    const char *operation = NULL;
    if (code == 67) operation = "backspace";
    else if (code == 112) operation = "delete";
    else if (code == 21) operation = "left";
    else if (code == 22) operation = "right";
    else if (code == 122) operation = "home";
    else if (code == 123) operation = "end";
    if (operation != NULL) return command(env, receiver, operation, NULL, 0, 0);
    jint point = (*env)->CallIntMethod(env, event, unicode, meta);
    if (exception(env) || point <= 0 || point > 0x10FFFF || (point >= 0xD800 && point <= 0xDFFF)) return false;
    jchar characters16[2];
    jsize length = 1;
    if (point <= 0xFFFF) characters16[0] = (jchar)point;
    else {
        characters16[0] = (jchar)(0xD800 + ((point - 0x10000) >> 10));
        characters16[1] = (jchar)(0xDC00 + ((point - 0x10000) & 1023));
        length = 2;
    }
    jstring text = (*env)->NewString(env, characters16, length);
    return text != NULL && command(env, receiver, "commit", text, 1, 0);
}

bool gg_recipe_input_key(JNIEnv *env, const AInputEvent *event) {
    if (AInputEvent_getType(event) != AINPUT_EVENT_TYPE_KEY || (*env)->PushLocalFrame(env, 32) != JNI_OK) return false;
    (void)pthread_mutex_lock(&input_mutex);
    jobject receiver = input.connection == NULL ? NULL : (*env)->NewLocalRef(env, input.connection);
    (void)pthread_mutex_unlock(&input_mutex);
    bool handled = false;
    if (receiver != NULL) {
        jclass type = (*env)->FindClass(env, "android/view/KeyEvent");
        jmethodID constructor = type == NULL ? NULL : (*env)->GetMethodID(env, type, "<init>", "(JJIIII)V");
        if (!exception(env) && constructor != NULL) {
            jobject key = (*env)->NewObject(env, type, constructor,
                (jlong)(AKeyEvent_getDownTime(event) / 1000000), (jlong)(AKeyEvent_getEventTime(event) / 1000000),
                AKeyEvent_getAction(event), AKeyEvent_getKeyCode(event), AKeyEvent_getRepeatCount(event), AKeyEvent_getMetaState(event));
            handled = key != NULL && key_event(env, receiver, key);
        }
    }
    (*env)->PopLocalFrame(env, NULL);
    return handled;
}

JNIEXPORT jboolean JNICALL
Java_org_isomorphisms_greengrocer_RecipeInputView_onKeyDown(JNIEnv *env, jobject receiver, jint code, jobject event) {
    (void)receiver; (void)code;
    (void)pthread_mutex_lock(&input_mutex);
    jobject connection = input.connection == NULL ? NULL : (*env)->NewLocalRef(env, input.connection);
    (void)pthread_mutex_unlock(&input_mutex);
    return connection != NULL && key_event(env, connection, event) ? JNI_TRUE : JNI_FALSE;
}
JNIEXPORT jboolean JNICALL
Java_org_isomorphisms_greengrocer_RecipeInputView_onKeyUp(JNIEnv *env, jobject receiver, jint code, jobject event) {
    return Java_org_isomorphisms_greengrocer_RecipeInputView_onKeyDown(env, receiver, code, event);
}

#define GG_CALLBACKS(PREFIX) \
JNIEXPORT jobject JNICALL PREFIX##_getEditable(JNIEnv *env, jobject self) { return editable(env, self); } \
JNIEXPORT jboolean JNICALL PREFIX##_finishComposingText(JNIEnv *env, jobject self) { return command(env, self, "finish", NULL, 0, 0); } \
JNIEXPORT jboolean JNICALL PREFIX##_setSelection(JNIEnv *env, jobject self, jint a, jint b) { return command(env, self, "select", NULL, a, b); } \
JNIEXPORT jboolean JNICALL PREFIX##_deleteSurroundingText(JNIEnv *env, jobject self, jint a, jint b) { return command(env, self, "delete-units", NULL, a, b); } \
JNIEXPORT jboolean JNICALL PREFIX##_deleteSurroundingTextInCodePoints(JNIEnv *env, jobject self, jint a, jint b) { return command(env, self, "delete-characters", NULL, a, b); } \
JNIEXPORT jboolean JNICALL PREFIX##_sendKeyEvent(JNIEnv *env, jobject self, jobject event) { return key_event(env, self, event); } \
JNIEXPORT jboolean JNICALL PREFIX##_performEditorAction(JNIEnv *env, jobject self, jint action) { return action == 6 && command(env, self, "done", NULL, 0, 0); } \
JNIEXPORT jboolean JNICALL PREFIX##_performContextMenuAction(JNIEnv *env, jobject self, jint action) { return context_action(env, self, action); } \
JNIEXPORT jboolean JNICALL PREFIX##_beginBatchEdit(JNIEnv *env, jobject self) { gg_recipe_input_snapshot snapshot = {0}; return connection_snapshot(env, self, &snapshot); } \
JNIEXPORT jboolean JNICALL PREFIX##_endBatchEdit(JNIEnv *env, jobject self) { gg_recipe_input_snapshot snapshot = {0}; return connection_snapshot(env, self, &snapshot); } \
JNIEXPORT void JNICALL PREFIX##_closeConnection(JNIEnv *env, jobject self) { \
    (void)command(env, self, "finish", NULL, 0, 0); \
    (void)pthread_mutex_lock(&input_mutex); \
    if (input.connection != NULL && (*env)->IsSameObject(env, self, input.connection)) invalidate_connection(env); \
    (void)pthread_mutex_unlock(&input_mutex); \
}

GG_CALLBACKS(Java_org_isomorphisms_greengrocer_RecipeInputConnection)
GG_CALLBACKS(Java_org_isomorphisms_greengrocer_RecipeModernInputConnection)

#define GG_TEXT_CALLBACK(NAME, OPERATION) \
JNIEXPORT jboolean JNICALL NAME(JNIEnv *env, jobject self, jobject sequence, jint cursor) { \
    jstring text = as_text(env, sequence); \
    return text != NULL && !exception(env) && command(env, self, OPERATION, text, cursor, 0); \
}
GG_TEXT_CALLBACK(Java_org_isomorphisms_greengrocer_RecipeInputConnection_commitText, "commit")
GG_TEXT_CALLBACK(Java_org_isomorphisms_greengrocer_RecipeInputConnection_setComposingText, "compose")
GG_TEXT_CALLBACK(Java_org_isomorphisms_greengrocer_RecipeModernInputConnection_commitText__Ljava_lang_CharSequence_2I, "commit")
GG_TEXT_CALLBACK(Java_org_isomorphisms_greengrocer_RecipeModernInputConnection_setComposingText__Ljava_lang_CharSequence_2I, "compose")

#define GG_MODERN_TEXT(NAME, OPERATION) \
JNIEXPORT jboolean JNICALL NAME(JNIEnv *env, jobject self, jobject sequence, jint cursor, jobject attributes) { \
    (void)attributes; jstring text = as_text(env, sequence); \
    return text != NULL && !exception(env) && command(env, self, OPERATION, text, cursor, 0); \
}
GG_MODERN_TEXT(Java_org_isomorphisms_greengrocer_RecipeModernInputConnection_commitText__Ljava_lang_CharSequence_2ILandroid_view_inputmethod_TextAttribute_2, "commit")
GG_MODERN_TEXT(Java_org_isomorphisms_greengrocer_RecipeModernInputConnection_setComposingText__Ljava_lang_CharSequence_2ILandroid_view_inputmethod_TextAttribute_2, "compose")

JNIEXPORT jboolean JNICALL
Java_org_isomorphisms_greengrocer_RecipeInputConnection_setComposingRegion(JNIEnv *env, jobject self, jint a, jint b) {
    return command(env, self, "region", NULL, a, b);
}
JNIEXPORT jboolean JNICALL
Java_org_isomorphisms_greengrocer_RecipeModernInputConnection_setComposingRegion__II(JNIEnv *env, jobject self, jint a, jint b) {
    return command(env, self, "region", NULL, a, b);
}
JNIEXPORT jboolean JNICALL
Java_org_isomorphisms_greengrocer_RecipeModernInputConnection_setComposingRegion__IILandroid_view_inputmethod_TextAttribute_2(JNIEnv *env, jobject self, jint a, jint b, jobject attributes) {
    (void)attributes;
    return command(env, self, "region", NULL, a, b);
}
JNIEXPORT jboolean JNICALL
Java_org_isomorphisms_greengrocer_RecipeModernInputConnection_replaceText(JNIEnv *env, jobject self,
    jint a, jint b, jobject sequence, jint cursor, jobject attributes) {
    (void)attributes;
    jstring text = as_text(env, sequence);
    jstring expected = connection_target(env, self);
    bool accepted = text != NULL && expected != NULL && !exception(env) &&
        gg_recipe_engine_replace(env, expected, text, a, b, cursor);
    if (accepted) (void)gg_recipe_input_refresh(env, false);
    return accepted;
}
