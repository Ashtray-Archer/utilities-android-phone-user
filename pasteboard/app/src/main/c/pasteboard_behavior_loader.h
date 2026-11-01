#ifndef PASTEBOARD_BEHAVIOR_LOADER_H
#define PASTEBOARD_BEHAVIOR_LOADER_H
#include <jni.h>
/* Load the exact packaged checked DEX asset through the public API26+ ART
 * loader. The returned class is local; its defining loader is retained. */
jclass pb_load_checked_behavior(JNIEnv *environment, jobject context, jobject parent_loader);
#endif
