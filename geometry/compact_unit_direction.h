#ifndef COMPACT_UNIT_DIRECTION_H
#define COMPACT_UNIT_DIRECTION_H

#include <math.h>
#include <stdbool.h>
#include <stdint.h>

/* An arbitrary three-dimensional vector; no normalization is implied. */
struct compact_unit_direction_vector3 {
    float x;
    float y;
    float z;
};

/* A result constructed on {x ∈ ℝ³ : ‖x‖₁ = 1}. */
struct compact_unit_direction_point_on_unit_l1_octahedron {
    float x;
    float y;
    float z;
};

/*
 * Coordinates in the folded octahedral square [-1, 1]^2.
 *
 * This parameterization has folds and seams.  It is not one ordinary global
 * manifold chart on S².
 */
struct compact_unit_direction_octahedral_square_coordinates {
    float first;
    float second;
};

/*
 * Two signed 12-bit Q0.11 codes, held in int32_t for C arithmetic.
 * The C record cannot enforce the range; the quantizer constructs values in
 * [-2048, 2047], and the packer is specified for values in that range.
 */
struct compact_unit_direction_q0_11_coordinates {
    int32_t first;
    int32_t second;
};

/* A result constructed on S², up to Float32 rounding. */
struct compact_unit_direction_point_on_unit_sphere {
    float x;
    float y;
    float z;
};

/*
 * Compatibility carrier for the API extracted before the mathematical stage
 * types were named.  Unlike point_on_unit_sphere, this type alone does not
 * assert unit length.
 */
struct direction3 {
    float x;
    float y;
    float z;
};

/*
 * Three-byte storage for one direction on S^2.
 *
 * A unit direction (x,y,z) is equivalently the unit pure quaternion
 *
 *     0 + x i + y j + z k.
 *
 * This is only the S² subset of pure unit quaternions.  It is not the S³ of
 * general unit quaternions and does not represent a complete SO(3) device
 * orientation.
 *
 * The mathematical encoding stages are
 *
 *     ℝ³ \ {0}
 *       → {x ∈ ℝ³ : ‖x‖₁ = 1}
 *       → folded octahedral square coordinates
 *       → two signed Q0.11 integer coordinates
 *       → 24 packed bits.
 *
 * Decode follows the reverse stages and finally normalizes onto S².  Packing
 * and unpacking are exact for signed 12-bit codes.  Quantization and Float32
 * arithmetic make the complete encode/decode path approximate rather than an
 * exact numerical inverse.
 */
struct compact_unit_direction {
    uint8_t low;
    uint8_t middle;
    uint8_t high;
};

_Static_assert(
    sizeof(struct compact_unit_direction) == 3U,
    "compact unit direction must occupy exactly three bytes");

enum compact_unit_direction_encode_status {
    COMPACT_UNIT_DIRECTION_ENCODE_OK = 0,
    COMPACT_UNIT_DIRECTION_ENCODE_ZERO,
    COMPACT_UNIT_DIRECTION_ENCODE_NONFINITE
};

enum {
    COMPACT_UNIT_DIRECTION_FRACTION_BITS = 11,
    COMPACT_UNIT_DIRECTION_SCALE = 1 << COMPACT_UNIT_DIRECTION_FRACTION_BITS,
    COMPACT_UNIT_DIRECTION_MAXIMUM_CODE = COMPACT_UNIT_DIRECTION_SCALE - 1,
    COMPACT_UNIT_DIRECTION_MINIMUM_CODE = -COMPACT_UNIT_DIRECTION_SCALE
};

static inline float compact_unit_direction_sign_with_zero_positive(float value)
{
    /* The octahedral seam uses the positive branch for both +0 and -0. */
    return value < 0.0F ? -1.0F : 1.0F;
}

static inline bool compact_unit_direction_vector3_is_finite(
    struct compact_unit_direction_vector3 vector)
{
    return isfinite(vector.x) && isfinite(vector.y) && isfinite(vector.z);
}

static inline bool compact_unit_direction_vector3_is_zero(
    struct compact_unit_direction_vector3 vector)
{
    return vector.x == 0.0F && vector.y == 0.0F && vector.z == 0.0F;
}

static inline float compact_unit_direction_l_infinity_norm_of_vector3(
    struct compact_unit_direction_vector3 vector)
{
    return fmaxf(fabsf(vector.x), fmaxf(fabsf(vector.y), fabsf(vector.z)));
}

static inline struct compact_unit_direction_vector3
compact_unit_direction_prescale_nonzero_vector_by_l_infinity_norm(
    struct compact_unit_direction_vector3 nonzero_vector)
{
    float l_infinity_norm =
        compact_unit_direction_l_infinity_norm_of_vector3(nonzero_vector);

