#include "accelerometer_model.h"
#include "sevenths_display.h"

#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static int failure_count = 0;

static void check(bool condition, const char *description)
{
    if (!condition) {
        (void)fprintf(stderr, "FAIL: %s\n", description);
        failure_count += 1;
    }
}

static bool near(float first, float second, float tolerance)
{
    return fabsf(first - second) <= tolerance;
}

static float norm(struct physical_acceleration value)
{
    return sqrtf(value.x * value.x + value.y * value.y + value.z * value.z);
}

static struct physical_acceleration subtract(
    struct physical_acceleration first,
    struct physical_acceleration second)
{
    return (struct physical_acceleration){
        first.x - second.x,
        first.y - second.y,
        first.z - second.z};
}

static void test_balanced_reference(void)
{
    struct physical_acceleration reference = compact_acceleration_balanced_reference();
    struct compact_acceleration encoded = {1U, 2U, 3U, 4U};
    enum compact_acceleration_encode_status encode_status =
        compact_acceleration_encode(reference, &encoded);
    check(encode_status == COMPACT_ACCELERATION_ENCODE_OK, "balanced reference encodes");
    check(
        memcmp(
            &encoded,
            &(struct compact_acceleration){0U, 0U, 0U, 0U},
            sizeof(encoded)) == 0,
        "balanced reference has canonical zero encoding");

    struct physical_acceleration decoded = {0.0F, 0.0F, 0.0F};
    check(
        compact_acceleration_decode(&encoded, &decoded) == COMPACT_ACCELERATION_DECODE_OK,
        "balanced reference decodes");
    check(
        decoded.x == reference.x && decoded.y == reference.y && decoded.z == reference.z,
        "balanced reference reconstructs exactly in the implementation arithmetic");
}

static void test_antipodal_gravity(void)
{
    struct physical_acceleration reference = compact_acceleration_balanced_reference();
    struct physical_acceleration antipode = {-reference.x, -reference.y, -reference.z};
    struct compact_acceleration encoded;
    enum compact_acceleration_encode_status status =
        compact_acceleration_encode(antipode, &encoded);
    check(status == COMPACT_ACCELERATION_ENCODE_OK, "antipodal gravity does not saturate");
    check(encoded.magnitude < 255U, "antipodal gravity fits the magnitude byte");

    struct physical_acceleration decoded = {0.0F, 0.0F, 0.0F};
    check(
        compact_acceleration_decode(&encoded, &decoded) == COMPACT_ACCELERATION_DECODE_OK,
        "antipodal gravity decodes");
    struct physical_acceleration error = subtract(decoded, antipode);
    check(
        fmaxf(fabsf(error.x), fmaxf(fabsf(error.y), fabsf(error.z))) < 1.0F / 7.0F,
        "antipodal gravity error is below one seventh");
}

static void test_axis_gravity_points(void)
{
    static const struct physical_acceleration gravity_points[] = {
        {10.0F, 0.0F, 0.0F},
        {-10.0F, 0.0F, 0.0F},
        {0.0F, 10.0F, 0.0F},
        {0.0F, -10.0F, 0.0F},
        {0.0F, 0.0F, 10.0F},
        {0.0F, 0.0F, -10.0F},
    };

    for (size_t index = 0U; index < sizeof(gravity_points) / sizeof(gravity_points[0]); ++index) {
        struct compact_acceleration encoded;
        struct physical_acceleration decoded = {0.0F, 0.0F, 0.0F};
        check(
            compact_acceleration_encode(gravity_points[index], &encoded) ==
                COMPACT_ACCELERATION_ENCODE_OK,
            "axis gravity point encodes");
        check(
            compact_acceleration_decode(&encoded, &decoded) == COMPACT_ACCELERATION_DECODE_OK,
            "axis gravity point decodes");
        struct physical_acceleration error = subtract(decoded, gravity_points[index]);
        check(
            fmaxf(fabsf(error.x), fmaxf(fabsf(error.y), fabsf(error.z))) < 1.0F / 7.0F,
            "axis gravity point round-trip is below one seventh");
    }
}

