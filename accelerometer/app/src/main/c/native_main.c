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

_Static_assert(sizeof(_Float16) == 2U, "_Float16 must use two-byte storage");

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
    {'D', {0x1e, 0x11, 0x11, 0x11, 0x11, 0x11, 0x1e}},
    {'E', {0x1f, 0x10, 0x10, 0x1e, 0x10, 0x10, 0x1f}},
    {'G', {0x0f, 0x10, 0x10, 0x17, 0x11, 0x11, 0x0f}},
    {'H', {0x11, 0x11, 0x11, 0x1f, 0x11, 0x11, 0x11}},
    {'I', {0x1f, 0x04, 0x04, 0x04, 0x04, 0x04, 0x1f}},
    {'L', {0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1f}},
    {'M', {0x11, 0x1b, 0x15, 0x15, 0x11, 0x11, 0x11}},
    {'a', {0x00, 0x00, 0x0e, 0x01, 0x0f, 0x11, 0x0f}},
    {'d', {0x01, 0x01, 0x0f, 0x11, 0x11, 0x11, 0x0f}},
    {'e', {0x00, 0x00, 0x0e, 0x11, 0x1f, 0x10, 0x0e}},
    {'g', {0x00, 0x00, 0x0f, 0x11, 0x0f, 0x01, 0x0e}},
    {'h', {0x10, 0x10, 0x1e, 0x11, 0x11, 0x11, 0x11}},
    {'i', {0x04, 0x00, 0x0c, 0x04, 0x04, 0x04, 0x0e}},
    {'m', {0x00, 0x00, 0x1a, 0x15, 0x15, 0x15, 0x15}},
    {'n', {0x00, 0x00, 0x1e, 0x11, 0x11, 0x11, 0x11}},
    {'o', {0x00, 0x00, 0x0e, 0x11, 0x11, 0x11, 0x0e}},
    {'p', {0x00, 0x00, 0x1e, 0x11, 0x1e, 0x10, 0x10}},
    {'r', {0x00, 0x00, 0x16, 0x19, 0x10, 0x10, 0x10}},
    {'s', {0x00, 0x00, 0x0f, 0x10, 0x0e, 0x01, 0x1e}},
    {'t', {0x04, 0x04, 0x1f, 0x04, 0x04, 0x04, 0x03}},
    {'u', {0x00, 0x00, 0x11, 0x11, 0x11, 0x13, 0x0d}},
    {'y', {0x00, 0x00, 0x11, 0x11, 0x0f, 0x01, 0x0e}},
    {'N', {0x11, 0x19, 0x19, 0x15, 0x13, 0x13, 0x11}},
    {'O', {0x0e, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0e}},
    {'P', {0x1e, 0x11, 0x11, 0x1e, 0x10, 0x10, 0x10}},
    {'R', {0x1e, 0x11, 0x11, 0x1e, 0x14, 0x12, 0x11}},
    {'S', {0x0f, 0x10, 0x10, 0x0e, 0x01, 0x01, 0x1e}},
    {'T', {0x1f, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04}},
    {'U', {0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0e}},
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
    _Float16 x;
    _Float16 y;
    _Float16 z;
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

static void blend_pixel(
    ANativeWindow_Buffer *buffer,
    int32_t x,
    int32_t y,
    uint32_t value,
    uint8_t coverage)
{
    if (x < 0 || y < 0 || x >= buffer->width || y >= buffer->height || coverage == 0U) {
        return;
    }

    uint32_t *pixels = (uint32_t *)buffer->bits;
    size_t offset = (size_t)y * (size_t)buffer->stride + (size_t)x;
    if (coverage == 255U) {
        pixels[offset] = value;
        return;
    }

    uint32_t under = pixels[offset];
    uint32_t inverse = 255U - (uint32_t)coverage;
    uint32_t mixed = 0U;
    for (unsigned int shift = 0U; shift < 32U; shift += 8U) {
        uint32_t under_channel = (under >> shift) & 0xffU;
        uint32_t value_channel = (value >> shift) & 0xffU;
        uint32_t channel =
            (under_channel * inverse + value_channel * (uint32_t)coverage + 127U) / 255U;
        mixed |= channel << shift;
    }
    pixels[offset] = mixed;
}

static bool glyph_cell_on(const uint8_t *rows, int32_t row, int32_t column)
{
    if (row < 0 || row >= 7 || column < 0 || column >= 5) {
        return false;
    }
    uint8_t mask = (uint8_t)(1U << (unsigned int)(4 - column));
    return (rows[row] & mask) != 0U;
}

