#ifndef ACCELEROMETER_INSPECTION_H
#define ACCELEROMETER_INSPECTION_H

#include "accelerometer_snapshot.h"

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>

static inline const char *accelerometer_encode_status_text(
    enum compact_acceleration_encode_status status)
{
    switch (status) {
        case COMPACT_ACCELERATION_ENCODE_OK:
            return "ok";
        case COMPACT_ACCELERATION_ENCODE_SATURATED:
            return "saturated";
        case COMPACT_ACCELERATION_ENCODE_NONFINITE:
            return "nonfinite";
        default:
            return "unknown";
    }
}

/*
 * Write one complete human-readable and machine-selectable accelerometer record.
 *
 * Section headings explain the meaning to a person. Dotted keys remain stable
 * for Grease and other command-line consumers.
 */
static inline int accelerometer_inspection_write(
    FILE *out,
    FILE *error,
    int64_t timestamp_ns,
    struct physical_acceleration android_reading,
    long record_index)
{
    struct accelerometer_snapshot snapshot;
    enum accelerometer_snapshot_status status =
        accelerometer_snapshot_make(android_reading, &snapshot);

    if (status == ACCELEROMETER_SNAPSHOT_NONFINITE) {
        (void)fprintf(error, "Android accelerometer reading contains a nonfinite value\n");
        return -1;
    }
    if (status == ACCELEROMETER_SNAPSHOT_MALFORMED) {
        (void)fprintf(error, "compact accelerometer state could not be reconstructed\n");
        return -1;
    }
    if (status == ACCELEROMETER_SNAPSHOT_DISPLAY_UNAVAILABLE) {
        (void)fprintf(error, "reconstructed accelerometer value cannot be shown as sevenths\n");
        return -1;
    }

    struct physical_acceleration balanced =
        compact_acceleration_balanced_reference();
    char screen_x[16];
    char screen_y[16];
    char screen_z[16];
    if (!sevenths_display_format_ascii(&snapshot.screen_x, screen_x, sizeof(screen_x)) ||
        !sevenths_display_format_ascii(&snapshot.screen_y, screen_y, sizeof(screen_y)) ||
        !sevenths_display_format_ascii(&snapshot.screen_z, screen_z, sizeof(screen_z))) {
        (void)fprintf(error, "could not format the screen values\n");
        return -1;
    }

    if (record_index >= 0) {
        (void)fprintf(out, "record.index\t%ld\n", record_index);
    }

    (void)fprintf(out, "Android accelerometer reading\n");
    (void)fprintf(out, "android.timestamp_ns\t%" PRId64 "\n", timestamp_ns);
    (void)fprintf(out, "android.x_m_per_s2\t%.9g\n", snapshot.android_reading.x);
    (void)fprintf(out, "android.y_m_per_s2\t%.9g\n", snapshot.android_reading.y);
    (void)fprintf(out, "android.z_m_per_s2\t%.9g\n", snapshot.android_reading.z);

    (void)fprintf(out, "\nDifference from balanced gravity\n");
    (void)fprintf(out, "balanced_gravity.x_m_per_s2\t%.9g\n", balanced.x);
    (void)fprintf(out, "balanced_gravity.y_m_per_s2\t%.9g\n", balanced.y);
    (void)fprintf(out, "balanced_gravity.z_m_per_s2\t%.9g\n", balanced.z);
    (void)fprintf(
        out,
        "difference.x_m_per_s2\t%.9g\n",
        snapshot.difference_from_balanced_gravity.x);
    (void)fprintf(
        out,
        "difference.y_m_per_s2\t%.9g\n",
        snapshot.difference_from_balanced_gravity.y);
    (void)fprintf(
        out,
        "difference.z_m_per_s2\t%.9g\n",
        snapshot.difference_from_balanced_gravity.z);
    (void)fprintf(
        out,
        "difference.magnitude_m_per_s2\t%.9g\n",
        snapshot.difference_magnitude);

    (void)fprintf(
        out,
        "\nResidual direction represented by compact state (unit pure quaternion)\n");
    (void)fprintf(
        out,
        "residual_direction.defined\t%s\n",
        snapshot.compact_direction_defined ? "true" : "false");
    if (snapshot.compact_direction_defined) {
        (void)fprintf(out, "residual_direction.quaternion.real\t0\n");
        (void)fprintf(
            out,
            "residual_direction.quaternion.i\t%.9g\n",
            snapshot.compact_direction.x);
        (void)fprintf(
            out,
            "residual_direction.quaternion.j\t%.9g\n",
            snapshot.compact_direction.y);
        (void)fprintf(
            out,
            "residual_direction.quaternion.k\t%.9g\n",
            snapshot.compact_direction.z);
    } else {
        (void)fprintf(out, "residual_direction.quaternion.real\tundefined\n");
        (void)fprintf(out, "residual_direction.quaternion.i\tundefined\n");
        (void)fprintf(out, "residual_direction.quaternion.j\tundefined\n");
        (void)fprintf(out, "residual_direction.quaternion.k\tundefined\n");
    }

    (void)fprintf(out, "\nCompact geometric state\n");
    (void)fprintf(
        out,
        "compact.direction_bytes_hex\t%02x%02x%02x\n",
        (unsigned int)snapshot.compact_state.direction_high,
        (unsigned int)snapshot.compact_state.direction_middle,
        (unsigned int)snapshot.compact_state.direction_low);
    (void)fprintf(
        out,
        "compact.magnitude_code\t%u\n",
        (unsigned int)snapshot.compact_state.magnitude);
    (void)fprintf(
        out,
        "compact.residual_magnitude_m_per_s2\t%.9g\n",
        (float)snapshot.compact_state.magnitude *
            compact_acceleration_magnitude_quantum());
    (void)fprintf(
        out,
        "compact.encode_status\t%s\n",
        accelerometer_encode_status_text(snapshot.encode_status));

    (void)fprintf(out, "\nReconstructed from compact state\n");
    (void)fprintf(out, "reconstructed.x_m_per_s2\t%.9g\n", snapshot.reconstructed.x);
    (void)fprintf(out, "reconstructed.y_m_per_s2\t%.9g\n", snapshot.reconstructed.y);
    (void)fprintf(out, "reconstructed.z_m_per_s2\t%.9g\n", snapshot.reconstructed.z);

    (void)fprintf(out, "\nWhat the screen shows\n");
    (void)fprintf(out, "screen.x_m_per_s2\t%s\n", screen_x);
    (void)fprintf(out, "screen.y_m_per_s2\t%s\n", screen_y);
    (void)fprintf(out, "screen.z_m_per_s2\t%s\n", screen_z);

    return 0;
}

#endif
