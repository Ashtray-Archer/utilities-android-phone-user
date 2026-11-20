#ifndef GREEN_GROCER_RECIPE_INPUT_H
#define GREEN_GROCER_RECIPE_INPUT_H

#include <android/input.h>
#include <android/native_activity.h>
#include <jni.h>
#include <stdbool.h>

/* SDK focus/InputConnection plumbing. Editable text and selection belong to
 * the checked Idriç session; the transparent View never renders a form. */
bool gg_recipe_input_open(ANativeActivity *activity, JNIEnv *env);
void gg_recipe_input_close(JNIEnv *env);
bool gg_recipe_input_refresh(JNIEnv *env, bool show_keyboard);
bool gg_recipe_input_paste(JNIEnv *env);
bool gg_recipe_input_key(JNIEnv *env, const AInputEvent *event);

#endif