/*
 * Keep the 5x7 bitmap as the glyph definition, but use the physical pixels
 * inside each scaled cell to soften exposed corners. A 4x4 subpixel coverage
 * grid gives grayscale antialiasing without assuming an RGB subpixel order.
 */
static uint8_t rounded_corner_coverage(int32_t x, int32_t y, int32_t scale)
{
    if (scale < 3) {
        return 255U;
    }

    int32_t radius = scale / 2;
    if (radius < 1 || x >= radius || y >= radius) {
        return 255U;
    }

    int32_t radius_eighths = radius * 8;
    int32_t radius_squared = radius_eighths * radius_eighths;
    int32_t covered = 0;

    for (int32_t sample_y = 1; sample_y < 8; sample_y += 2) {
        for (int32_t sample_x = 1; sample_x < 8; sample_x += 2) {
            int32_t x_eighths = x * 8 + sample_x;
            int32_t y_eighths = y * 8 + sample_y;
            int32_t dx = radius_eighths - x_eighths;
            int32_t dy = radius_eighths - y_eighths;
            if (dx * dx + dy * dy <= radius_squared) {
                covered += 1;
            }
        }
    }

    return (uint8_t)((covered * 255 + 8) / 16);
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
            if (!glyph_cell_on(rows, row, column)) {
                continue;
            }

            bool north = glyph_cell_on(rows, row - 1, column);
            bool south = glyph_cell_on(rows, row + 1, column);
            bool west = glyph_cell_on(rows, row, column - 1);
            bool east = glyph_cell_on(rows, row, column + 1);

            /*
             * Preserve a square corner when a diagonal cell touches it. The
             * original 5x7 font uses those diagonal contacts as real strokes.
             */
            bool round_top_left =
                !north && !west && !glyph_cell_on(rows, row - 1, column - 1);
            bool round_top_right =
                !north && !east && !glyph_cell_on(rows, row - 1, column + 1);
            bool round_bottom_left =
                !south && !west && !glyph_cell_on(rows, row + 1, column - 1);
            bool round_bottom_right =
                !south && !east && !glyph_cell_on(rows, row + 1, column + 1);

            for (int32_t pixel_y = 0; pixel_y < scale; ++pixel_y) {
                for (int32_t pixel_x = 0; pixel_x < scale; ++pixel_x) {
                    uint8_t coverage = 255U;

                    if (round_top_left) {
                        uint8_t corner =
                            rounded_corner_coverage(pixel_x, pixel_y, scale);
                        if (corner < coverage) {
                            coverage = corner;
                        }
                    }
                    if (round_top_right) {
                        uint8_t corner = rounded_corner_coverage(
                            scale - 1 - pixel_x,
                            pixel_y,
                            scale);
                        if (corner < coverage) {
                            coverage = corner;
                        }
                    }
                    if (round_bottom_left) {
                        uint8_t corner = rounded_corner_coverage(
                            pixel_x,
                            scale - 1 - pixel_y,
                            scale);
                        if (corner < coverage) {
                            coverage = corner;
                        }
                    }
                    if (round_bottom_right) {
                        uint8_t corner = rounded_corner_coverage(
                            scale - 1 - pixel_x,
                            scale - 1 - pixel_y,
                            scale);
                        if (corner < coverage) {
                            coverage = corner;
                        }
                    }

                    blend_pixel(
                        buffer,
                        left + column * scale + pixel_x,
                        top + row * scale + pixel_y,
                        value,
                        coverage);
                }
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

static int32_t text_width(const char *text, int32_t scale)
{
    size_t count = strlen(text);
    if (count == 0U) {
        return 0;
    }
    return ((int32_t)count * 6 - 1) * scale;
}

static void draw_text_centered(
    ANativeWindow_Buffer *buffer,
    const char *text,
    int32_t top,
    int32_t scale,
    uint32_t value)
{
    int32_t left = (buffer->width - text_width(text, scale)) / 2;
    draw_text(buffer, text, left, top, scale, value);
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
    const uint32_t track = 0xff505050U;
    clear_buffer(&buffer, background);

    int32_t scale = buffer.width / 180;
    if (scale < 2) {
        scale = 2;
    } else if (scale > 6) {
        scale = 6;
    }

    /*
     * The first phone-sized version used scale + 1, which is 4 on the MIRO A1.
     * Use roughly twice that size now, capped only so the longest title line
     * still fits on unusually narrow displays.
     */
    int32_t text_scale = 2 * (scale + 1);
    int32_t title_fit_scale = buffer.width / 60;
    if (text_scale > title_fit_scale) {
        text_scale = title_fit_scale;
    }
    if (text_scale < 2) {
        text_scale = 2;
    }

    int32_t title_scale = text_scale - 4;
    if (title_scale < 3) {
        title_scale = 3;
    }

    int32_t unit_scale = text_scale - scale;
    if (unit_scale < 3) {
        unit_scale = 3;
    }
    int32_t superscript_scale = unit_scale > 2 ? unit_scale - 1 : unit_scale;

    int32_t springs_scale = title_scale + 1;
    int32_t title_line_height = 15 * title_scale;
    int32_t reading_stride = buffer.height / 5;
    int32_t bar_height = 6 * scale;

    /*
     * Place the major vertical regions as proportions of screen height.
     * This keeps the composition balanced instead of accumulating small
     * scale-unit offsets that leave excess space at the bottom.
     */
    int32_t top = (9 * buffer.height) / 100;

    draw_text_centered(&buffer, "there are", top, title_scale, secondary);
    draw_text_centered(
        &buffer,
        "SPRINGS",
        top + title_line_height,
        springs_scale,
        foreground);
    draw_text_centered(
        &buffer,
        "inside",
        top + 2 * title_line_height,
        title_scale,
        secondary);
    draw_text_centered(
        &buffer,
        "your phone",
        top + 3 * title_line_height,
        title_scale,
        secondary);

    int32_t readings_top = (2 * buffer.height) / 5;

    if (state->accelerometer == NULL) {
        draw_text_centered(
            &buffer,
            "NO SENSOR",
            readings_top + reading_stride,
            text_scale,
            foreground);
        (void)ANativeWindow_unlockAndPost(state->app->window);
        return;
    }

    char values[3][16];
    (void)snprintf(values[0], sizeof(values[0]), "%+.1f", (double)state->x);
    (void)snprintf(values[1], sizeof(values[1]), "%+.1f", (double)state->y);
    (void)snprintf(values[2], sizeof(values[2]), "%+.1f", (double)state->z);

    /*
     * Treat the three reading fields like tab stops rather than one text run.
     * This leaves deliberate horizontal space and keeps all rows aligned even
     * as the signs and digits change.
     */
    const char axes[3] = {'X', 'Y', 'Z'};
    int32_t axis_left = 4 * scale;
    int32_t value_left = buffer.width / 4;
    int32_t unit_left = (7 * buffer.width) / 10;
    int32_t bar_margin = 2 * scale;
    int32_t center_x = buffer.width / 2;
    int32_t half_width = (buffer.width - 2 * bar_margin) / 2;
    int32_t track_height = 2 * scale;
    int32_t track_width = buffer.width - 2 * bar_margin;
    float accelerations[3] = {state->x, state->y, state->z};

    for (int32_t axis = 0; axis < 3; ++axis) {
        int32_t text_y = readings_top + axis * reading_stride;
        draw_glyph(
            &buffer,
            axes[axis],
            axis_left,
            text_y,
            text_scale,
            foreground);
        draw_text(
            &buffer,
            values[axis],
            value_left,
            text_y,
            text_scale,
            foreground);

        int32_t unit_y = text_y + 7 * (text_scale - unit_scale);
        draw_text(&buffer, "m/s", unit_left, unit_y, unit_scale, foreground);

        int32_t unit_end = unit_left + text_width("m/s", unit_scale) + unit_scale;
        int32_t superscript_raise = 2 * superscript_scale;
        draw_glyph(
            &buffer,
            '2',
            unit_end,
            unit_y - superscript_raise,
            superscript_scale,
            foreground);

        int32_t bar_y = text_y + 9 * text_scale;
        fill_rect(
            &buffer,
            bar_margin,
            bar_y + (bar_height - track_height) / 2,
            track_width,
            track_height,
            track);
        fill_rect(&buffer, center_x, bar_y, 1, bar_height, secondary);
        draw_axis_bar(
            &buffer,
            center_x,
            bar_y,
            half_width,
            bar_height,
            accelerations[axis],
            secondary);
    }

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

        state->x = (_Float16)event.acceleration.x;
        state->y = (_Float16)event.acceleration.y;
        state->z = (_Float16)event.acceleration.z;
        state->have_sample = true;

        state->log_counter += 1U;
        if (state->log_counter >= 25U) {
            __android_log_print(
                ANDROID_LOG_INFO,
                LOG_TAG,
                "x=%.2f y=%.2f z=%.2f m/s2",
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
