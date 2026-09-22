#ifndef ACCELEROMETER_MODEL_H
#define ACCELEROMETER_MODEL_H

#include "compact_acceleration.h"

/*
 * Instrument meaning above the Android boundary:
 *
 * Android accelerometer reading
 *     -> difference from balanced gravity
 *     -> compact geometric state
 *     -> reconstructed acceleration
 *     -> what the screen shows
 *
 * The first arrow is implemented by compact_acceleration_encode(). The model
 * intentionally retains only the compact geometric state; the Android reading
 * remains available to callers as a transient oracle rather than a shadow copy.
 */
struct accelerometer_model {
    struct compact_acceleration retained;
    bool have_android_sample;
};

static inline void accelerometer_model_initialize(struct accelerometer_model *model)
{
    struct physical_acceleration screen_placeholder = {0.0F, 0.0F, 0.0F};
    enum compact_acceleration_encode_status status =
        compact_acceleration_encode(screen_placeholder, &model->retained);
    model->have_android_sample = false;
    (void)status;
}

static inline enum compact_acceleration_encode_status accelerometer_model_accept(
    struct accelerometer_model *model,
    struct physical_acceleration android_reading)
{
    struct compact_acceleration candidate;
    enum compact_acceleration_encode_status status =
        compact_acceleration_encode(android_reading, &candidate);
    if (status != COMPACT_ACCELERATION_ENCODE_NONFINITE) {
        model->retained = candidate;
        model->have_android_sample = true;
    }
    return status;
}

static inline bool accelerometer_model_reconstruct(
    const struct accelerometer_model *model,
    struct physical_acceleration *reconstructed)
{
    return compact_acceleration_decode(&model->retained, reconstructed) ==
        COMPACT_ACCELERATION_DECODE_OK;
}

#endif
