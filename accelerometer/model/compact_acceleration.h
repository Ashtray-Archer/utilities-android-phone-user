#ifndef COMPACT_ACCELERATION_H
#define COMPACT_ACCELERATION_H

#include <math.h>
#include <stdbool.h>
#include <stdint.h>

struct physical_acceleration {
    float x;
    float y;
    float z;
};

/*
 * The residual direction is an octahedral chart of S^2.  Each chart
 * coordinate is signed Q0.11, packed as two adjacent 12-bit two's-complement
 * integers.  The fourth byte is exactly the magnitude code m, with
 *
 *     residual magnitude = m / (4 sqrt(3)) m/s^2.
 *
 * Two Q0.7 direction bytes were the intended three-byte design.  The error
 * study in accelerometer/tests/error_study.c shows that their worst component
 * error exceeds the screen's 1/7 m/s^2 quantum.  Q0.11 is the smallest simple
 * dyadic octahedral lattice that leaves comfortable error margin across the
 * full magnitude-byte range; byte alignment therefore makes the retained
 * state four bytes.
 */
struct compact_acceleration {
    uint8_t direction_low;
    uint8_t direction_middle;
    uint8_t direction_high;
    uint8_t magnitude;
};

_Static_assert(
    sizeof(struct compact_acceleration) == 4U,
    "compact acceleration must occupy exactly four bytes");

enum compact_acceleration_encode_status {
    COMPACT_ACCELERATION_ENCODE_OK = 0,
    COMPACT_ACCELERATION_ENCODE_SATURATED,
    COMPACT_ACCELERATION_ENCODE_NONFINITE
};

enum compact_acceleration_decode_status {
    COMPACT_ACCELERATION_DECODE_OK = 0,
    COMPACT_ACCELERATION_DECODE_NONCANONICAL_ZERO
};

enum {
    COMPACT_DIRECTION_FRACTION_BITS = 11,
    COMPACT_DIRECTION_SCALE = 1 << COMPACT_DIRECTION_FRACTION_BITS,
    COMPACT_DIRECTION_MAXIMUM_CODE = COMPACT_DIRECTION_SCALE - 1,
    COMPACT_DIRECTION_MINIMUM_CODE = -COMPACT_DIRECTION_SCALE
};

static inline float compact_acceleration_root_three(void)
{
    return sqrtf(3.0F);
}

static inline float compact_acceleration_magnitude_quantum(void)
{
    return 1.0F / (4.0F * compact_acceleration_root_three());
}

static inline struct physical_acceleration compact_acceleration_balanced_reference(void)
{
    float component = -10.0F / compact_acceleration_root_three();
    return (struct physical_acceleration){component, component, component};
}


static inline struct physical_acceleration acceleration_difference_from_balanced_gravity(
    struct physical_acceleration measured)
{
    struct physical_acceleration reference = compact_acceleration_balanced_reference();
    return (struct physical_acceleration){
        measured.x - reference.x,
        measured.y - reference.y,
        measured.z - reference.z};
}

static inline float physical_acceleration_magnitude(struct physical_acceleration value)
{
    float scale = fmaxf(fabsf(value.x), fmaxf(fabsf(value.y), fabsf(value.z)));
    if (scale == 0.0F) {
        return 0.0F;
    }

    float scaled_x = value.x / scale;
    float scaled_y = value.y / scale;
    float scaled_z = value.z / scale;
    return scale * sqrtf(
        scaled_x * scaled_x +
        scaled_y * scaled_y +
        scaled_z * scaled_z);
}

static inline struct compact_acceleration compact_acceleration_canonical_zero(void)
{
    return (struct compact_acceleration){0U, 0U, 0U, 0U};
}

static inline float compact_acceleration_sign_not_zero(float value)
{
    return value < 0.0F ? -1.0F : 1.0F;
}

static inline int32_t compact_acceleration_quantize_direction_coordinate(float coordinate)
{
    float scaled = coordinate * (float)COMPACT_DIRECTION_SCALE;
    int32_t code = (int32_t)roundf(scaled);
    if (code < COMPACT_DIRECTION_MINIMUM_CODE) {
        return COMPACT_DIRECTION_MINIMUM_CODE;
    }
    if (code > COMPACT_DIRECTION_MAXIMUM_CODE) {
        return COMPACT_DIRECTION_MAXIMUM_CODE;
    }
    return code;
}

static inline uint32_t compact_acceleration_twos_complement_12(int32_t code)
{
    return (uint32_t)code & 0x0fffU;
}

static inline int32_t compact_acceleration_sign_extend_12(uint32_t bits)
{
    bits &= 0x0fffU;
    return (bits & 0x0800U) != 0U ? (int32_t)bits - 0x1000 : (int32_t)bits;
}

static inline void compact_acceleration_encode_direction(
    float unit_x,
    float unit_y,
    float unit_z,
    struct compact_acceleration *encoded)
{
    float l1_norm = fabsf(unit_x) + fabsf(unit_y) + fabsf(unit_z);
    float chart_x = unit_x / l1_norm;
    float chart_y = unit_y / l1_norm;
    float chart_z = unit_z / l1_norm;

