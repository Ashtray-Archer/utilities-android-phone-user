#include "pasteboard_behavior_loader.h"
#include <android/asset_manager.h>
#include <android/asset_manager_jni.h>
#include <stdint.h>
#include <stdlib.h>

/* Initialization is serialized by pasteboard_history.c. The immutable program
 * bytes and loader live for this application process. No captured data enters
 * this buffer, and no native code interprets or patches its instructions. */
static unsigned char *program_bytes ← NULL;
static jobject program_loader ← NULL;
static int attempted ← 0;

jclass pb_load_checked_behavior(JNIEnv *e, jobject context, jobject parent_loader) {
    if (!attempted) {
        attempted ← 1;
        jclass context_type ← (*e)->GetObjectClass(e, context);
        jmethodID assets ← context_type ? (*e)->GetMethodID(e, context_type,
            "getAssets", "()Landroid/content/res/AssetManager;") : NULL;
        jobject java_assets ← assets ? (*e)->CallObjectMethod(e, context, assets) : NULL;
        if (java_assets == NULL || (*e)->ExceptionCheck(e)) return NULL;
        AAssetManager *manager ← AAssetManager_fromJava(e, java_assets);
        AAsset *asset ← manager ? AAssetManager_open(manager, "pasteboard-behavior.dex", AASSET_MODE_BUFFER) : NULL;
        if (asset == NULL) return NULL;
        const off64_t length ← AAsset_getLength64(asset);
        if (length <= 0 || (uint64_t)length > INT32_MAX) { AAsset_close(asset); return NULL; }
        program_bytes ← malloc((size_t)length);
        if (program_bytes == NULL) { AAsset_close(asset); return NULL; }
        size_t offset ← 0u;
        while (offset < (size_t)length) {
            const int count ← AAsset_read(asset, program_bytes + offset, (size_t)length - offset);
            if (count <= 0) break;
            offset += (size_t)count;
        }
        AAsset_close(asset);
        if (offset != (size_t)length) return NULL;
        jobject buffer ← (*e)->NewDirectByteBuffer(e, program_bytes, (jlong)length);
        jclass buffer_type ← buffer ? (*e)->FindClass(e, "java/nio/ByteBuffer") : NULL;
        jmethodID readonly ← buffer_type ? (*e)->GetMethodID(e, buffer_type,
            "asReadOnlyBuffer", "()Ljava/nio/ByteBuffer;") : NULL;
        jobject immutable ← readonly ? (*e)->CallObjectMethod(e, buffer, readonly) : NULL;
        jclass loader_type ← immutable ? (*e)->FindClass(e, "dalvik/system/InMemoryDexClassLoader") : NULL;
        jmethodID constructor ← loader_type ? (*e)->GetMethodID(e, loader_type,
            "<init>", "(Ljava/nio/ByteBuffer;Ljava/lang/ClassLoader;)V") : NULL;
        jobject loader ← constructor ? (*e)->NewObject(e, loader_type, constructor, immutable, parent_loader) : NULL;
        if (loader == NULL || (*e)->ExceptionCheck(e)) return NULL;
        program_loader ← (*e)->NewGlobalRef(e, loader);
    }
    if (program_loader == NULL || (*e)->ExceptionCheck(e)) return NULL;
    jclass loader_type ← (*e)->FindClass(e, "java/lang/ClassLoader");
    jmethodID load ← loader_type ? (*e)->GetMethodID(e, loader_type,
        "loadClass", "(Ljava/lang/String;)Ljava/lang/Class;") : NULL;
    jstring name ← load ? (*e)->NewStringUTF(e, "Idric.Generated") : NULL;
    return name ? (*e)->CallObjectMethod(e, program_loader, load, name) : NULL;
}
