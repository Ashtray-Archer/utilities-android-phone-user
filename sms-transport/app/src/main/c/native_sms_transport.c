#include <android/log.h>
#include <android/native_activity.h>
#include <jni.h>

#include <stddef.h>

#define LOG_TAG "SmsTransport"

static int jni_exception(JNIEnv *environment, const char *operation)
{
    if ((*environment)->ExceptionCheck(environment) == JNI_FALSE) {
        return 0;
    }

    (*environment)->ExceptionClear(environment);
    __android_log_print(
        ANDROID_LOG_ERROR,
        LOG_TAG,
        "FAIL %s raised an Android exception",
        operation);
    return 1;
}

static jobject activity_intent(
    JNIEnv *environment,
    ANativeActivity *activity)
{
    jclass activity_class =
        (*environment)->GetObjectClass(environment, activity->clazz);
    if (activity_class == NULL || jni_exception(environment, "GetObjectClass")) {
        return NULL;
    }

    jmethodID get_intent = (*environment)->GetMethodID(
        environment,
        activity_class,
        "getIntent",
        "()Landroid/content/Intent;");
    if (get_intent == NULL || jni_exception(environment, "getIntent lookup")) {
        return NULL;
    }

    jobject intent = (*environment)->CallObjectMethod(
        environment,
        activity->clazz,
        get_intent);
    if (jni_exception(environment, "getIntent")) {
        return NULL;
    }

    return intent;
}

static jstring intent_string_extra(
    JNIEnv *environment,
    jobject intent,
    const char *name)
{
    jclass intent_class = (*environment)->GetObjectClass(environment, intent);
    if (intent_class == NULL || jni_exception(environment, "Intent class lookup")) {
        return NULL;
    }

    jmethodID get_string_extra = (*environment)->GetMethodID(
        environment,
        intent_class,
        "getStringExtra",
        "(Ljava/lang/String;)Ljava/lang/String;");
    if (get_string_extra == NULL ||
        jni_exception(environment, "getStringExtra lookup")) {
        return NULL;
    }

    jstring key = (*environment)->NewStringUTF(environment, name);
    if (key == NULL || jni_exception(environment, "intent-extra key creation")) {
        return NULL;
    }

    jstring value = (jstring)(*environment)->CallObjectMethod(
        environment,
        intent,
        get_string_extra,
        key);
    (void)(*environment)->DeleteLocalRef(environment, key);

    if (jni_exception(environment, "getStringExtra")) {
        return NULL;
    }

    return value;
}

static int send_sms_permission_granted(
    JNIEnv *environment,
    ANativeActivity *activity)
{
    jclass activity_class =
        (*environment)->GetObjectClass(environment, activity->clazz);
    if (activity_class == NULL || jni_exception(environment, "Activity class lookup")) {
        return 0;
    }

    jmethodID check_permission = (*environment)->GetMethodID(
        environment,
        activity_class,
        "checkSelfPermission",
        "(Ljava/lang/String;)I");
    if (check_permission == NULL ||
        jni_exception(environment, "checkSelfPermission lookup")) {
        return 0;
    }

    jstring permission = (*environment)->NewStringUTF(
        environment,
        "android.permission.SEND_SMS");
    if (permission == NULL ||
        jni_exception(environment, "SEND_SMS permission string creation")) {
        return 0;
    }

    jint result = (*environment)->CallIntMethod(
        environment,
        activity->clazz,
        check_permission,
        permission);
    (void)(*environment)->DeleteLocalRef(environment, permission);

    if (jni_exception(environment, "checkSelfPermission")) {
        return 0;
    }

    return result == 0;
}

static void log_request_pass(
    JNIEnv *environment,
    jstring request_id,
    jstring body)
{
    const char *request = "request";
    const char *borrowed_request = NULL;

    if (request_id != NULL) {
        borrowed_request =
            (*environment)->GetStringUTFChars(environment, request_id, NULL);
        if (borrowed_request != NULL &&
            !jni_exception(environment, "request-id UTF-8 conversion")) {
            request = borrowed_request;
        }
    }

    jsize body_bytes = (*environment)->GetStringUTFLength(environment, body);
    if (jni_exception(environment, "body UTF-8 length")) {
        body_bytes = -1;
    }

    __android_log_print(
        ANDROID_LOG_INFO,
        LOG_TAG,
        "PASS request_submitted request_id=%s body_utf8_bytes=%d",
        request,
        (int)body_bytes);

    if (borrowed_request != NULL) {
        (*environment)->ReleaseStringUTFChars(
            environment,
            request_id,
            borrowed_request);
    }
}

static int submit_sms(
    JNIEnv *environment,
    jstring destination,
    jstring body)
{
    jclass sms_manager_class = (*environment)->FindClass(
        environment,
        "android/telephony/SmsManager");
    if (sms_manager_class == NULL ||
        jni_exception(environment, "SmsManager class lookup")) {
        return 0;
    }

    jmethodID get_default = (*environment)->GetStaticMethodID(
        environment,
        sms_manager_class,
        "getDefault",
        "()Landroid/telephony/SmsManager;");
    if (get_default == NULL ||
        jni_exception(environment, "SmsManager.getDefault lookup")) {
        return 0;
    }

    jobject sms_manager = (*environment)->CallStaticObjectMethod(
        environment,
        sms_manager_class,
        get_default);
    if (sms_manager == NULL ||
        jni_exception(environment, "SmsManager.getDefault")) {
        __android_log_print(
            ANDROID_LOG_ERROR,
            LOG_TAG,
            "FAIL Android SMS service is unavailable");
        return 0;
    }

    jmethodID send_text_message = (*environment)->GetMethodID(
        environment,
        sms_manager_class,
        "sendTextMessage",
        "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;"
        "Landroid/app/PendingIntent;Landroid/app/PendingIntent;)V");
    if (send_text_message == NULL ||
        jni_exception(environment, "SmsManager.sendTextMessage lookup")) {
        return 0;
    }

    (*environment)->CallVoidMethod(
        environment,
        sms_manager,
        send_text_message,
        destination,
        NULL,
        body,
        NULL,
        NULL);

    if (jni_exception(environment, "SmsManager.sendTextMessage")) {
        return 0;
    }

    return 1;
}

void ANativeActivity_onCreate(
    ANativeActivity *activity,
    void *saved_state,
    size_t saved_state_size)
{
    (void)saved_state;
    (void)saved_state_size;

    JNIEnv *environment = activity->env;
    jobject intent = activity_intent(environment, activity);
    if (intent == NULL) {
        ANativeActivity_finish(activity);
        return;
    }

    jstring destination =
        intent_string_extra(environment, intent, "destination");
    jstring body = intent_string_extra(environment, intent, "body");
    jstring request_id =
        intent_string_extra(environment, intent, "request_id");

    if (destination == NULL) {
        __android_log_print(
            ANDROID_LOG_ERROR,
            LOG_TAG,
            "FAIL destination is missing");
        ANativeActivity_finish(activity);
        return;
    }

    if (body == NULL) {
        __android_log_print(
            ANDROID_LOG_ERROR,
            LOG_TAG,
            "FAIL message body is missing");
        ANativeActivity_finish(activity);
        return;
    }

    if (!send_sms_permission_granted(environment, activity)) {
        __android_log_print(
            ANDROID_LOG_ERROR,
            LOG_TAG,
            "FAIL SMS permission not granted (SEND_SMS)");
        ANativeActivity_finish(activity);
        return;
    }

    if (submit_sms(environment, destination, body)) {
        log_request_pass(environment, request_id, body);
    }

    ANativeActivity_finish(activity);
}
