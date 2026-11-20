#ifndef GREEN_GROCER_RECIPE_ENGINE_H
#define GREEN_GROCER_RECIPE_ENGINE_H

#include "recipe_view.h"
#include <android/native_activity.h>
#include <android/looper.h>
#include <jni.h>
#include <limits.h>

typedef struct {
    JavaVM *vm;
    JNIEnv *env;
    bool detach;
    char directory[PATH_MAX];
    uint64_t revision;
} gg_recipe_engine;

typedef struct {
    jstring target, text;
    jint first, last, composing_first, composing_last;
} gg_recipe_input_snapshot;

bool gg_recipe_engine_open(gg_recipe_engine *engine, ANativeActivity *activity,
                           gg_recipe_view *view);
void gg_recipe_engine_close(gg_recipe_engine *engine);
bool gg_recipe_engine_action(gg_recipe_engine *engine, const gg_text *token,
                             gg_recipe_view *view);
bool gg_recipe_engine_import(gg_recipe_engine *engine, const char *path,
                             gg_recipe_view *view);
/* 1: checked navigation handled, 0: root permits Activity finish, -1: failure. */
int gg_recipe_engine_back(gg_recipe_engine *engine, gg_recipe_view *view);
void gg_recipe_engine_listen(ALooper *looper);
void gg_recipe_engine_unlisten(ALooper *looper);
/* 1: refreshed, 0: unchanged, -1: projection failed. */
int gg_recipe_engine_refresh(gg_recipe_engine *engine, gg_recipe_view *view);
bool gg_recipe_engine_snapshot(JNIEnv *env, gg_recipe_input_snapshot *snapshot);
bool gg_recipe_engine_input(JNIEnv *env, jstring expected, const char *operation,
                            jstring text, jint first, jint last);
bool gg_recipe_engine_replace(JNIEnv *env, jstring expected, jstring text,
                              jint first, jint last, jint cursor);

#endif
