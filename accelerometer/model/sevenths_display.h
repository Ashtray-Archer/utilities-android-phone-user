#ifndef SEVENTHS_DISPLAY_H
#define SEVENTHS_DISPLAY_H

#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

struct sevenths_display_value {
    bool negative;
    uint8_t whole;
    uint8_t numerator;
};

static inline bool sevenths_display_quantize(
    float acceleration,
    struct sevenths_display_value *display)
{
    /* Every component reconstructed by the compact codec lies inside this dyadic bound. */
    if (!isfinite(acceleration) || acceleration < -64.0F || acceleration > 64.0F) {
        return false;
    }

    int32_t sevenths = (int32_t)roundf(7.0F * acceleration);
    display->negative = sevenths < 0;
    uint32_t magnitude = (uint32_t)(display->negative ? -sevenths : sevenths);
    display->whole = (uint8_t)(magnitude / 7U);
    display->numerator = (uint8_t)(magnitude % 7U);
    return true;
}

static inline bool sevenths_display_format_ascii(
    const struct sevenths_display_value *display,
    char *text,
    size_t capacity)
{
    char sign = display->negative ? '-' : '+';
    int written;
    if (display->numerator == 0U) {
        written = snprintf(text, capacity, "%c%u", sign, (unsigned int)display->whole);
    } else if (display->whole == 0U) {
        written = snprintf(
            text,
            capacity,
            "%c%u/7",
            sign,
            (unsigned int)display->numerator);
    } else {
        written = snprintf(
            text,
            capacity,
            "%c%u %u/7",
            sign,
            (unsigned int)display->whole,
            (unsigned int)display->numerator);
    }
    return written >= 0 && (size_t)written < capacity;
}

#endif