    return (struct compact_unit_direction_vector3){
        nonzero_vector.x / l_infinity_norm,
        nonzero_vector.y / l_infinity_norm,
        nonzero_vector.z / l_infinity_norm};
}

static inline float compact_unit_direction_l1_norm_of_vector3(
    struct compact_unit_direction_vector3 vector)
{
    return fabsf(vector.x) + fabsf(vector.y) + fabsf(vector.z);
}

static inline struct compact_unit_direction_point_on_unit_l1_octahedron
compact_unit_direction_l1_normalize_nonzero_vector(
    struct compact_unit_direction_vector3 nonzero_vector)
{
    float l1_norm =
        compact_unit_direction_l1_norm_of_vector3(nonzero_vector);

    return (struct compact_unit_direction_point_on_unit_l1_octahedron){
        nonzero_vector.x / l1_norm,
        nonzero_vector.y / l1_norm,
        nonzero_vector.z / l1_norm};
}

static inline struct compact_unit_direction_point_on_unit_l1_octahedron
compact_unit_direction_project_nonzero_vector_onto_unit_l1_octahedron(
    struct compact_unit_direction_vector3 nonzero_vector)
{
    /*
     * Dividing first by the L-infinity norm prevents |x| + |y| + |z| from
     * overflowing.  It does not change the represented direction:
     *
     *     (v / ‖v‖∞) / ‖v / ‖v‖∞‖₁ = v / ‖v‖₁.
     */
    struct compact_unit_direction_vector3 safely_prescaled_vector =
        compact_unit_direction_prescale_nonzero_vector_by_l_infinity_norm(
            nonzero_vector);

    return compact_unit_direction_l1_normalize_nonzero_vector(
        safely_prescaled_vector);
}

static inline float
compact_unit_direction_reflect_coordinate_across_octahedral_fold(
    float coordinate,
    float other_coordinate)
{
    return (1.0F - fabsf(other_coordinate)) *
        compact_unit_direction_sign_with_zero_positive(coordinate);
}

static inline struct compact_unit_direction_octahedral_square_coordinates
compact_unit_direction_fold_unit_l1_octahedron_into_square_coordinates(
    struct compact_unit_direction_point_on_unit_l1_octahedron octahedron_point)
{
    if (octahedron_point.z >= 0.0F) {
        return (struct compact_unit_direction_octahedral_square_coordinates){
            octahedron_point.x,
            octahedron_point.y};
    }

    return (struct compact_unit_direction_octahedral_square_coordinates){
        compact_unit_direction_reflect_coordinate_across_octahedral_fold(
            octahedron_point.x,
            octahedron_point.y),
        compact_unit_direction_reflect_coordinate_across_octahedral_fold(
            octahedron_point.y,
            octahedron_point.x)};
}

static inline float
compact_unit_direction_scale_square_coordinate_to_q0_11_units(
    float octahedral_coordinate)
{
    return octahedral_coordinate * (float)COMPACT_UNIT_DIRECTION_SCALE;
}

static inline int32_t
compact_unit_direction_round_q0_11_units_to_nearest_integer_code(
    float q0_11_units)
{
    return (int32_t)roundf(q0_11_units);
}

static inline int32_t
compact_unit_direction_clamp_integer_to_signed_12_bit_range(
    int32_t integer_code)
{
    if (integer_code < COMPACT_UNIT_DIRECTION_MINIMUM_CODE) {
        return COMPACT_UNIT_DIRECTION_MINIMUM_CODE;
    }
    if (integer_code > COMPACT_UNIT_DIRECTION_MAXIMUM_CODE) {
        return COMPACT_UNIT_DIRECTION_MAXIMUM_CODE;
    }
    return integer_code;
}

static inline int32_t compact_unit_direction_quantize_one_q0_11_coordinate(
    float octahedral_coordinate)
{
    float q0_11_units =
        compact_unit_direction_scale_square_coordinate_to_q0_11_units(
            octahedral_coordinate);
    int32_t nearest_integer_code =
        compact_unit_direction_round_q0_11_units_to_nearest_integer_code(
            q0_11_units);

    return compact_unit_direction_clamp_integer_to_signed_12_bit_range(
        nearest_integer_code);
}

static inline struct compact_unit_direction_q0_11_coordinates
compact_unit_direction_quantize_square_coordinates_as_q0_11(
    struct compact_unit_direction_octahedral_square_coordinates coordinates)
{
    return (struct compact_unit_direction_q0_11_coordinates){
        compact_unit_direction_quantize_one_q0_11_coordinate(coordinates.first),
        compact_unit_direction_quantize_one_q0_11_coordinate(coordinates.second)};
}

