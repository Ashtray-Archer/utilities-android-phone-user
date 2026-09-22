#include "compact_unit_direction.h"

#include <float.h>
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

static float unit_sphere_point_norm(
    struct compact_unit_direction_point_on_unit_sphere point)
{
    return sqrtf(point.x * point.x + point.y * point.y + point.z * point.z);
}

static void check_axis(
    struct compact_unit_direction_vector3 direction,
    struct compact_unit_direction expected,
    const char *message)
{
    struct compact_unit_direction encoded = {0U, 0U, 0U};
    check(
        compact_unit_direction_encode_vector(direction, &encoded) ==
            COMPACT_UNIT_DIRECTION_ENCODE_OK,
        message);
    check(
        memcmp(&encoded, &expected, sizeof(encoded)) == 0,
        "axis direction has stable packed bytes");

    struct compact_unit_direction_point_on_unit_sphere decoded =
        compact_unit_direction_decode_to_unit_sphere(&encoded);
    check(
        fabsf(unit_sphere_point_norm(decoded) - 1.0F) <= 2.0e-6F,
        "axis direction decodes onto S2");
}

static void test_fixed_axis_bytes(void)
{
    check_axis(
        (struct compact_unit_direction_vector3){1.0F, 0.0F, 0.0F},
        (struct compact_unit_direction){0xffU, 0x07U, 0x00U},
        "+X encodes");
    check_axis(
        (struct compact_unit_direction_vector3){-1.0F, 0.0F, 0.0F},
        (struct compact_unit_direction){0x00U, 0x08U, 0x00U},
        "-X encodes");
    check_axis(
        (struct compact_unit_direction_vector3){0.0F, 1.0F, 0.0F},
        (struct compact_unit_direction){0x00U, 0xf0U, 0x7fU},
        "+Y encodes");
    check_axis(
        (struct compact_unit_direction_vector3){0.0F, -1.0F, 0.0F},
        (struct compact_unit_direction){0x00U, 0x00U, 0x80U},
        "-Y encodes");
    check_axis(
        (struct compact_unit_direction_vector3){0.0F, 0.0F, 1.0F},
        (struct compact_unit_direction){0x00U, 0x00U, 0x00U},
        "+Z encodes");
    check_axis(
        (struct compact_unit_direction_vector3){0.0F, 0.0F, -1.0F},
        (struct compact_unit_direction){0xffU, 0xf7U, 0x7fU},
        "-Z encodes");
}

static void test_rejected_input_does_not_publish(void)
{
    struct compact_unit_direction encoded = {0x12U, 0x34U, 0x56U};
    struct compact_unit_direction before = encoded;

    check(
        compact_unit_direction_encode_vector(
            (struct compact_unit_direction_vector3){0.0F, 0.0F, 0.0F},
            &encoded) == COMPACT_UNIT_DIRECTION_ENCODE_ZERO,
        "zero has no direction");
    check(
        memcmp(&encoded, &before, sizeof(encoded)) == 0,
        "zero leaves output untouched");

    check(
        compact_unit_direction_encode_vector(
            (struct compact_unit_direction_vector3){NAN, 0.0F, 0.0F},
            &encoded) == COMPACT_UNIT_DIRECTION_ENCODE_NONFINITE,
        "nonfinite direction is rejected");
    check(
        memcmp(&encoded, &before, sizeof(encoded)) == 0,
        "nonfinite input leaves output untouched");
}

static void test_projection_uses_overflow_safe_l_infinity_prescaling(void)
{
    struct compact_unit_direction_vector3 large_vector = {
        FLT_MAX,
        -FLT_MAX,
        FLT_MAX};
    struct compact_unit_direction_point_on_unit_l1_octahedron projected =
        compact_unit_direction_project_nonzero_vector_onto_unit_l1_octahedron(
            large_vector);

    check(
        isfinite(projected.x) && isfinite(projected.y) && isfinite(projected.z),
        "L-infinity prescaling keeps the L1 projection finite");
    check(
        near(
            fabsf(projected.x) + fabsf(projected.y) + fabsf(projected.z),
            1.0F,
            2.0e-6F),
        "projection lands on the unit L1 octahedron");
    check(
        near(projected.x, 1.0F / 3.0F, 1.0e-6F) &&
            near(projected.y, -1.0F / 3.0F, 1.0e-6F) &&
            near(projected.z, 1.0F / 3.0F, 1.0e-6F),
        "L-infinity prescaling does not change the represented direction");
}

static void test_fold_and_unfold_are_reverse_geometric_stages(void)
{
    struct compact_unit_direction_point_on_unit_l1_octahedron lower_point = {
        0.2F,
        -0.3F,
        -0.5F};
    struct compact_unit_direction_octahedral_square_coordinates folded =
        compact_unit_direction_fold_unit_l1_octahedron_into_square_coordinates(
            lower_point);
    struct compact_unit_direction_point_on_unit_l1_octahedron unfolded =
        compact_unit_direction_unfold_square_coordinates_onto_unit_l1_octahedron(
            folded);

    check(
        near(folded.first, 0.7F, 1.0e-6F) &&
            near(folded.second, -0.8F, 1.0e-6F),
        "lower octahedral hemisphere folds into the square");
    check(
        near(unfolded.x, lower_point.x, 1.0e-6F) &&
            near(unfolded.y, lower_point.y, 1.0e-6F) &&
            near(unfolded.z, lower_point.z, 1.0e-6F),
        "unfold reverses fold before quantization");
}

