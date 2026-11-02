#ifndef PASTEBOARD_HISTORY_H
#define PASTEBOARD_HISTORY_H
#include "pasteboard_store.h"
#include "pasteboard_wire.h"
#include <jni.h>

typedef struct {
    PBText identity;
    PBText text;
    PBText captured_at;
    int board;
    bool protected;
} PBHistoryEntry;

typedef struct {
    PBHistoryEntry *entries;
    size_t count;
    int selected_board;
    bool capture_enabled;
    bool automatic_requested;
    uint64_t generation;
    PBStoreStatus storage_status;
    char outcome[64];
} PBHistoryView;

bool pb_history_open(JNIEnv *environment, jobject context);
bool pb_history_apply(JNIEnv *environment, const char *action,
                       const PBText *arguments, size_t argument_count,
                       PBHistoryView *view);
void pb_history_view_free(PBHistoryView *view);
const char *pb_history_last_error(void);
const char *pb_history_private_directory(void);
bool pb_history_capture_enabled(void);
bool pb_history_automatic_requested(void);
void pb_history_set_change_handler(void (*handler)(void *), void *owner);

/* Explicit standard UTF-8 boundary, also used by the clipboard observer. */
jstring pb_history_java_text(JNIEnv *environment, const char *bytes, size_t length);
bool pb_history_native_text(JNIEnv *environment, jstring string, PBText *text);
#endif
