#ifndef PB_SHIZUKU_CLIENT_H
#define PB_SHIZUKU_CLIENT_H

#include <jni.h>
#include <stdint.h>
#include <sys/types.h>

/* OS authorization only. Capture, ordering and retention remain checked Idriç
 * decisions; a status enum alone does not authorize an event. */
typedef enum {
    PB_SHIZUKU_UNAVAILABLE = 0,
    PB_SHIZUKU_ATTACHING = 1,
    PB_SHIZUKU_PERMISSION_REQUIRED = 2,
    PB_SHIZUKU_AUTHORIZED = 3,
    PB_SHIZUKU_PLATFORM_ERROR = 4
} pb_shizuku_capability_state;

typedef void (*pb_shizuku_revocation_handler)(void *owner, uint64_t revoked_through_epoch);

/* Unregister waits for an already running handler. A handler must not call
 * this registration function; revoke only ingress instances minted no later
 * than revoked_through_epoch, promptly. A late old event cannot revoke a new
 * legitimate connection's instances. */
void pb_shizuku_client_set_revocation_handler(pb_shizuku_revocation_handler handler,
                                             void *owner);

/* Every use rechecks actual Shizuku permission after a validated application
 * attachment. A cached permission callback is never sufficient authority. */
pb_shizuku_capability_state pb_shizuku_client_authorization(JNIEnv *environment,
                                                            uid_t *server_uid,
                                                            uint64_t *capability_epoch);
int pb_shizuku_client_request_permission(JNIEnv *environment);

/* App-owned companion adapter only; never return this broad service
 * handle to application behavior or distribute it as the private capture key.
 * The caller owns the returned local JNI reference. */
jobject pb_shizuku_client_authorized_server(JNIEnv *environment,
                                            uid_t *server_uid,
                                            uint64_t *capability_epoch);

#endif
