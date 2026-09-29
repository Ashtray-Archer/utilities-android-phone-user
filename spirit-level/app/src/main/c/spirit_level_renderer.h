#ifndef SPIRIT_LEVEL_RENDERER_H
#define SPIRIT_LEVEL_RENDERER_H

#include "spirit_level_geometry.h"

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* This same RGBA8888 renderer runs in a native window and the host tests. */
struct spirit_level_surface {
    uint32_t *pixels;
    int width;
    int height;
    int stride;
};

struct spirit_level_segment {
    int first_x;
    int first_y;
    int last_x;
    int last_y;
};

enum { SPIRIT_LEVEL_FOOTER_HEIGHT = 24 };
#define SPIRIT_LEVEL_BACKGROUND UINT32_C(0xff101010)
#define SPIRIT_LEVEL_GUIDE UINT32_C(0xff606060)
#define SPIRIT_LEVEL_LINE UINT32_C(0xffeeeeee)
#define SPIRIT_LEVEL_MARKER UINT32_C(0xff70e0c0)

static inline bool spirit_level_surface_valid(struct spirit_level_surface surface)
{
    /* Bound integer raster arithmetic; caller owns stride*height pixels. */
    return surface.pixels != NULL &&
        surface.width >= 32 && surface.width <= 16384 &&
        surface.height >= 48 && surface.height <= 16384 &&
        surface.stride >= surface.width && surface.stride <= 16384;
}

static inline void spirit_level_pixel(
    struct spirit_level_surface surface, int x, int y, uint32_t color)
{
    if (x >= 0 && x < surface.width && y >= 0 && y < surface.height) {
        surface.pixels[(size_t)y * (size_t)surface.stride + (size_t)x] = color;
    }
}

static inline void spirit_level_line(
    struct spirit_level_surface surface,
    struct spirit_level_segment segment,
    uint32_t color)
{
    int dx = abs(segment.last_x - segment.first_x);
    int dy = -abs(segment.last_y - segment.first_y);
    int step_x = segment.first_x < segment.last_x ? 1 : -1;
    int step_y = segment.first_y < segment.last_y ? 1 : -1;
    int error = dx + dy;
    for (;;) {
        spirit_level_pixel(surface, segment.first_x, segment.first_y, color);
        if (segment.first_x == segment.last_x && segment.first_y == segment.last_y) {
            break;
        }
        int twice_error = 2 * error;
        if (twice_error >= dy) {
            error += dy;
            segment.first_x += step_x;
        }
        if (twice_error <= dx) {
            error += dx;
            segment.first_y += step_y;
        }
    }
}

/* Clip in pixels, not a square normalized viewport: preserve physical angles
 * on portrait, landscape, and non-square screens. Footer is never in the clip.
 */
static inline struct spirit_level_segment spirit_level_segment_make(
    int width, int height, const struct spirit_level_geometry *geometry)
{
    float center_x = (float)(width - 1) / 2.0F;
    float center_y = (float)(height - SPIRIT_LEVEL_FOOTER_HEIGHT - 1) / 2.0F;
    float half_width = center_x - 4.0F;
    float half_height = center_y - 4.0F;
    float right = geometry->line_right;
    float down = geometry->line_down;
    float extent = hypotf(half_width, half_height);
    if (fabsf(right) > 0.0F) {
        extent = fminf(extent, half_width / fabsf(right));
    }
    if (fabsf(down) > 0.0F) {
        extent = fminf(extent, half_height / fabsf(down));
    }
    return (struct spirit_level_segment){
        (int)lroundf(center_x - extent * right),
        (int)lroundf(center_y - extent * down),
        (int)lroundf(center_x + extent * right),
        (int)lroundf(center_y + extent * down)};
}

struct spirit_level_glyph { char character; uint8_t rows[7]; };
static const struct spirit_level_glyph spirit_level_glyphs[] = {
    {' ', {0,0,0,0,0,0,0}},
    {'+', {0,4,4,31,4,4,0}}, {'-', {0,0,0,31,0,0,0}},
    {'.', {0,0,0,0,0,12,12}},
    {'0', {14,17,19,21,25,17,14}}, {'1', {4,12,20,4,4,4,31}},
    {'2', {14,17,1,2,4,8,31}}, {'3', {30,1,1,14,1,1,30}},
    {'4', {2,6,10,18,31,2,2}}, {'5', {31,16,16,30,1,1,30}},
    {'6', {14,16,16,30,17,17,14}}, {'7', {31,1,2,4,8,8,8}},
    {'8', {14,17,17,14,17,17,14}}, {'9', {14,17,17,15,1,1,14}},
    {'A', {14,17,17,31,17,17,17}}, {'C', {15,16,16,16,16,16,15}},
    {'D', {30,17,17,17,17,17,30}}, {'E', {31,16,16,30,16,16,31}},
    {'F', {31,16,16,30,16,16,16}}, {'G', {15,16,16,23,17,17,15}},
    {'I', {31,4,4,4,4,4,31}}, {'L', {16,16,16,16,16,16,31}},
    {'N', {17,25,25,21,19,19,17}}, {'O', {14,17,17,17,17,17,14}},
    {'P', {30,17,17,30,16,16,16}}, {'R', {30,17,17,30,20,18,17}},
    {'S', {15,16,16,14,1,1,30}}, {'T', {31,4,4,4,4,4,4}},
    {'U', {17,17,17,17,17,17,14}}, {'V', {17,17,17,17,17,10,4}},
    {'W', {17,17,17,21,21,21,10}},
    {'X', {17,17,10,4,10,17,17}}, {'Y', {17,17,10,4,4,4,4}},
    {'Z', {31,1,2,4,8,16,31}}, {'?', {14,17,1,2,4,0,4}}
};

