#ifndef COMPACT_UNIT_DIRECTION_H
#define COMPACT_UNIT_DIRECTION_H

#include <math.h>
#include <stdint.h>

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
 * The representation stores two signed Q0.11 coordinates in the standard
 * octahedral chart of S^2 as adjacent 12-bit two's-complement integers.
 * It does not store a general unit quaternion in S^3 and carries no magnitude.
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

static inline float compact_unit_direction_sign_not_zero(float value)
{
    return value < 0.0F ? -1.0F : 1.0F;
}

static inline int32_t compact_unit_direction_quantize_coordinate(float coordinate)
{
    float scaled = coordinate * (float)COMPACT_UNIT_DIRECTION_SCALE;
    int32_t code = (int32_t)roundf(scaled);
    if (code < COMPACT_UNIT_DIRECTION_MINIMUM_CODE) {
        return COMPACT_UNIT_DIRECTION_MINIMUM_CODE;
    }
    if (code > COMPACT_UNIT_DIRECTION_MAXIMUM_CODE) {
        return COMPACT_UNIT_DIRECTION_MAXIMUM_CODE;
    }
    return code;
}

static inline uint32_t compact_unit_direction_twos_complement_12(int32_t code)
{
    return (uint32_t)code & 0x0fffU;
}

static inline int32_t compact_unit_direction_sign_extend_12(uint32_t bits)
{
    bits &= 0x0fffU;
    return (bits & 0x0800U) != 0U ? (int32_t)bits - 0x1000 : (int32_t)bits;
}

static inline void compact_unit_direction_encode_nonzero_finite(
    struct direction3 direction,
    struct compact_unit_direction *encoded)
{
    float scale = fmaxf(
        fabsf(direction.x),
        fmaxf(fabsf(direction.y), fabsf(direction.z)));
    float scaled_x = direction.x / scale;
    float scaled_y = direction.y / scale;
    float scaled_z = direction.z / scale;
    float l1_norm = fabsf(scaled_x) + fabsf(scaled_y) + fabsf(scaled_z);
    float chart_x = scaled_x / l1_norm;
    float chart_y = scaled_y / l1_norm;
    float chart_z = scaled_z / l1_norm;

    if (chart_z < 0.0F) {
        float unfolded_x = chart_x;
        float unfolded_y = chart_y;
        chart_x =
            (1.0F - fabsf(unfolded_y)) *
            compact_unit_direction_sign_not_zero(unfolded_x);
        chart_y =
            (1.0F - fabsf(unfolded_x)) *
            compact_unit_direction_sign_not_zero(unfolded_y);
    }

    int32_t first_code = compact_unit_direction_quantize_coordinate(chart_x);
    int32_t second_code = compact_unit_direction_quantize_coordinate(chart_y);
    uint32_t packed =
        compact_unit_direction_twos_complement_12(first_code) |
        (compact_unit_direction_twos_complement_12(second_code) << 12U);

    encoded->low = (uint8_t)(packed & 0xffU);
    encoded->middle = (uint8_t)((packed >> 8U) & 0xffU);
    encoded->high = (uint8_t)((packed >> 16U) & 0xffU);
}

static inline enum compact_unit_direction_encode_status compact_unit_direction_encode(
    struct direction3 direction,
    struct compact_unit_direction *encoded)
{
    if (!isfinite(direction.x) ||
        !isfinite(direction.y) ||
        !isfinite(direction.z)) {
        return COMPACT_UNIT_DIRECTION_ENCODE_NONFINITE;
    }

    float scale = fmaxf(
        fabsf(direction.x),
        fmaxf(fabsf(direction.y), fabsf(direction.z)));
    if (scale == 0.0F) {
        return COMPACT_UNIT_DIRECTION_ENCODE_ZERO;
    }

    compact_unit_direction_encode_nonzero_finite(direction, encoded);
    return COMPACT_UNIT_DIRECTION_ENCODE_OK;
}

static inline void compact_unit_direction_decode(
    const struct compact_unit_direction *encoded,
    struct direction3 *unit_direction)
{
    uint32_t packed =
        (uint32_t)encoded->low |
        ((uint32_t)encoded->middle << 8U) |
        ((uint32_t)encoded->high << 16U);
    int32_t first_code = compact_unit_direction_sign_extend_12(packed);
    int32_t second_code = compact_unit_direction_sign_extend_12(packed >> 12U);

    float chart_x =
        (float)first_code / (float)COMPACT_UNIT_DIRECTION_SCALE;
    float chart_y =
        (float)second_code / (float)COMPACT_UNIT_DIRECTION_SCALE;
    float chart_z = 1.0F - fabsf(chart_x) - fabsf(chart_y);

    if (chart_z < 0.0F) {
        float folded_x = chart_x;
        float folded_y = chart_y;
        chart_x =
            (1.0F - fabsf(folded_y)) *
            compact_unit_direction_sign_not_zero(folded_x);
        chart_y =
            (1.0F - fabsf(folded_x)) *
            compact_unit_direction_sign_not_zero(folded_y);
    }

    float norm =
        sqrtf(chart_x * chart_x + chart_y * chart_y + chart_z * chart_z);
    unit_direction->x = chart_x / norm;
    unit_direction->y = chart_y / norm;
    unit_direction->z = chart_z / norm;
}

#endif