static inline uint32_t compact_unit_direction_signed_12_bit_twos_complement_bits(
    int32_t signed_code)
{
    return (uint32_t)signed_code & 0x0fffU;
}

static inline uint32_t
compact_unit_direction_combine_two_signed_12_bit_coordinates_as_24_bits(
    struct compact_unit_direction_q0_11_coordinates quantized_coordinates)
{
    return compact_unit_direction_signed_12_bit_twos_complement_bits(
            quantized_coordinates.first) |
        (compact_unit_direction_signed_12_bit_twos_complement_bits(
            quantized_coordinates.second) << 12U);
}

static inline struct compact_unit_direction
compact_unit_direction_pack_two_signed_12_bit_coordinates(
    struct compact_unit_direction_q0_11_coordinates quantized_coordinates)
{
    uint32_t packed_24_bits =
        compact_unit_direction_combine_two_signed_12_bit_coordinates_as_24_bits(
            quantized_coordinates);

    return (struct compact_unit_direction){
        (uint8_t)(packed_24_bits & 0xffU),
        (uint8_t)((packed_24_bits >> 8U) & 0xffU),
        (uint8_t)((packed_24_bits >> 16U) & 0xffU)};
}

static inline int32_t compact_unit_direction_sign_extend_signed_12_bit_code(
    uint32_t bits)
{
    uint32_t low_12_bits = bits & 0x0fffU;
    return (low_12_bits & 0x0800U) != 0U
        ? (int32_t)low_12_bits - 0x1000
        : (int32_t)low_12_bits;
}

static inline uint32_t compact_unit_direction_three_bytes_as_24_bits(
    struct compact_unit_direction encoded)
{
    return (uint32_t)encoded.low |
        ((uint32_t)encoded.middle << 8U) |
        ((uint32_t)encoded.high << 16U);
}

static inline struct compact_unit_direction_q0_11_coordinates
compact_unit_direction_unpack_two_signed_12_bit_coordinates(
    struct compact_unit_direction encoded)
{
    uint32_t packed_24_bits =
        compact_unit_direction_three_bytes_as_24_bits(encoded);

    return (struct compact_unit_direction_q0_11_coordinates){
        compact_unit_direction_sign_extend_signed_12_bit_code(packed_24_bits),
        compact_unit_direction_sign_extend_signed_12_bit_code(
            packed_24_bits >> 12U)};
}

static inline float compact_unit_direction_dequantize_one_q0_11_coordinate(
    int32_t quantized_coordinate)
{
    return (float)quantized_coordinate /
        (float)COMPACT_UNIT_DIRECTION_SCALE;
}

static inline struct compact_unit_direction_octahedral_square_coordinates
compact_unit_direction_dequantize_q0_11_square_coordinates(
    struct compact_unit_direction_q0_11_coordinates quantized_coordinates)
{
    return (struct compact_unit_direction_octahedral_square_coordinates){
        compact_unit_direction_dequantize_one_q0_11_coordinate(
            quantized_coordinates.first),
        compact_unit_direction_dequantize_one_q0_11_coordinate(
            quantized_coordinates.second)};
}

static inline float
compact_unit_direction_reconstruct_octahedron_z_from_square_coordinates(
    struct compact_unit_direction_octahedral_square_coordinates coordinates)
{
    return 1.0F - fabsf(coordinates.first) - fabsf(coordinates.second);
}

static inline struct compact_unit_direction_point_on_unit_l1_octahedron
compact_unit_direction_unfold_square_coordinates_onto_unit_l1_octahedron(
    struct compact_unit_direction_octahedral_square_coordinates coordinates)
{
    float octahedron_z =
        compact_unit_direction_reconstruct_octahedron_z_from_square_coordinates(
            coordinates);

    if (octahedron_z >= 0.0F) {
        return (struct compact_unit_direction_point_on_unit_l1_octahedron){
            coordinates.first,
            coordinates.second,
            octahedron_z};
    }

    return (struct compact_unit_direction_point_on_unit_l1_octahedron){
        compact_unit_direction_reflect_coordinate_across_octahedral_fold(
            coordinates.first,
            coordinates.second),
        compact_unit_direction_reflect_coordinate_across_octahedral_fold(
            coordinates.second,
            coordinates.first),
        octahedron_z};
}

static inline float
compact_unit_direction_euclidean_norm_of_l1_octahedron_point(
    struct compact_unit_direction_point_on_unit_l1_octahedron octahedron_point)
{
    return sqrtf(
        octahedron_point.x * octahedron_point.x +
        octahedron_point.y * octahedron_point.y +
        octahedron_point.z * octahedron_point.z);
}