static inline void spirit_level_text(
    struct spirit_level_surface surface, const char *text, int top)
{
    size_t glyph_count = sizeof(spirit_level_glyphs) / sizeof(spirit_level_glyphs[0]);
    for (int left = 4; *text != '\0' && left + 5 < surface.width; ++text, left += 6) {
        const uint8_t *rows = spirit_level_glyphs[glyph_count - 1U].rows;
        for (size_t index = 0; index < glyph_count; ++index) {
            if (spirit_level_glyphs[index].character == *text) {
                rows = spirit_level_glyphs[index].rows;
                break;
            }
        }
        for (int row = 0; row < 7; ++row) {
            for (int column = 0; column < 5; ++column) {
                if ((rows[row] & (1U << (4 - column))) != 0U) {
                    spirit_level_pixel(surface, left + column, top + row, SPIRIT_LEVEL_LINE);
                }
            }
        }
    }
}

/* Only formatted diagnostic text survives the raw Android callback. It is not
 * parsed back, normalized, quantized through the codec, or used for geometry.
 * %G bounds even FLT_MAX/subnormal strings while keeping all three axes visible.
 */
static inline void spirit_level_diagnostics(
    char text[64], float raw_x, float raw_y, float raw_z)
{
    (void)snprintf(text, 64, "X %+.4G Y %+.4G Z %+.4G",
        (double)raw_x, (double)raw_y, (double)raw_z);
}

static inline const char *spirit_level_status_text(
    const struct spirit_level_geometry *geometry)
{
    switch (geometry->status) {
        case SPIRIT_LEVEL_TRACKING: return geometry->face_up ? "FACE UP" : "FACE DOWN";
        case SPIRIT_LEVEL_FLAT: return geometry->face_up ? "FLAT FACE UP" : "FLAT FACE DOWN";
        case SPIRIT_LEVEL_WAITING: return "WAITING";
        case SPIRIT_LEVEL_NO_GRAVITY: return "NO GRAVITY DIRECTION";
        case SPIRIT_LEVEL_SATURATED: return "OUT OF RANGE";
        case SPIRIT_LEVEL_INVALID: return "INVALID READING";
        case SPIRIT_LEVEL_NO_ROTATION: return "NO DISPLAY ROTATION";
        case SPIRIT_LEVEL_NO_SENSOR: return "NO SENSOR";
        default: return "INVALID READING";
    }
}

static inline bool spirit_level_draw(
    struct spirit_level_surface surface,
    const struct spirit_level_geometry *geometry,
    const char *diagnostics)
{
    if (!spirit_level_surface_valid(surface)) {
        return false;
    }
    for (int y = 0; y < surface.height; ++y) {
        for (int x = 0; x < surface.width; ++x) {
            spirit_level_pixel(surface, x, y, SPIRIT_LEVEL_BACKGROUND);
        }
    }
    struct spirit_level_segment segment =
        spirit_level_segment_make(surface.width, surface.height, geometry);
    bool have_tilt = spirit_level_has_tilt(geometry->status);
    spirit_level_line(surface, segment,
        geometry->status == SPIRIT_LEVEL_TRACKING ? SPIRIT_LEVEL_LINE : SPIRIT_LEVEL_GUIDE);
    if (have_tilt) {
        float center_x = (float)(surface.width - 1) / 2.0F;
        float center_y = (float)(surface.height - SPIRIT_LEVEL_FOOTER_HEIGHT - 1) / 2.0F;
        float radius = fminf(center_x, center_y) - 10.0F;
        int marker_x = (int)lroundf(center_x + radius * geometry->marker_right);
        int marker_y = (int)lroundf(center_y + radius * geometry->marker_down);
        int cx = (int)lroundf(center_x);
        int cy = (int)lroundf(center_y);
        spirit_level_line(surface, (struct spirit_level_segment){cx-4, cy, cx+4, cy}, SPIRIT_LEVEL_GUIDE);
        spirit_level_line(surface, (struct spirit_level_segment){cx, cy-4, cx, cy+4}, SPIRIT_LEVEL_GUIDE);
        for (int y = -3; y <= 3; ++y) {
            for (int x = -3; x <= 3; ++x) {
                /* Filled face-up marker; outlined face-down marker. */
                if (geometry->face_up || abs(x) == 3 || abs(y) == 3) {
                    spirit_level_pixel(surface, marker_x+x, marker_y+y, SPIRIT_LEVEL_MARKER);
                }
            }
        }
    }
    spirit_level_text(surface, spirit_level_status_text(geometry), surface.height - 21);
    spirit_level_text(surface, diagnostics, surface.height - 10);
    return true;
}

#endif
