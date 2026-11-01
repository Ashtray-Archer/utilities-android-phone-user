/* Mechanical JNI implementation of Shizuku-API a27f6e4151's BinderContainer
 * wire contract. No acquisition, navigation, authorization or storage policy
 * belongs here. Candidate classes come from the maintained typed DEX writer. */
#include <jni.h>

static void invalid(JNIEnv *environment, const char *message) {
    jclass type = (*environment)->FindClass(environment, "java/lang/IllegalArgumentException");
    if (type) (*environment)->ThrowNew(environment, type, message);
}

JNIEXPORT void JNICALL
Java_moe_shizuku_api_BinderContainer_initializeCreator(JNIEnv *environment, jclass container) {
    jfieldID binder = (*environment)->GetFieldID(environment, container, "binder", "Landroid/os/IBinder;");
    if (!binder) return;
    jfieldID creator_field = (*environment)->GetStaticFieldID(environment, container,
        "CREATOR", "Landroid/os/Parcelable$Creator;");
    if (!creator_field) return;
    jclass creator_type = (*environment)->FindClass(environment, "com/isomorphisms/pasteboard/BinderContainerCreator");
    if (!creator_type) return;
    jmethodID constructor = (*environment)->GetMethodID(environment, creator_type, "<init>", "()V");
    if (!constructor) return;
    jobject creator = (*environment)->NewObject(environment, creator_type, constructor);
    if (!creator) return;
    (*environment)->SetStaticObjectField(environment, container, creator_field, creator);
}

JNIEXPORT jint JNICALL
Java_moe_shizuku_api_BinderContainer_describeContents(JNIEnv *environment, jobject container) {
    (void)environment;
    (void)container;
    return 0;
}

JNIEXPORT void JNICALL
Java_moe_shizuku_api_BinderContainer_writeToParcel(JNIEnv *environment, jobject container,
                                                  jobject parcel, jint flags) {
    (void)flags;
    if (!parcel) { invalid(environment, "BinderContainer requires a Parcel"); return; }
    jclass container_type = (*environment)->GetObjectClass(environment, container);
    if (!container_type) return;
    jclass parcel_type = (*environment)->FindClass(environment, "android/os/Parcel");
    if (!parcel_type) return;
    jfieldID binder_field = (*environment)->GetFieldID(environment, container_type,
        "binder", "Landroid/os/IBinder;");
    if (!binder_field) return;
    jmethodID write = (*environment)->GetMethodID(environment, parcel_type,
        "writeStrongBinder", "(Landroid/os/IBinder;)V");
    if (!write) return;
    jobject binder = (*environment)->GetObjectField(environment, container, binder_field);
    if ((*environment)->ExceptionCheck(environment)) return;
    (*environment)->CallVoidMethod(environment, parcel, write, binder);
}

JNIEXPORT jobject JNICALL
Java_com_isomorphisms_pasteboard_BinderContainerCreator_createFromParcel(JNIEnv *environment,
                                                                 jobject creator,
                                                                 jobject parcel) {
    (void)creator;
    if (!parcel) { invalid(environment, "BinderContainer requires a Parcel"); return NULL; }
    jclass parcel_type = (*environment)->FindClass(environment, "android/os/Parcel");
    if (!parcel_type) return NULL;
    jclass container_type = (*environment)->FindClass(environment, "moe/shizuku/api/BinderContainer");
    if (!container_type) return NULL;
    jmethodID read = (*environment)->GetMethodID(environment, parcel_type,
        "readStrongBinder", "()Landroid/os/IBinder;");
    if (!read) return NULL;
    jmethodID constructor = (*environment)->GetMethodID(environment, container_type, "<init>", "()V");
    if (!constructor) return NULL;
    jfieldID binder_field = (*environment)->GetFieldID(environment, container_type,
        "binder", "Landroid/os/IBinder;");
    if (!binder_field) return NULL;
    jobject binder = (*environment)->CallObjectMethod(environment, parcel, read);
    if ((*environment)->ExceptionCheck(environment)) return NULL;
    jobject result = (*environment)->NewObject(environment, container_type, constructor);
    if (!result) return NULL;
    (*environment)->SetObjectField(environment, result, binder_field, binder);
    return (*environment)->ExceptionCheck(environment) ? NULL : result;
}

JNIEXPORT jobjectArray JNICALL
Java_com_isomorphisms_pasteboard_BinderContainerCreator_newArray(JNIEnv *environment,
                                                        jobject creator, jint size) {
    (void)creator;
    if (size < 0 || size > 4096) {
        invalid(environment, "BinderContainer array exceeds its declared boundary");
        return NULL;
    }
    jclass container_type = (*environment)->FindClass(environment, "moe/shizuku/api/BinderContainer");
    if (!container_type) return NULL;
    return (*environment)->NewObjectArray(environment, size, container_type, NULL);
}
