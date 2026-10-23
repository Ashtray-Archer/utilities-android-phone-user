/* Narrow native instrument adapter: Android's actual reported values -> text.
 * No fused orientation, smoothing, calibration or substitute sensor readings. */
#include <android/native_window.h>
#include <android/sensor.h>
#include <android/log.h>
#include <android_native_app_glue.h>
#include "subpixel/font.h"
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#if INSTRUMENT_KIND == 1
#define SENSOR_TYPE ASENSOR_TYPE_GYROSCOPE
#define TITLE "GYROSCOPE"
#define UNIT "rad/s"
#define PACKAGE "com.ashtrayarcher.gyroscope"
#define AXES 3
#elif INSTRUMENT_KIND == 2
#define SENSOR_TYPE ASENSOR_TYPE_MAGNETIC_FIELD
#define TITLE "MAGNETOMETER"
#define UNIT "microtesla"
#define PACKAGE "com.ashtrayarcher.magnetometer"
#define AXES 3
#elif INSTRUMENT_KIND == 3
#define SENSOR_TYPE ASENSOR_TYPE_LIGHT
#define TITLE "AMBIENT LIGHT"
#define UNIT "lux"
#define PACKAGE "com.isomorphisms.ambientlight"
#define AXES 1
#else
#error INSTRUMENT_KIND must select gyroscope (1), magnetometer (2), or light (3)
#endif

struct instrument {
    struct android_app *app;
    ASensorManager *manager;
    const ASensor *sensor;
    ASensorEventQueue *queue;
    bool enabled, have_sample;
    float value[AXES];
    int64_t timestamp;
    const char *status;
};
struct cached_mask { unsigned char code; int scale; uint8_t *coverage; };
static struct cached_mask cache[96];
static unsigned replacement;