static void test_q0_11_and_24_bit_storage_boundaries(void)
{
    struct compact_unit_direction_q0_11_coordinates quantized =
        compact_unit_direction_quantize_square_coordinates_as_q0_11(
            (struct compact_unit_direction_octahedral_square_coordinates){
                1.0F,
                -1.0F});
    check(
        quantized.first == COMPACT_UNIT_DIRECTION_MAXIMUM_CODE &&
            quantized.second == COMPACT_UNIT_DIRECTION_MINIMUM_CODE,
        "Q0.11 quantization preserves its asymmetric signed endpoints");

    struct compact_unit_direction packed =
        compact_unit_direction_pack_two_signed_12_bit_coordinates(quantized);
    struct compact_unit_direction_q0_11_coordinates unpacked =
        compact_unit_direction_unpack_two_signed_12_bit_coordinates(packed);
    check(
        unpacked.first == quantized.first &&
            unpacked.second == quantized.second,
        "packing and unpacking are exact for signed 12-bit coordinates");

    struct compact_unit_direction_octahedral_square_coordinates dequantized =
        compact_unit_direction_dequantize_q0_11_square_coordinates(unpacked);
    check(
        near(
            dequantized.first,
            (float)COMPACT_UNIT_DIRECTION_MAXIMUM_CODE /
                (float)COMPACT_UNIT_DIRECTION_SCALE,
            0.0F) &&
            near(dequantized.second, -1.0F, 0.0F),
        "dequantization exposes the positive-endpoint approximation");

    for (int32_t code = COMPACT_UNIT_DIRECTION_MINIMUM_CODE;
         code <= COMPACT_UNIT_DIRECTION_MAXIMUM_CODE;
         ++code) {
        struct compact_unit_direction_q0_11_coordinates every_code = {
            code,
            code};
        struct compact_unit_direction every_code_packed =
            compact_unit_direction_pack_two_signed_12_bit_coordinates(every_code);
        struct compact_unit_direction_q0_11_coordinates every_code_unpacked =
            compact_unit_direction_unpack_two_signed_12_bit_coordinates(
                every_code_packed);
        check(
            every_code_unpacked.first == code &&
                every_code_unpacked.second == code,
            "every signed 12-bit code survives packing and unpacking");
    }
}

static void test_encoding_depends_on_direction_not_vector_magnitude(void)
{
    struct compact_unit_direction ordinary_encoding;
    struct compact_unit_direction large_encoding;
    check(
        compact_unit_direction_encode_vector(
            (struct compact_unit_direction_vector3){4.0F, -2.0F, 1.0F},
            &ordinary_encoding) == COMPACT_UNIT_DIRECTION_ENCODE_OK,
        "ordinary nonzero vector encodes");
    check(
        compact_unit_direction_encode_vector(
            (struct compact_unit_direction_vector3){
                FLT_MAX,
                -0.5F * FLT_MAX,
                0.25F * FLT_MAX},
            &large_encoding) == COMPACT_UNIT_DIRECTION_ENCODE_OK,
        "large finite vector encodes");
    check(
        memcmp(
            &ordinary_encoding,
            &large_encoding,
            sizeof(ordinary_encoding)) == 0,
        "positive rescaling leaves compact direction bytes unchanged");
}

static void test_extracted_api_names_remain_compatible(void)
{
    struct compact_unit_direction canonical_encoding;
    struct compact_unit_direction compatibility_encoding;
    check(
        compact_unit_direction_encode_vector(
            (struct compact_unit_direction_vector3){0.25F, -0.5F, 0.75F},
            &canonical_encoding) == COMPACT_UNIT_DIRECTION_ENCODE_OK,
        "canonical vector API encodes compatibility fixture");
    check(
        compact_unit_direction_encode(
            (struct direction3){0.25F, -0.5F, 0.75F},
            &compatibility_encoding) == COMPACT_UNIT_DIRECTION_ENCODE_OK,
        "extracted direction3 API still encodes");
    check(
        memcmp(
            &canonical_encoding,
            &compatibility_encoding,
            sizeof(canonical_encoding)) == 0,
        "compatibility encoder delegates without changing bytes");

    struct compact_unit_direction_point_on_unit_sphere canonical_decoding =
        compact_unit_direction_decode_to_unit_sphere(&canonical_encoding);
    struct direction3 compatibility_decoding;
    compact_unit_direction_decode(
        &compatibility_encoding,
        &compatibility_decoding);
    check(
        compatibility_decoding.x == canonical_decoding.x &&
            compatibility_decoding.y == canonical_decoding.y &&
            compatibility_decoding.z == canonical_decoding.z,
        "compatibility decoder delegates without changing components");
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
        struct compact_unit_direction_vector3 expected = {
            radius * cosf(angle),
            radius * sinf(angle),
            expected_z};

        struct compact_unit_direction encoded;
        check(
            compact_unit_direction_encode_vector(expected, &encoded) ==
                COMPACT_UNIT_DIRECTION_ENCODE_OK,
            "sphere direction encodes");

        struct compact_unit_direction_point_on_unit_sphere decoded =
            compact_unit_direction_decode_to_unit_sphere(&encoded);
        check(
            fabsf(unit_sphere_point_norm(decoded) - 1.0F) <= 2.0e-6F,
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
    test_projection_uses_overflow_safe_l_infinity_prescaling();
    test_fold_and_unfold_are_reverse_geometric_stages();
    test_q0_11_and_24_bit_storage_boundaries();
    test_encoding_depends_on_direction_not_vector_magnitude();
    test_extracted_api_names_remain_compatible();
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
