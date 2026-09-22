#include "accelerometer_snapshot.h"

#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static int failures = 0;

static void check(bool condition, const char *message)
{
    if (!condition) {
        (void)fprintf(stderr, "FAIL: %s\n", message);
        failures += 1;
    }
}

static bool near(float first, float second, float tolerance)
{
    return fabsf(first - second) <= tolerance;
}

static void test_complete_snapshot(void)
{
    struct physical_acceleration android_reading = {1.25F, -2.5F, 9.0F};
    struct accelerometer_snapshot snapshot;
    enum accelerometer_snapshot_status status =
        accelerometer_snapshot_make(android_reading, &snapshot);

    check(status == ACCELEROMETER_SNAPSHOT_OK, "ordinary reading produces a complete snapshot");
    check(
        snapshot.android_reading.x == android_reading.x &&
            snapshot.android_reading.y == android_reading.y &&
            snapshot.android_reading.z == android_reading.z,
        "snapshot preserves the transient Android reading");

    struct physical_acceleration expected_difference =
        acceleration_difference_from_balanced_gravity(android_reading);
    check(
        snapshot.difference_from_balanced_gravity.x == expected_difference.x &&
            snapshot.difference_from_balanced_gravity.y == expected_difference.y &&
            snapshot.difference_from_balanced_gravity.z == expected_difference.z,
        "snapshot uses the shared balanced-gravity difference");
    check(
        near(
            snapshot.difference_magnitude,
            physical_acceleration_magnitude(expected_difference),
            1.0e-6F),
        "snapshot exposes the difference magnitude");

    struct compact_acceleration direct_compact;
    enum compact_acceleration_encode_status direct_status =
        compact_acceleration_encode(android_reading, &direct_compact);
    check(
        direct_status == snapshot.encode_status,
        "snapshot reports the compact encoder status");
    check(
        memcmp(
            &direct_compact,
            &snapshot.compact_state,
            sizeof(direct_compact)) == 0,
        "snapshot exposes exactly the retained compact bytes");

    check(snapshot.compact_direction_defined, "ordinary reading has a compact residual direction");
    if (snapshot.compact_direction_defined) {
        float norm = physical_acceleration_magnitude(snapshot.compact_direction);
        check(near(norm, 1.0F, 2.0e-6F), "compact residual direction is unit length");
    }

    struct physical_acceleration direct_reconstruction;
    check(
        compact_acceleration_decode(&direct_compact, &direct_reconstruction) ==
            COMPACT_ACCELERATION_DECODE_OK,
        "direct compact state decodes");
    check(
        snapshot.reconstructed.x == direct_reconstruction.x &&
            snapshot.reconstructed.y == direct_reconstruction.y &&
            snapshot.reconstructed.z == direct_reconstruction.z,
        "snapshot reconstruction is the model reconstruction");

    struct sevenths_display_value expected_x;
    struct sevenths_display_value expected_y;
    struct sevenths_display_value expected_z;
    check(
        sevenths_display_quantize(direct_reconstruction.x, &expected_x) &&
            sevenths_display_quantize(direct_reconstruction.y, &expected_y) &&
            sevenths_display_quantize(direct_reconstruction.z, &expected_z),
        "direct reconstruction quantizes for the screen");
    check(
        snapshot.screen_x.negative == expected_x.negative &&
            snapshot.screen_x.whole == expected_x.whole &&
            snapshot.screen_x.numerator == expected_x.numerator &&
            snapshot.screen_y.negative == expected_y.negative &&
            snapshot.screen_y.whole == expected_y.whole &&
            snapshot.screen_y.numerator == expected_y.numerator &&
            snapshot.screen_z.negative == expected_z.negative &&
            snapshot.screen_z.whole == expected_z.whole &&
            snapshot.screen_z.numerator == expected_z.numerator,
        "snapshot exposes exactly what the screen quantizer sees");
}

static void test_balanced_gravity_has_no_direction(void)
{
    struct accelerometer_snapshot snapshot;
    struct physical_acceleration balanced =
        compact_acceleration_balanced_reference();
    enum accelerometer_snapshot_status status =
        accelerometer_snapshot_make(balanced, &snapshot);

    check(status == ACCELEROMETER_SNAPSHOT_OK, "balanced gravity produces a snapshot");
    check(
        snapshot.difference_magnitude == 0.0F,
        "balanced gravity has zero difference magnitude");
    check(
        !snapshot.compact_direction_defined,
        "zero residual does not invent a unit quaternion direction");
    check(
        snapshot.compact_state.direction_low == 0U &&
            snapshot.compact_state.direction_middle == 0U &&
            snapshot.compact_state.direction_high == 0U &&
            snapshot.compact_state.magnitude == 0U,
        "balanced gravity uses the canonical compact zero");
}

static void test_nonfinite_does_not_publish_snapshot(void)
{
    struct accelerometer_snapshot snapshot;
    (void)memset(&snapshot, 0x5a, sizeof(snapshot));
    struct accelerometer_snapshot before = snapshot;

    enum accelerometer_snapshot_status status =
        accelerometer_snapshot_make(
            (struct physical_acceleration){NAN, 0.0F, 0.0F},
            &snapshot);

    check(
        status == ACCELEROMETER_SNAPSHOT_NONFINITE,
        "nonfinite Android reading is rejected");
    check(
        memcmp(&snapshot, &before, sizeof(snapshot)) == 0,
        "rejected reading does not publish a partial snapshot");
}

int main(void)
{
    test_complete_snapshot();
    test_balanced_gravity_has_no_direction();
    test_nonfinite_does_not_publish_snapshot();

    if (failures != 0) {
        (void)fprintf(stderr, "%d accelerometer snapshot test(s) failed\n", failures);
        return 1;
    }

    (void)printf("PASS shared accelerometer snapshot semantics\n");
    return 0;
}