static void test_direction_sphere(void)
{
    float golden_angle = acosf(-1.0F) * (3.0F - sqrtf(5.0F));
    float maximum_component_error = 0.0F;
    for (uint32_t index = 0U; index < 131072U; ++index) {
        float unit_z = 1.0F - 2.0F * ((float)index + 0.5F) / 131072.0F;
        float radius = sqrtf(fmaxf(0.0F, 1.0F - unit_z * unit_z));
        float angle = (float)index * golden_angle;
        struct physical_acceleration expected = {
            radius * cosf(angle),
            radius * sinf(angle),
            unit_z};
        struct compact_acceleration encoded = compact_acceleration_canonical_zero();
        compact_acceleration_encode_direction(expected.x, expected.y, expected.z, &encoded);
        encoded.magnitude = 1U;

        struct physical_acceleration decoded = {0.0F, 0.0F, 0.0F};
        compact_acceleration_decode_direction(&encoded, &decoded);
        check(near(norm(decoded), 1.0F, 2.0e-6F), "decoded direction remains on S2");
        float component_error = fmaxf(
            fabsf(decoded.x - expected.x),
            fmaxf(fabsf(decoded.y - expected.y), fabsf(decoded.z - expected.z)));
        if (component_error > maximum_component_error) {
            maximum_component_error = component_error;
        }
    }
    check(
        maximum_component_error < 0.0011F,
        "Q0.11 octahedral direction stays within its declared component-error bound");
}

static void test_every_magnitude_code(void)
{
    struct physical_acceleration reference = compact_acceleration_balanced_reference();
    for (uint32_t magnitude = 0U; magnitude <= 255U; ++magnitude) {
        struct compact_acceleration encoded = compact_acceleration_canonical_zero();
        if (magnitude != 0U) {
            compact_acceleration_encode_direction(0.0F, 0.0F, 1.0F, &encoded);
            encoded.magnitude = (uint8_t)magnitude;
        }
        struct physical_acceleration decoded;
        check(
            compact_acceleration_decode(&encoded, &decoded) == COMPACT_ACCELERATION_DECODE_OK,
            "magnitude code decodes");
        float decoded_magnitude = norm(subtract(decoded, reference));
        float expected_magnitude =
            (float)magnitude / (4.0F * compact_acceleration_root_three());
        check(
            near(decoded_magnitude, expected_magnitude, 8.0e-6F),
            "magnitude code means m/(4 sqrt(3))");
    }
}

static void test_zero_overflow_nonfinite_and_malformed(void)
{
    struct physical_acceleration reference = compact_acceleration_balanced_reference();
    struct compact_acceleration tiny;
    struct physical_acceleration tiny_residual = {
        reference.x + compact_acceleration_magnitude_quantum() / 4.0F,
        reference.y,
        reference.z};
    check(
        compact_acceleration_encode(tiny_residual, &tiny) == COMPACT_ACCELERATION_ENCODE_OK,
        "sub-quantum residual encodes");
    check(
        memcmp(
            &tiny,
            &(struct compact_acceleration){0U, 0U, 0U, 0U},
            sizeof(tiny)) == 0,
        "zero magnitude has one deterministic encoding");

    float beyond_range = 256.0F * compact_acceleration_magnitude_quantum();
    struct physical_acceleration overflow = {
        reference.x + beyond_range,
        reference.y,
        reference.z};
    struct compact_acceleration saturated;
    check(
        compact_acceleration_encode(overflow, &saturated) ==
            COMPACT_ACCELERATION_ENCODE_SATURATED,
        "overflow reports saturation explicitly");
    check(saturated.magnitude == 255U, "overflow saturates instead of wrapping");

    struct compact_acceleration unchanged = {11U, 22U, 33U, 44U};
    struct compact_acceleration before = unchanged;
    check(
        compact_acceleration_encode(
            (struct physical_acceleration){NAN, 0.0F, 0.0F},
            &unchanged) == COMPACT_ACCELERATION_ENCODE_NONFINITE,
        "nonfinite Android input is rejected explicitly");
    check(
        memcmp(&unchanged, &before, sizeof(unchanged)) == 0,
        "nonfinite input leaves retained state untouched");

    struct compact_acceleration malformed = {1U, 0U, 0U, 0U};
    struct physical_acceleration output = {31.0F, 32.0F, 33.0F};
    check(
        compact_acceleration_decode(&malformed, &output) ==
            COMPACT_ACCELERATION_DECODE_NONCANONICAL_ZERO,
        "noncanonical zero is rejected as malformed");
    check(
        output.x == 31.0F && output.y == 32.0F && output.z == 33.0F,
        "malformed state does not fabricate a reconstruction");
}