static const uint8_t *mask(unsigned char code, int scale) {
    for (unsigned i = 0; i < 96; ++i)
        if (cache[i].coverage && cache[i].code == code && cache[i].scale == scale)
            return cache[i].coverage;
    if (scale < 1 || scale > 20) return NULL;
    size_t bytes = 35U * (size_t)scale * (size_t)scale;
    uint8_t *coverage = malloc(bytes);
    if (!coverage) return NULL;
    if (!sf57_render_coverage(code,(unsigned)scale,0,0,coverage,bytes)) {
        free(coverage); return NULL;
    }
    unsigned slot = replacement++ % 96;
    free(cache[slot].coverage);
    cache[slot] = (struct cached_mask){code,scale,coverage};
    return coverage;
}
static void text(ANativeWindow_Buffer *buffer, const char *s, int top, int scale,
    uint32_t ink) {
    int left = (buffer->width-((int)strlen(s)*6-1)*scale)/2;
    uint32_t *pixels = buffer->bits;
    for (; *s; ++s,left += 6*scale) {
        const uint8_t *coverage = mask((unsigned char)*s,scale);
        if (!coverage) continue;
        for (int y = 0; y < 7*scale; ++y)
            for (int x = 0; x < 5*scale; ++x) {
                int px = left+x, py = top+y;
                if (px < 0 || py < 0 || px >= buffer->width || py >= buffer->height) continue;
                unsigned alpha = coverage[y*5*scale+x];
                size_t at = (size_t)py*(size_t)buffer->stride+(size_t)px;
                uint32_t under = pixels[at], mixed = 0;
                for (unsigned shift = 0; shift < 32; shift += 8) {
                    unsigned channel = (((under>>shift)&255)*(255-alpha)+
                        ((ink>>shift)&255)*alpha+127)/255;
                    mixed |= channel << shift;
                }
                pixels[at] = mixed;
            }
    }
}
static void draw(struct instrument *state) {
    if (!state->app->window) return;
    ANativeWindow_Buffer buffer;
    struct timespec started = {0}, finished = {0};
    (void)clock_gettime(CLOCK_MONOTONIC,&started);
    if (ANativeWindow_lock(state->app->window,&buffer,NULL) < 0) return;
    uint32_t *pixels = buffer.bits;
    for (int y = 0; y < buffer.height; ++y)
        for (int x = 0; x < buffer.width; ++x)
            pixels[(size_t)y*(size_t)buffer.stride+(size_t)x] = UINT32_C(0xff161616);
    int small = buffer.width/180;
    if (small < 2) small = 2;
    if (small > 5) small = 5;
    text(&buffer,TITLE,buffer.height/12,small+1,UINT32_C(0xffeeeeee));
    text(&buffer,state->status,buffer.height/5,small,UINT32_C(0xffa0a0a0));
    if (state->have_sample) {
        for (unsigned axis = 0; axis < AXES; ++axis) {
            char number[64];
            if (AXES == 3)
                (void)snprintf(number,sizeof number,"%c %+.5g",(int)('X'+axis),(double)state->value[axis]);
            else (void)snprintf(number,sizeof number,"%.5g",(double)state->value[axis]);
            text(&buffer,number,buffer.height*(3+(int)axis)/8,small+2,UINT32_C(0xffeee8c0));
        }
        text(&buffer,UNIT,buffer.height*7/8,small,UINT32_C(0xffa0a0a0));
    }
    (void)ANativeWindow_unlockAndPost(state->app->window);
    (void)clock_gettime(CLOCK_MONOTONIC,&finished);
    static unsigned frames;
    if (frames++ == 0 || frames % 100 == 0) {
        long long ns = (long long)(finished.tv_sec-started.tv_sec)*1000000000LL+
            (long long)finished.tv_nsec-started.tv_nsec;
        size_t cached_bytes = 0;
        for (unsigned i = 0; i < 96; ++i)
            if (cache[i].coverage) cached_bytes += 35U*(size_t)cache[i].scale*(size_t)cache[i].scale;
        __android_log_print(ANDROID_LOG_INFO,"Instrument", "frame_posted frames=%u render_ns=%lld cached_bytes=%zu",frames,ns,cached_bytes);
    }
}
static void disable(struct instrument *state) {
    if (state->enabled) (void)ASensorEventQueue_disableSensor(state->queue,state->sensor);
    state->enabled = false;
    state->have_sample = false;
}
static void command(struct android_app *app, int32_t cmd) {
    struct instrument *state = app->userData;
    if (cmd == APP_CMD_INIT_WINDOW) {
        (void)ANativeWindow_setBuffersGeometry(app->window,0,0,WINDOW_FORMAT_RGBA_8888);
        draw(state);
    } else if (cmd == APP_CMD_GAINED_FOCUS) {
        state->have_sample = false;
        if (state->queue) {
            ASensorEvent discarded;
            while (ASensorEventQueue_getEvents(state->queue,&discarded,1) > 0) {}
        }
        if (state->queue && ASensorEventQueue_enableSensor(state->queue,state->sensor) >= 0) {
            state->enabled = true;
            int delay = ASensor_getMinDelay(state->sensor);
            if (delay > 0 && ASensorEventQueue_setEventRate(state->queue,state->sensor,
                    delay > 20000 ? delay : 20000) < 0) {
                disable(state); state->status = "SENSOR RATE FAILED";
            } else state->status = "WAITING";
        } else state->status = state->sensor ? "SENSOR ENABLE FAILED" : "NO SENSOR";
        draw(state);
    } else if (cmd == APP_CMD_LOST_FOCUS || cmd == APP_CMD_TERM_WINDOW) {
        disable(state); state->status = "PAUSED";
    }
}
void android_main(struct android_app *app) {
    struct instrument state = {.app=app,.status="NO SENSOR"};
    app->userData = &state;
    app->onAppCmd = command;
    state.manager = ASensorManager_getInstanceForPackage(PACKAGE);
    if (state.manager) state.sensor = ASensorManager_getDefaultSensor(state.manager,SENSOR_TYPE);
    if (state.sensor) state.queue = ASensorManager_createEventQueue(state.manager,app->looper,3,NULL,NULL);
    if (state.queue) state.status = "WAITING";
    while (!app->destroyRequested) {
        struct android_poll_source *source = NULL;
        int id = ALooper_pollOnce(-1,NULL,NULL,(void **)&source);
        if (id == ALOOPER_POLL_ERROR) break;
        if (source) source->process(app,source);
        if (id == 3 && state.queue) {
            ASensorEvent event;
            bool changed = false;
            while (ASensorEventQueue_getEvents(state.queue,&event,1) > 0) {
                if (!state.enabled || event.type != SENSOR_TYPE || event.timestamp <= state.timestamp) continue;
                bool valid = true;
                for (unsigned axis = 0; axis < AXES; ++axis)
                    if (!isfinite(event.data[axis])) valid = false;
                if (AXES == 1 && event.data[0] < 0) valid = false;
                if (!valid) { state.have_sample=false; state.status="INVALID READING"; changed=true; continue; }
                for (unsigned axis = 0; axis < AXES; ++axis) state.value[axis] = event.data[axis];
                state.timestamp = event.timestamp;
                state.have_sample = true;
                state.status = "ANDROID READING";
                changed = true;
            }
            if (changed) draw(&state);
        }
    }
    disable(&state);
    if (state.queue) (void)ASensorManager_destroyEventQueue(state.manager,state.queue);
    for (unsigned i = 0; i < 96; ++i) free(cache[i].coverage);
}
