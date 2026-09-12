#include <android/log.h>
#include <android/looper.h>
#include <android/native_window.h>
#include <android/sensor.h>
#include <android_native_app_glue.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define LOG_TAG "Accelerometer"
#define SENSOR_PERIOD_US 20000

struct glyph {
    char character;
    uint8_t rows[7];
};

static const struct glyph glyphs[] = {
    {' ', {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
    {'+', {0x00, 0x04, 0x04, 0x1f, 0x04, 0x04, 0x00}},
    {'-', {0x00, 0x00, 0x00, 0x1f, 0x00, 0x00, 0x00}},
    {'.', {0x00, 0x00, 0x00, 0x00, 0x00, 0x0c, 0x0c}},
    {'/', {0x01, 0x02, 0x02, 0x04, 0x08, 0x08, 0x10}},
    {'0', {0x0e, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0e}},
    {'1', {0x04, 0x0c, 0x14, 0x04, 0x04, 0x04, 0x1f}},
    {'2', {0x0e, 0x11, 0x01, 0x02, 0x04, 0x08, 0x1f}},
    {'3', {0x1e, 0x01, 0x01, 0x0e, 0x01, 0x01, 0x1e}},
    {'4', {0x02, 0x06, 0x0a, 0x12, 0x1f, 0x02, 0x02}},
    {'5', {0x1f, 0x10, 0x10, 0x1e, 0x01, 0x01, 0x1e}},
    {'6', {0x0e, 0x10, 0x10, 0x1e, 0x11, 0x11, 0x0e}},
    {'7', {0x1f, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08}},
    {'8', {0x0e, 0x11, 0x11, 0x0e, 0x11, 0x11, 0x0e}},
    {'9', {0x0e, 0x11, 0x11, 0x0f, 0x01, 0x01, 0x0e}},
    {'A', {0x0e, 0x11, 0x11, 0x1f, 0x11, 0x11, 0x11}},
    {'C', {0x0f, 0x10, 0x10, 0x10, 0x10, 0x10, 0x0f}},
    {'E', {0x1f, 0x10, 0x10, 0x1e, 0x10, 0x10, 0x1f}},
    {'L', {0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1f}},
    {'M', {0x11, 0x1b, 0x15, 0x15, 0x11, 0x11, 0x11}},
    {'N', {0x11, 0x19, 0x19, 0x15, 0x13, 0x13, 0x11}},
    {'O', {0x0e, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0e}},
    {'R', {0x1e, 0x11, 0x11, 0x1e, 0x14, 0x12, 0x11}},
    {'S', {0x0f, 0x10, 0x10, 0x0e, 0x01, 0x01, 0x1e}},
    {'T', {0x1f, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04}},
    {'X', {0x11, 0x11, 0x0a, 0x04, 0x0a, 0x11, 0x11}},
    {'Y', {0x11, 0x11, 0x0a, 0x04, 0x04, 0x04, 0x04}},
    {'Z', {0x1f, 0x01, 0x02, 0x04, 0x08, 0x10, 0x1f}},
    {'?', {0x0e, 0x11, 0x01, 0x02, 0x04, 0x00, 0x04}},
};

struct accelerometer_state {
    struct android_app *app;
    ASensorManager *sensor_manager;
    const ASensor *accelerometer;
    ASensorEventQueue *sensor_queue;
    float x;
    float y;
    float z;
    bool sensor_enabled;
    bool have_sample;
    unsigned int log_counter;
};

static const uint8_t *glyph_rows(char character)
{
    size_t count = sizeof(glyphs) / sizeof(glyphs[0]);
    for (size_t index = 0; index < count; ++index) {
        if (glyphs[index].character == character) {
            return glyphs[index].rows;
        }
    }
    return glyphs[count - 1U].rows;
}

static void put_pixel(ANativeWindow_Buffer *buffer, int32_t x, int32_t y, uint32_t value)
{
    if (x < 0 || y < 0 || x >= buffer->width || y >= buffer->height) {
        return;
    }
    uint32_t *pixels = (uint32_t *)buffer->bits;
    size_t offset = (size_t)y * (size_t)buffer->stride + (size_t)x;
    pixels[offset] = value;
}

static void fill_rect(
    ANativeWindow_Buffer *buffer,
    int32_t left,
    int32_t top,
    int32_t width,
    int32_t height,
    uint32_t value)
{
    for (int32_t y = 0; y < height; ++y) {
        for (int32_t x = 0; x < width; ++x) {
            put_pixel(buffer, left + x, top + y, value);
        }
    }
}

static void draw_glyph(
    ANativeWindow_Buffer *buffer,
    char character,
    int32_t left,
    int32_t top,
    int32_t scale,
    uint32_t value)
{
    const uint8_t *rows = glyph_rows(character);
    for (int32_t row = 0; row < 7; ++row) {
        for (int32_t column = 0; column < 5; ++column) {
            uint8_t mask = (uint8_t)(1U << (unsigned int)(4 - column));
            if ((rows[row] & mask) != 0U) {
                fill_rect(
                    buffer,
                    left + column * scale,
                    top + row * scale,
                    scale,
                    scale,
                    value);
            }
        }
    }
}

static void draw_text(
    ANativeWindow_Buffer *buffer,
    const char *text,
    int32_t left,
    int32_t top,
    int32_t scale,
    uint32_t value)
{
    int32_t x = left;
    for (const char *cursor = text; *cursor != '\0'; ++cursor) {
        draw_glyph(buffer, *cursor, x, top, scale, value);
        x += 6 * scale;
    }
}

static void clear_buffer(ANativeWindow_Buffer *buffer, uint32_t value)
{
    uint32_t *pixels = (uint32_t *)buffer->bits;
    for (int32_t y = 0; y < buffer->height; ++y) {
        for (int32_t x = 0; x < buffer->width; ++x) {
            size_t offset = (size_t)y * (size_t)buffer->stride + (size_t)x;
            pixels[offset] = value;
        }
    }
}

static void draw_axis_bar(
    ANativeWindow_Buffer *buffer,
    int32_t center_x,
    int32_t center_y,
    int32_t half_width,
    int32_t height,
    float acceleration,
    uint32_t value)
{
    float clipped = acceleration;
    if (clipped > 20.0F) {
        clipped = 20.0F;
    } else if (clipped < -20.0F) {
        clipped = -20.0F;
    }

    int32_t length = (int32_t)((clipped / 20.0F) * (float)half_width);
    if (length >= 0) {
        fill_rect(buffer, center_x, center_y, length, height, value);
    } else {
        fill_rect(buffer, center_x + length, center_y, -length, height, value);
    }
}

static void draw_screen(struct accelerometer_state *state)
{
    if (state->app->window == NULL) {
        return;
    }

    (void)ANativeWindow_setBuffersGeometry(
        state->app->window,
        0,
        0,
        WINDOW_FORMAT_RGBA_8888);

    ANativeWindow_Buffer buffer;
    if (ANativeWindow_lock(state->app->window, &buffer, NULL) != 0) {
        return;
    }

    const uint32_t background = 0xff181818U;
    const uint32_t foreground = 0xffeeeeeeU;
    const uint32_t secondary = 0xffa8a8a8U;
    clear_buffer(&buffer, background);

    int32_t scale = buffer.width / 180;
    if (scale < 2) {
        scale = 2;
    } else if (scale > 6) {
        scale = 6;
    }

    int32_t left = 6 * scale;
    int32_t top = 8 * scale;
    int32_t line_height = 12 * scale;

    draw_text(&buffer, "ACCELEROMETER", left, top, scale, foreground);

    if (state->accelerometer == NULL) {
        draw_text(&buffer, "NO SENSOR", left, top + 2 * line_height, scale, foreground);
        (void)ANativeWindow_unlockAndPost(state->app->window);
        return;
    }

    char x_line[48];
    char y_line[48];
    char z_line[48];
    (void)snprintf(x_line, sizeof(x_line), "X %+7.3f M/S2", (double)state->x);
    (void)snprintf(y_line, sizeof(y_line), "Y %+7.3f M/S2", (double)state->y);
    (void)snprintf(z_line, sizeof(z_line), "Z %+7.3f M/S2", (double)state->z);

    draw_text(&buffer, x_line, left, top + 2 * line_height, scale, foreground);
    draw_text(&buffer, y_line, left, top + 4 * line_height, scale, foreground);
    draw_text(&buffer, z_line, left, top + 6 * line_height, scale, foreground);

    int32_t center_x = buffer.width / 2;
    int32_t half_width = (buffer.width - 2 * left) / 2;
    int32_t bar_height = scale + 1;
    fill_rect(&buffer, center_x, top + 3 * line_height, 1, 5 * line_height, secondary);
    draw_axis_bar(&buffer, center_x, top + 3 * line_height, half_width, bar_height, state->x, secondary);
    draw_axis_bar(&buffer, center_x, top + 5 * line_height, half_width, bar_height, state->y, secondary);
    draw_axis_bar(&buffer, center_x, top + 7 * line_height, half_width, bar_height, state->z, secondary);

    (void)ANativeWindow_unlockAndPost(state->app->window);
}

static void set_sensor_enabled(struct accelerometer_state *state, bool enabled)
{
    if (state->sensor_queue == NULL || state->accelerometer == NULL) {
        state->sensor_enabled = false;
        return;
    }

    if (enabled && !state->sensor_enabled) {
        if (ASensorEventQueue_enableSensor(state->sensor_queue, state->accelerometer) == 0) {
            (void)ASensorEventQueue_setEventRate(
                state->sensor_queue,
                state->accelerometer,
                SENSOR_PERIOD_US);
            state->sensor_enabled = true;
        }
    } else if (!enabled && state->sensor_enabled) {
        (void)ASensorEventQueue_disableSensor(state->sensor_queue, state->accelerometer);
        state->sensor_enabled = false;
    }
}

static void handle_app_command(struct android_app *app, int32_t command)
{
    struct accelerometer_state *state = (struct accelerometer_state *)app->userData;
    switch (command) {
        case APP_CMD_INIT_WINDOW:
            draw_screen(state);
            break;
        case APP_CMD_GAINED_FOCUS:
            set_sensor_enabled(state, true);
            draw_screen(state);
            break;
        case APP_CMD_LOST_FOCUS:
        case APP_CMD_PAUSE:
            set_sensor_enabled(state, false);
            break;
        default:
            break;
    }
}

static void consume_sensor_events(struct accelerometer_state *state)
{
    if (state->sensor_queue == NULL) {
        return;
    }

    ASensorEvent event;
    while (ASensorEventQueue_getEvents(state->sensor_queue, &event, 1) > 0) {
        if (event.type != ASENSOR_TYPE_ACCELEROMETER) {
            continue;
        }

        state->x = event.acceleration.x;
        state->y = event.acceleration.y;
        state->z = event.acceleration.z;
        state->have_sample = true;

        state->log_counter += 1U;
        if (state->log_counter >= 25U) {
            __android_log_print(
                ANDROID_LOG_INFO,
                LOG_TAG,
                "x=%.4f y=%.4f z=%.4f m/s2",
                (double)state->x,
                (double)state->y,
                (double)state->z);
            state->log_counter = 0U;
        }
    }

    draw_screen(state);
}

void android_main(struct android_app *app)
{
    struct accelerometer_state state;
    (void)memset(&state, 0, sizeof(state));
    state.app = app;
    app->userData = &state;
    app->onAppCmd = handle_app_command;

    state.sensor_manager = ASensorManager_getInstanceForPackage(
        "com.ashtrayarcher.accelerometer");
    if (state.sensor_manager != NULL) {
        state.accelerometer = ASensorManager_getDefaultSensor(
            state.sensor_manager,
            ASENSOR_TYPE_ACCELEROMETER);
        state.sensor_queue = ASensorManager_createEventQueue(
            state.sensor_manager,
            app->looper,
            LOOPER_ID_USER,
            NULL,
            NULL);
    }

    for (;;) {
        int events = 0;
        struct android_poll_source *source = NULL;
        int ident = ALooper_pollOnce(
            state.sensor_enabled ? -1 : 100,
            NULL,
            &events,
            (void **)&source);

        if (source != NULL) {
            source->process(app, source);
        }
        if (ident == LOOPER_ID_USER) {
            consume_sensor_events(&state);
        }
        if (app->destroyRequested != 0) {
            break;
        }
    }

    set_sensor_enabled(&state, false);
    if (state.sensor_manager != NULL && state.sensor_queue != NULL) {
        (void)ASensorManager_destroyEventQueue(state.sensor_manager, state.sensor_queue);
    }
}