static void test_screen_path_cannot_bypass_retained_state(void)
{
    struct accelerometer_model model;
    accelerometer_model_initialize(&model);
    struct physical_acceleration raw = {2.25F, -4.5F, 8.75F};
    check(
        accelerometer_model_accept(&model, raw) == COMPACT_ACCELERATION_ENCODE_OK,
        "model accepts finite Android sample");
    struct compact_acceleration retained_snapshot = model.retained;

    raw = (struct physical_acceleration){60.0F, 60.0F, 60.0F};
    struct physical_acceleration screen_value = {0.0F, 0.0F, 0.0F};
    struct physical_acceleration snapshot_value = {0.0F, 0.0F, 0.0F};
    check(
        accelerometer_model_reconstruct(&model, &screen_value),
        "screen model reconstructs retained state");
    check(
        compact_acceleration_decode(&retained_snapshot, &snapshot_value) ==
            COMPACT_ACCELERATION_DECODE_OK,
        "retained snapshot decodes");
    check(
        screen_value.x == snapshot_value.x &&
            screen_value.y == snapshot_value.y &&
            screen_value.z == snapshot_value.z,
        "screen value comes from compact bytes after raw sample changes");
    check(
        screen_value.x != raw.x && screen_value.y != raw.y && screen_value.z != raw.z,
        "screen model has no raw-value bypass");
}

static void check_display(float value, const char *expected)
{
    struct sevenths_display_value display = {false, 0U, 0U};
    char text[16];
    check(sevenths_display_quantize(value, &display), "screen value quantizes");
    check(sevenths_display_format_ascii(&display, text, sizeof(text)), "screen value formats");
    check(strcmp(text, expected) == 0, "screen value has expected seventh format");
}

static void test_sevenths_display(void)
{
    check_display(-0.01F, "+0");
    check_display(-0.08F, "-1/7");
    check_display(0.08F, "+1/7");
    check_display(5.0F, "+5");
    check_display(-40.0F / 7.0F, "-5 5/7");

    for (uint32_t numerator = 0U; numerator <= 6U; ++numerator) {
        struct sevenths_display_value display = {false, 0U, 0U};
        check(
            sevenths_display_quantize((float)numerator / 7.0F, &display),
            "fraction numerator quantizes");
        check(display.whole == 0U, "proper seventh has zero whole part");
        check(display.numerator == numerator, "formatter covers numerator 0 through 6");
    }

    struct sevenths_display_value integral = {false, 0U, 0U};
    check(sevenths_display_quantize(5.0F, &integral), "integral value quantizes");
    check(integral.numerator == 0U, "integral value suppresses 0/7");
}

int main(void)
{
    test_balanced_reference();
    test_antipodal_gravity();
    test_axis_gravity_points();
    test_direction_sphere();
    test_every_magnitude_code();
    test_zero_overflow_nonfinite_and_malformed();
    test_screen_path_cannot_bypass_retained_state();
    test_sevenths_display();

    if (failure_count != 0) {
        (void)fprintf(stderr, "%d compact-acceleration test(s) failed\n", failure_count);
        return 1;
    }
    (void)printf("PASS compact acceleration codec, retained screen path, and sevenths display\n");
    return 0;
}
