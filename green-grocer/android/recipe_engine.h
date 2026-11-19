#ifndef GREEN_GROCER_RECIPE_ENGINE_H
#define GREEN_GROCER_RECIPE_ENGINE_H

#include "recipe_view.h"
#include <android/native_activity.h>
#include <jni.h>
#include <limits.h>

typedef struct {
    JavaVM *vm;
    JNIEnv *env;
    bool detach;
    char directory[PATH_MAX];
} gg_recipe_engine;

bool gg_recipe_engine_open(gg_recipe_engine *engine, ANativeActivity *activity,
                           gg_recipe_view *view);
void gg_recipe_engine_close(gg_recipe_engine *engine);
bool gg_recipe_engine_action(gg_recipe_engine *engine, const gg_text *token,
                             gg_recipe_view *view);
bool gg_recipe_engine_import(gg_recipe_engine *engine, const char *path,
                             gg_recipe_view *view);
/* 1: checked navigation handled, 0: root permits Activity finish, -1: failure. */
int gg_recipe_engine_back(gg_recipe_engine *engine, gg_recipe_view *view);

#endif
