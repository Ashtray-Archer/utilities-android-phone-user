#ifndef ACCELEROMETER_SNAPSHOT_H
#define ACCELEROMETER_SNAPSHOT_H

#include "accelerometer_model.h"
#include "sevenths_display.h"

#include <stdbool.h>

struct accelerometer_snapshot {
    struct physical_acceleration android_reading;
    struct physical_acceleration difference_from_balanced_gravity;
    float difference_magnitude;

    struct compact_acceleration compact_state;
    bool compact_direction_defined;
    struct physical_acceleration compact_direction;

    struct physical_acceleration reconstructed;
    struct sevenths_display_value screen_x;
    struct sevenths_display_value screen_y;
    struct sevenths_display_value screen_z;

    enum compact_acceleration_encode_status encode_status;
};

enum accelerometer_snapshot_status {
    ACCELEROMETER_SNAPSHOT_OK = 0,
    ACCELEROMETER_SNAPSHOT_SATURATED,
    ACCELEROMETER_SNAPSHOT_NONFINITE,
    ACCELEROMETER_SNAPSHOT_MALFORMED,
    ACCELEROMETER_SNAPSHOT_DISPLAY_UNAVAILABLE
};

/*
 * Build every human-meaningful view of one Android accelerometer reading
 * through the same model used by the graphical instrument.
 *
 * Android accelerometer reading
 *     -> difference from balanced gravity
 *     -> compact geometric state
 *     -> residual direction represented as a unit pure quaternion
 *        (real part zero; i/j/k are compact_direction x/y/z)
 *     -> reconstructed acceleration
 *     -> what the screen shows
 *
 * This does not claim a full phone-orientation quaternion. The unit pure
 * quaternion describes the residual direction retained by the compact state.
 */
static inline enum accelerometer_snapshot_status accelerometer_snapshot_make(
    struct physical_acceleration android_reading,
    struct accelerometer_snapshot *snapshot)
{
    struct accelerometer_snapshot candidate = {0};
    candidate.android_reading = android_reading;
    candidate.difference_from_balanced_gravity =
        acceleration_difference_from_balanced_gravity(android_reading);
    candidate.difference_magnitude =
        physical_acceleration_magnitude(candidate.difference_from_balanced_gravity);

    struct accelerometer_model model;
    accelerometer_model_initialize(&model);
    candidate.encode_status = accelerometer_model_accept(&model, android_reading);
    if (candidate.encode_status == COMPACT_ACCELERATION_ENCODE_NONFINITE) {
        return ACCELEROMETER_SNAPSHOT_NONFINITE;
    }

    candidate.compact_state = model.retained;
    candidate.compact_direction_defined = candidate.compact_state.magnitude != 0U;
    if (candidate.compact_direction_defined) {
        compact_acceleration_decode_direction(
            &candidate.compact_state,
            &candidate.compact_direction);
    }

    if (!accelerometer_model_reconstruct(&model, &candidate.reconstructed)) {
        return ACCELEROMETER_SNAPSHOT_MALFORMED;
    }

    if (!sevenths_display_quantize(candidate.reconstructed.x, &candidate.screen_x) ||
        !sevenths_display_quantize(candidate.reconstructed.y, &candidate.screen_y) ||
        !sevenths_display_quantize(candidate.reconstructed.z, &candidate.screen_z)) {
        return ACCELEROMETER_SNAPSHOT_DISPLAY_UNAVAILABLE;
    }

    *snapshot = candidate;
    return candidate.encode_status == COMPACT_ACCELERATION_ENCODE_SATURATED
        ? ACCELEROMETER_SNAPSHOT_SATURATED
        : ACCELEROMETER_SNAPSHOT_OK;
}

#endif