    if (chart_z < 0.0F) {
        float unfolded_x = chart_x;
        float unfolded_y = chart_y;
        chart_x =
            (1.0F - fabsf(unfolded_y)) * compact_acceleration_sign_not_zero(unfolded_x);
        chart_y =
            (1.0F - fabsf(unfolded_x)) * compact_acceleration_sign_not_zero(unfolded_y);
    }

    int32_t first_code = compact_acceleration_quantize_direction_coordinate(chart_x);
    int32_t second_code = compact_acceleration_quantize_direction_coordinate(chart_y);
    uint32_t packed = compact_acceleration_twos_complement_12(first_code) |
        (compact_acceleration_twos_complement_12(second_code) << 12U);
    encoded->direction_low = (uint8_t)(packed & 0xffU);
    encoded->direction_middle = (uint8_t)((packed >> 8U) & 0xffU);
    encoded->direction_high = (uint8_t)((packed >> 16U) & 0xffU);
}

static inline void compact_acceleration_decode_direction(
    const struct compact_acceleration *encoded,
    struct physical_acceleration *unit_direction)
{
    uint32_t packed = (uint32_t)encoded->direction_low |
        ((uint32_t)encoded->direction_middle << 8U) |
        ((uint32_t)encoded->direction_high << 16U);
    int32_t first_code = compact_acceleration_sign_extend_12(packed);
    int32_t second_code = compact_acceleration_sign_extend_12(packed >> 12U);
    float chart_x = (float)first_code / (float)COMPACT_DIRECTION_SCALE;
    float chart_y = (float)second_code / (float)COMPACT_DIRECTION_SCALE;
    float chart_z = 1.0F - fabsf(chart_x) - fabsf(chart_y);

    if (chart_z < 0.0F) {
        float folded_x = chart_x;
        float folded_y = chart_y;
        chart_x =
            (1.0F - fabsf(folded_y)) * compact_acceleration_sign_not_zero(folded_x);
        chart_y =
            (1.0F - fabsf(folded_x)) * compact_acceleration_sign_not_zero(folded_y);
    }

    float norm = sqrtf(chart_x * chart_x + chart_y * chart_y + chart_z * chart_z);
    unit_direction->x = chart_x / norm;
    unit_direction->y = chart_y / norm;
    unit_direction->z = chart_z / norm;
}

static inline enum compact_acceleration_encode_status compact_acceleration_encode(
    struct physical_acceleration measured,
    struct compact_acceleration *encoded)
{
    if (!isfinite(measured.x) || !isfinite(measured.y) || !isfinite(measured.z)) {
        return COMPACT_ACCELERATION_ENCODE_NONFINITE;
    }

    struct physical_acceleration difference =
        acceleration_difference_from_balanced_gravity(measured);
    float residual_magnitude = physical_acceleration_magnitude(difference);

    if (residual_magnitude == 0.0F) {
        *encoded = compact_acceleration_canonical_zero();
        return COMPACT_ACCELERATION_ENCODE_OK;
    }

    compact_acceleration_encode_direction(
        difference.x / residual_magnitude,
        difference.y / residual_magnitude,
        difference.z / residual_magnitude,
        encoded);

    float maximum_magnitude = 255.0F * compact_acceleration_magnitude_quantum();
    if (!isfinite(residual_magnitude) || residual_magnitude > maximum_magnitude) {
        encoded->magnitude = 255U;
        return COMPACT_ACCELERATION_ENCODE_SATURATED;
    }

    float magnitude_codes =
        residual_magnitude * 4.0F * compact_acceleration_root_three();
    uint8_t magnitude = (uint8_t)floorf(magnitude_codes + 0.5F);
    if (magnitude == 0U) {
        *encoded = compact_acceleration_canonical_zero();
    } else {
        encoded->magnitude = magnitude;
    }
    return COMPACT_ACCELERATION_ENCODE_OK;
}

static inline enum compact_acceleration_decode_status compact_acceleration_decode(
    const struct compact_acceleration *encoded,
    struct physical_acceleration *reconstructed)
{
    struct physical_acceleration reference = compact_acceleration_balanced_reference();
    if (encoded->magnitude == 0U) {
        if (encoded->direction_low != 0U ||
            encoded->direction_middle != 0U ||
            encoded->direction_high != 0U) {
            return COMPACT_ACCELERATION_DECODE_NONCANONICAL_ZERO;
        }
        *reconstructed = reference;
        return COMPACT_ACCELERATION_DECODE_OK;
    }

    struct physical_acceleration unit_direction;
    compact_acceleration_decode_direction(encoded, &unit_direction);
    float residual_magnitude =
        (float)encoded->magnitude * compact_acceleration_magnitude_quantum();
    reconstructed->x = reference.x + residual_magnitude * unit_direction.x;
    reconstructed->y = reference.y + residual_magnitude * unit_direction.y;
    reconstructed->z = reference.z + residual_magnitude * unit_direction.z;
    return COMPACT_ACCELERATION_DECODE_OK;
}

#endif
