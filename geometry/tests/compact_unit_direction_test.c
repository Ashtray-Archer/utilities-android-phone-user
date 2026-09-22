#include "compact_unit_direction.h"

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

static float direction_norm(struct direction3 value)
{
    return sqrtf(value.x * value.x + value.y * value.y + value.z * value.z);
}

static void check_axis(
    struct direction3 direction,
    struct compact_unit_direction expected,
    const char *message)
{
    struct compact_unit_direction encoded = {0U, 0U, 0U};
    check(
        compact_unit_direction_encode(direction, &encoded) ==
            COMPACT_UNIT_DIRECTION_ENCODE_OK,
        message);
    check(
        memcmp(&encoded, &expected, sizeof(encoded)) == 0,
        "axis direction has stable packed bytes");

    struct direction3 decoded;
    compact_unit_direction_decode(&encoded, &decoded);
    check(
        fabsf(direction_norm(decoded) - 1.0F) <= 2.0e-6F,
        "axis direction decodes onto S2");
}

static void test_fixed_axis_bytes(void)
{
    check_axis(
        (struct direction3){1.0F, 0.0F, 0.0F},
        (struct compact_unit_direction){0xffU, 0x07U, 0x00U},
        "+X encodes");
    check_axis(
        (struct direction3){-1.0F, 0.0F, 0.0F},
        (struct compact_unit_direction){0x00U, 0x08U, 0x00U},
        "-X encodes");
    check_axis(
        (struct direction3){0.0F, 1.0F, 0.0F},
        (struct compact_unit_direction){0x00U, 0xf0U, 0x7fU},
        "+Y encodes");
    check_axis(
        (struct direction3){0.0F, -1.0F, 0.0F},
        (struct compact_unit_direction){0x00U, 0x00U, 0x80U},
        "-Y encodes");
    check_axis(
        (struct direction3){0.0F, 0.0F, 1.0F},
        (struct compact_unit_direction){0x00U, 0x00U, 0x00U},
        "+Z encodes");
    check_axis(
        (struct direction3){0.0F, 0.0F, -1.0F},
        (struct compact_unit_direction){0xffU, 0xf7U, 0x7fU},
        "-Z encodes");
}

static void test_rejected_input_does_not_publish(void)
{
    struct compact_unit_direction encoded = {0x12U, 0x34U, 0x56U};
    struct compact_unit_direction before = encoded;

    check(
        compact_unit_direction_encode(
            (struct direction3){0.0F, 0.0F, 0.0F},
            &encoded) == COMPACT_UNIT_DIRECTION_ENCODE_ZERO,
        "zero has no direction");
    check(
        memcmp(&encoded, &before, sizeof(encoded)) == 0,
        "zero leaves output untouched");

    check(
        compact_unit_direction_encode(
            (struct direction3){NAN, 0.0F, 0.0F},
            &encoded) == COMPACT_UNIT_DIRECTION_ENCODE_NONFINITE,
        "nonfinite direction is rejected");
    check(
        memcmp(&encoded, &before, sizeof(encoded)) == 0,
        "nonfinite input leaves output untouched");
}

static void test_direction_sphere(void)
{
    const uint32_t count = 131072U;
    float golden_angle = acosf(-1.0F) * (3.0F - sqrtf(5.0F));
    float maximum_component_error = 0.0F;

    for (uint32_t index = 0U; index < count; ++index) {
        float expected_z =
            1.0F - 2.0F * ((float)index + 0.5F) / (float)count;
        float radius =
            sqrtf(fmaxf(0.0F, 1.0F - expected_z * expected_z));
        float angle = (float)index * golden_angle;
        struct direction3 expected = {
            radius * cosf(angle),
            radius * sinf(angle),
            expected_z};

        struct compact_unit_direction encoded;
        check(
            compact_unit_direction_encode(expected, &encoded) ==
                COMPACT_UNIT_DIRECTION_ENCODE_OK,
            "sphere direction encodes");

        struct direction3 decoded;
        compact_unit_direction_decode(&encoded, &decoded);
        check(
            fabsf(direction_norm(decoded) - 1.0F) <= 2.0e-6F,
            "decoded direction remains on S2");

        float component_error = fmaxf(
            fabsf(decoded.x - expected.x),
            fmaxf(
                fabsf(decoded.y - expected.y),
                fabsf(decoded.z - expected.z)));
        if (component_error > maximum_component_error) {
            maximum_component_error = component_error;
        }
    }

    check(
        maximum_component_error < 0.0011F,
        "Q0.11 octahedral direction retains declared component-error bound");
}

int main(void)
{
    test_fixed_axis_bytes();
    test_rejected_input_does_not_publish();
    test_direction_sphere();

    if (failures != 0) {
        (void)fprintf(
            stderr,
            "%d compact-unit-direction test(s) failed\n",
            failures);
        return 1;
    }

    (void)printf(
        "PASS compact S2 / unit-pure-quaternion direction codec\n");
    return 0;
}