static inline struct compact_unit_direction_point_on_unit_sphere
compact_unit_direction_euclidean_normalize_octahedron_point_onto_unit_sphere(
    struct compact_unit_direction_point_on_unit_l1_octahedron octahedron_point)
{
    float euclidean_norm =
        compact_unit_direction_euclidean_norm_of_l1_octahedron_point(
            octahedron_point);

    return (struct compact_unit_direction_point_on_unit_sphere){
        octahedron_point.x / euclidean_norm,
        octahedron_point.y / euclidean_norm,
        octahedron_point.z / euclidean_norm};
}

static inline struct compact_unit_direction
compact_unit_direction_encode_nonzero_finite_vector(
    struct compact_unit_direction_vector3 nonzero_finite_vector)
{
    struct compact_unit_direction_point_on_unit_l1_octahedron octahedron_point =
        compact_unit_direction_project_nonzero_vector_onto_unit_l1_octahedron(
            nonzero_finite_vector);
    struct compact_unit_direction_octahedral_square_coordinates coordinates =
        compact_unit_direction_fold_unit_l1_octahedron_into_square_coordinates(
            octahedron_point);
    struct compact_unit_direction_q0_11_coordinates quantized_coordinates =
        compact_unit_direction_quantize_square_coordinates_as_q0_11(coordinates);

    return compact_unit_direction_pack_two_signed_12_bit_coordinates(
        quantized_coordinates);
}

static inline enum compact_unit_direction_encode_status
compact_unit_direction_encode_vector(
    struct compact_unit_direction_vector3 vector,
    struct compact_unit_direction *encoded)
{
    if (!compact_unit_direction_vector3_is_finite(vector)) {
        return COMPACT_UNIT_DIRECTION_ENCODE_NONFINITE;
    }
    if (compact_unit_direction_vector3_is_zero(vector)) {
        return COMPACT_UNIT_DIRECTION_ENCODE_ZERO;
    }

    struct compact_unit_direction complete_encoding =
        compact_unit_direction_encode_nonzero_finite_vector(vector);
    *encoded = complete_encoding;
    return COMPACT_UNIT_DIRECTION_ENCODE_OK;
}

static inline struct compact_unit_direction_point_on_unit_sphere
compact_unit_direction_decode_to_unit_sphere(
    const struct compact_unit_direction *encoded)
{
    struct compact_unit_direction_q0_11_coordinates quantized_coordinates =
        compact_unit_direction_unpack_two_signed_12_bit_coordinates(*encoded);
    struct compact_unit_direction_octahedral_square_coordinates coordinates =
        compact_unit_direction_dequantize_q0_11_square_coordinates(
            quantized_coordinates);
    struct compact_unit_direction_point_on_unit_l1_octahedron octahedron_point =
        compact_unit_direction_unfold_square_coordinates_onto_unit_l1_octahedron(
            coordinates);

    return compact_unit_direction_euclidean_normalize_octahedron_point_onto_unit_sphere(
        octahedron_point);
}

/* Compatibility wrappers for the names already consumed by accelerometer code. */
static inline float compact_unit_direction_sign_not_zero(float value)
{
    return compact_unit_direction_sign_with_zero_positive(value);
}

static inline int32_t compact_unit_direction_quantize_coordinate(float coordinate)
{
    return compact_unit_direction_quantize_one_q0_11_coordinate(coordinate);
}

static inline uint32_t compact_unit_direction_twos_complement_12(int32_t code)
{
    return compact_unit_direction_signed_12_bit_twos_complement_bits(code);
}

static inline int32_t compact_unit_direction_sign_extend_12(uint32_t bits)
{
    return compact_unit_direction_sign_extend_signed_12_bit_code(bits);
}

static inline void compact_unit_direction_encode_nonzero_finite(
    struct direction3 direction,
    struct compact_unit_direction *encoded)
{
    *encoded = compact_unit_direction_encode_nonzero_finite_vector(
        (struct compact_unit_direction_vector3){
            direction.x,
            direction.y,
            direction.z});
}

static inline enum compact_unit_direction_encode_status compact_unit_direction_encode(
    struct direction3 direction,
    struct compact_unit_direction *encoded)
{
    return compact_unit_direction_encode_vector(
        (struct compact_unit_direction_vector3){
            direction.x,
            direction.y,
            direction.z},
        encoded);
}

static inline void compact_unit_direction_decode(
    const struct compact_unit_direction *encoded,
    struct direction3 *unit_direction)
{
    struct compact_unit_direction_point_on_unit_sphere decoded =
        compact_unit_direction_decode_to_unit_sphere(encoded);
    *unit_direction = (struct direction3){decoded.x, decoded.y, decoded.z};
}

#endif
