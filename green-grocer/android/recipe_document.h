#ifndef GREEN_GROCER_RECIPE_DOCUMENT_H
#define GREEN_GROCER_RECIPE_DOCUMENT_H

#include <android/looper.h>
#include <android/native_activity.h>
#include <stdbool.h>

typedef enum { GG_DOCUMENT_NONE, GG_DOCUMENT_READY, GG_DOCUMENT_CANCELLED,
               GG_DOCUMENT_FAILED } gg_document_result;

/* SDK/provider transport only. The worker copies at most 1 MiB to a private
 * temporary file. Only Idriç validates and saves its recipe contents. */
bool gg_recipe_document_request(ANativeActivity *activity, JNIEnv *env);
void gg_recipe_document_listen(ALooper *looper);
void gg_recipe_document_unlisten(ALooper *looper);
gg_document_result gg_recipe_document_take(char **path);

#endif
