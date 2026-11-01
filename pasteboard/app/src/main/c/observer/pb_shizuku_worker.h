#ifndef PB_SHIZUKU_WORKER_H
#define PB_SHIZUKU_WORKER_H
#include <jni.h>
/* Called only on a native connection thread, after an actual permission check.
 * The worker receives a narrow per-install clipboard delivery capability. */
int pb_shizuku_observer_connect(JNIEnv *environment, const char *token);
#endif
