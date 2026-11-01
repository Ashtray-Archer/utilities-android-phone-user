#ifndef PB_OBSERVER_H
#define PB_OBSERVER_H
#include <jni.h>
#include <stdbool.h>
#include <sys/types.h>

void pb_observer_app_ready(JNIEnv *environment, jobject context);
void pb_observer_try_start(JNIEnv *environment);
bool pb_observer_begin_capture_session(void);
int pb_observer_enable(JNIEnv *environment);
const char *pb_observer_status(void);
void pb_observer_connected(int accepted);
jobject pb_observer_provider_request(JNIEnv *environment, jstring method,
    jstring token, jobject extras, uid_t caller, int *handled);
#endif
