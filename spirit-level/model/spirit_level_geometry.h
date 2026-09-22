#ifndef SPIRIT_LEVEL_GEOMETRY_H
#define SPIRIT_LEVEL_GEOMETRY_H

#include "accelerometer_model.h"

/* No Android API, raw-reading argument, or independently retained x/y/z here. */
enum spirit_level_status {
    SPIRIT_LEVEL_TRACKING,
    SPIRIT_LEVEL_FLAT,
    SPIRIT_LEVEL_WAITING,
    SPIRIT_LEVEL_NO_GRAVITY,
    SPIRIT_LEVEL_SATURATED,
    SPIRIT_LEVEL_INVALID,
    SPIRIT_LEVEL_NO_ROTATION,
    SPIRIT_LEVEL_NO_SENSOR
};

struct spirit_level_geometry {
    enum spirit_level_status status;
    /* Pixel coordinates use right/down; these directions have unit length. */
    float line_right;
    float line_down;
    float marker_right;
    float marker_down;
    float inclination_radians; /* 0 = face up, pi/2 = edge on, pi = face down. */
    bool face_up;
};

static inline bool spirit_level_has_tilt(enum spirit_level_status status)
{
    return status == SPIRIT_LEVEL_TRACKING || status == SPIRIT_LEVEL_FLAT;
}

/* Surface.ROTATION_* is a quarter turn from the device's natural orientation.
 * Android sensor axes do not follow display rotation. Return screen right/up.
 */
static inline bool spirit_level_screen_axes(
    struct physical_acceleration decoded,
    int display_rotation,
    float *right,
    float *up)
{
    switch (display_rotation) {
        case 0: *right = decoded.x;  *up = decoded.y;  return true;
        case 1: *right = decoded.y;  *up = -decoded.x; return true;
        case 2: *right = -decoded.x; *up = -decoded.y; return true;
        case 3: *right = -decoded.y; *up = decoded.x;  return true;
        default: return false;
    }
}

static inline struct spirit_level_geometry spirit_level_geometry_make(
    const struct accelerometer_model *model,
    enum compact_acceleration_encode_status last_status,
    int display_rotation)
{
    /* A neutral guide, never a claimed measurement when tilt is unavailable. */
    struct spirit_level_geometry result = {
        .status = SPIRIT_LEVEL_WAITING,
        .line_right = 1.0F,
        .face_up = true};

    if (last_status == COMPACT_ACCELERATION_ENCODE_NONFINITE) {
        result.status = SPIRIT_LEVEL_INVALID;
        return result;
    }
    if (!model->have_android_sample) {
        return result;
    }
    if (last_status == COMPACT_ACCELERATION_ENCODE_SATURATED) {
        result.status = SPIRIT_LEVEL_SATURATED;
        return result;
    }
    if (last_status != COMPACT_ACCELERATION_ENCODE_OK) {
        result.status = SPIRIT_LEVEL_INVALID;
        return result;
    }

    struct physical_acceleration decoded;
    if (!accelerometer_model_reconstruct(model, &decoded)) {
        result.status = SPIRIT_LEVEL_INVALID;
        return result;
    }
    float magnitude = physical_acceleration_magnitude(decoded);
    /* This is a display guard, not a free-fall detector or a calibration claim. */
    if (!isfinite(magnitude) || magnitude <= 0.5F) {
        result.status = SPIRIT_LEVEL_NO_GRAVITY;
        return result;
    }

    float right;
    float up;
    if (!spirit_level_screen_axes(decoded, display_rotation, &right, &up)) {
        result.status = SPIRIT_LEVEL_NO_ROTATION;
        return result;
    }
    float in_screen = hypotf(right, up);
    result.marker_right = right / magnitude;
    result.marker_down = -up / magnitude;
    result.inclination_radians = atan2f(in_screen, decoded.z);
    result.face_up = decoded.z >= 0.0F;

    /* At a pole, every in-screen direction is level. Do not magnify codec
     * noise into a spinning line. One magnitude quantum is a display deadband,
     * not a promised angular accuracy. The marker still uses the decoded tilt.
     */
    if (in_screen <= compact_acceleration_magnitude_quantum()) {
        result.status = SPIRIT_LEVEL_FLAT;
        return result;
    }
    result.status = SPIRIT_LEVEL_TRACKING;
    /* (up,right) is perpendicular to screen support acceleration (right,-up).
     * Use a direction, not a slope: vertical lines never divide by right/up.
     */
    result.line_right = up / in_screen;
    result.line_down = right / in_screen;
    return result;
}

#endif
