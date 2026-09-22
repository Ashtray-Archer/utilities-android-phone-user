#include "compact_acceleration.h"

#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

struct error_summary {
    float maximum_component_error;
    double squared_component_error_sum;
    uint64_t sample_count;
    float maximum_acceleration_magnitude_error;
    float maximum_residual_magnitude_error;
    struct physical_acceleration worst_direction;
    float worst_residual_magnitude;
};

static float acceleration_norm(struct physical_acceleration value)
{
    return sqrtf(value.x * value.x + value.y * value.y + value.z * value.z);
}

static struct physical_acceleration acceleration_subtract(
    struct physical_acceleration first,
    struct physical_acceleration second)
{
    return (struct physical_acceleration){
        first.x - second.x,
        first.y - second.y,
        first.z - second.z};
}

static struct physical_acceleration residual_point(
    struct physical_acceleration direction,
    float magnitude)
{
    struct physical_acceleration reference = compact_acceleration_balanced_reference();
    return (struct physical_acceleration){
        reference.x + magnitude * direction.x,
        reference.y + magnitude * direction.y,
        reference.z + magnitude * direction.z};
}

static void observe_error(
    struct error_summary *summary,
    struct physical_acceleration raw,
    struct physical_acceleration decoded)
{
    struct physical_acceleration error = acceleration_subtract(decoded, raw);
    float component_error = fmaxf(
        fabsf(error.x),
        fmaxf(fabsf(error.y), fabsf(error.z)));
    struct physical_acceleration reference = compact_acceleration_balanced_reference();
    struct physical_acceleration raw_residual = acceleration_subtract(raw, reference);
    struct physical_acceleration decoded_residual = acceleration_subtract(decoded, reference);
    float raw_residual_magnitude = acceleration_norm(raw_residual);
    float decoded_residual_magnitude = acceleration_norm(decoded_residual);

    summary->squared_component_error_sum +=
        (double)error.x * (double)error.x +
        (double)error.y * (double)error.y +
        (double)error.z * (double)error.z;
    summary->sample_count += 1U;

    if (component_error > summary->maximum_component_error) {
        summary->maximum_component_error = component_error;
        summary->worst_residual_magnitude = raw_residual_magnitude;
        if (raw_residual_magnitude == 0.0F) {
            summary->worst_direction = (struct physical_acceleration){0.0F, 0.0F, 0.0F};
        } else {
            summary->worst_direction = (struct physical_acceleration){
                raw_residual.x / raw_residual_magnitude,
                raw_residual.y / raw_residual_magnitude,
                raw_residual.z / raw_residual_magnitude};
        }
    }

    float acceleration_magnitude_error =
        fabsf(acceleration_norm(decoded) - acceleration_norm(raw));
    if (acceleration_magnitude_error > summary->maximum_acceleration_magnitude_error) {
        summary->maximum_acceleration_magnitude_error = acceleration_magnitude_error;
    }

    float residual_magnitude_error = fabsf(decoded_residual_magnitude - raw_residual_magnitude);
    if (residual_magnitude_error > summary->maximum_residual_magnitude_error) {
        summary->maximum_residual_magnitude_error = residual_magnitude_error;
    }
}

static struct physical_acceleration decode_q0_7_direction(
    struct physical_acceleration direction)
{
    float l1_norm = fabsf(direction.x) + fabsf(direction.y) + fabsf(direction.z);
    float chart_x = direction.x / l1_norm;
    float chart_y = direction.y / l1_norm;
    float chart_z = direction.z / l1_norm;
    if (chart_z < 0.0F) {
        float unfolded_x = chart_x;
        float unfolded_y = chart_y;
        chart_x =
            (1.0F - fabsf(unfolded_y)) * compact_acceleration_sign_not_zero(unfolded_x);
        chart_y =
            (1.0F - fabsf(unfolded_x)) * compact_acceleration_sign_not_zero(unfolded_y);
    }

    int32_t first_code = (int32_t)roundf(128.0F * chart_x);
    int32_t second_code = (int32_t)roundf(128.0F * chart_y);
    if (first_code < -128) {
        first_code = -128;
    } else if (first_code > 127) {
        first_code = 127;
    }
    if (second_code < -128) {
        second_code = -128;
    } else if (second_code > 127) {
        second_code = 127;
    }

    chart_x = (float)first_code / 128.0F;
    chart_y = (float)second_code / 128.0F;
    chart_z = 1.0F - fabsf(chart_x) - fabsf(chart_y);
    if (chart_z < 0.0F) {
        float folded_x = chart_x;
        float folded_y = chart_y;
        chart_x =
            (1.0F - fabsf(folded_y)) * compact_acceleration_sign_not_zero(folded_x);
        chart_y =
            (1.0F - fabsf(folded_x)) * compact_acceleration_sign_not_zero(folded_y);
    }
    float direction_norm =
        sqrtf(chart_x * chart_x + chart_y * chart_y + chart_z * chart_z);
    return (struct physical_acceleration){
        chart_x / direction_norm,
        chart_y / direction_norm,
        chart_z / direction_norm};
}

static struct physical_acceleration reconstruct_three_byte_candidate(
    struct physical_acceleration raw)
{
    struct physical_acceleration reference = compact_acceleration_balanced_reference();
    struct physical_acceleration residual = acceleration_subtract(raw, reference);
    float residual_magnitude = acceleration_norm(residual);
    float magnitude_codes =
        residual_magnitude * 4.0F * compact_acceleration_root_three();
    uint32_t magnitude_code = (uint32_t)floorf(magnitude_codes + 0.5F);
    if (magnitude_code == 0U) {
        return reference;
    }
    if (magnitude_code > 255U) {
        magnitude_code = 255U;
    }
    struct physical_acceleration direction = {
        residual.x / residual_magnitude,
        residual.y / residual_magnitude,
        residual.z / residual_magnitude};
    struct physical_acceleration decoded_direction = decode_q0_7_direction(direction);
    float decoded_magnitude =
        (float)magnitude_code * compact_acceleration_magnitude_quantum();
    return residual_point(decoded_direction, decoded_magnitude);
}

static bool reconstruct_production(
    struct physical_acceleration raw,
    struct physical_acceleration *decoded)
{
    struct compact_acceleration encoded;
    enum compact_acceleration_encode_status encode_status =
        compact_acceleration_encode(raw, &encoded);
    if (encode_status != COMPACT_ACCELERATION_ENCODE_OK) {
        return false;
    }
    return compact_acceleration_decode(&encoded, decoded) == COMPACT_ACCELERATION_DECODE_OK;
}

static struct physical_acceleration fibonacci_direction(uint32_t index, uint32_t count)
{
    float pi = acosf(-1.0F);
    float golden_angle = pi * (3.0F - sqrtf(5.0F));
    float unit_z = 1.0F - 2.0F * ((float)index + 0.5F) / (float)count;
    float radius = sqrtf(fmaxf(0.0F, 1.0F - unit_z * unit_z));
    float angle = fmodf((float)index * golden_angle, 2.0F * pi);
    return (struct physical_acceleration){
        radius * cosf(angle),
        radius * sinf(angle),
        unit_z};
}

static bool compare_one(
    struct error_summary *candidate_summary,
    struct error_summary *production_summary,
    struct physical_acceleration raw)
{
    struct physical_acceleration production;
    if (!reconstruct_production(raw, &production)) {
        return false;
    }
    struct physical_acceleration candidate = reconstruct_three_byte_candidate(raw);
    observe_error(candidate_summary, raw, candidate);
    observe_error(production_summary, raw, production);
    return true;
}

static bool sweep_full_residual_range(
    struct error_summary *candidate_summary,
    struct error_summary *production_summary)
{
    const uint32_t direction_count = 32768U;
    float quantum = compact_acceleration_magnitude_quantum();
    for (uint32_t direction_index = 0U;
         direction_index < direction_count;
         ++direction_index) {
        struct physical_acceleration direction =
            fibonacci_direction(direction_index, direction_count);
        for (uint32_t magnitude_code = 0U; magnitude_code <= 255U; ++magnitude_code) {
            float raw_magnitude = magnitude_code == 255U
                ? 254.51F * quantum
                : ((float)magnitude_code + 0.49F) * quantum;
            if (!compare_one(
                    candidate_summary,
                    production_summary,
                    residual_point(direction, raw_magnitude))) {
                return false;
            }
        }
    }
    return true;
}

static bool sweep_gravity_sphere(
    struct error_summary *candidate_summary,
    struct error_summary *production_summary)
{
    const uint32_t direction_count = 131072U;
    for (uint32_t index = 0U; index < direction_count; ++index) {
        struct physical_acceleration direction = fibonacci_direction(index, direction_count);
        struct physical_acceleration gravity = {
            10.0F * direction.x,
            10.0F * direction.y,
            10.0F * direction.z};
        if (!compare_one(candidate_summary, production_summary, gravity)) {
            return false;
        }
    }
    return true;
}

static bool sweep_hand_scale_translations(
    struct error_summary *candidate_summary,
    struct error_summary *production_summary)
{
    float component = 10.0F / compact_acceleration_root_three();
    static const struct physical_acceleration axis_gravity[] = {
        {10.0F, 0.0F, 0.0F},
        {-10.0F, 0.0F, 0.0F},
        {0.0F, 10.0F, 0.0F},
        {0.0F, -10.0F, 0.0F},
        {0.0F, 0.0F, 10.0F},
        {0.0F, 0.0F, -10.0F},
    };
    struct physical_acceleration balanced_gravity = {-component, -component, -component};

    for (size_t base_index = 0U;
         base_index <= sizeof(axis_gravity) / sizeof(axis_gravity[0]);
         ++base_index) {
        struct physical_acceleration base = base_index == 0U
            ? balanced_gravity
            : axis_gravity[base_index - 1U];
        for (int32_t translation_x = -5; translation_x <= 5; ++translation_x) {
            for (int32_t translation_y = -5; translation_y <= 5; ++translation_y) {
                for (int32_t translation_z = -5; translation_z <= 5; ++translation_z) {
                    struct physical_acceleration raw = {
                        base.x + (float)translation_x,
                        base.y + (float)translation_y,
                        base.z + (float)translation_z};
                    if (!compare_one(candidate_summary, production_summary, raw)) {
                        return false;
                    }
                }
            }
        }
    }
    return true;
}

static bool sweep_chart_boundaries(
    struct error_summary *candidate_summary,
    struct error_summary *production_summary)
{
    const uint32_t boundary_count = 65536U;
    float pi = acosf(-1.0F);
    float seam_offset = 1.0F / 65536.0F;
    float maximum_magnitude = 254.51F * compact_acceleration_magnitude_quantum();
    for (uint32_t index = 0U; index < boundary_count; ++index) {
        float angle = 2.0F * pi * (float)index / (float)boundary_count;
        for (int32_t side = -1; side <= 1; side += 2) {
            struct physical_acceleration almost_seam = {
                cosf(angle),
                sinf(angle),
                (float)side * seam_offset};
            float seam_norm = acceleration_norm(almost_seam);
            struct physical_acceleration direction = {
                almost_seam.x / seam_norm,
                almost_seam.y / seam_norm,
                almost_seam.z / seam_norm};
            if (!compare_one(
                    candidate_summary,
                    production_summary,
                    residual_point(direction, maximum_magnitude))) {
                return false;
            }
        }
    }
    return true;
}

static void print_summary(const char *name, const struct error_summary *summary)
{
    double rms_component_error = sqrt(
        summary->squared_component_error_sum /
        (3.0 * (double)summary->sample_count));
    (void)printf("%s\n", name);
    (void)printf("  samples=%llu\n", (unsigned long long)summary->sample_count);
    (void)printf("  maximum_component_error=%.6f m/s^2\n", summary->maximum_component_error);
    (void)printf("  rms_component_error=%.6f m/s^2\n", rms_component_error);
    (void)printf(
        "  maximum_acceleration_magnitude_error=%.6f m/s^2\n",
        summary->maximum_acceleration_magnitude_error);
    (void)printf(
        "  maximum_residual_magnitude_error=%.6f m/s^2\n",
        summary->maximum_residual_magnitude_error);
    (void)printf(
        "  worst_residual_direction=(%.6f, %.6f, %.6f)\n",
        summary->worst_direction.x,
        summary->worst_direction.y,
        summary->worst_direction.z);
    (void)printf("  worst_residual_magnitude=%.6f m/s^2\n", summary->worst_residual_magnitude);
}

int main(void)
{
    struct error_summary candidate = {0};
    struct error_summary production = {0};
    bool completed =
        sweep_full_residual_range(&candidate, &production) &&
        sweep_gravity_sphere(&candidate, &production) &&
        sweep_hand_scale_translations(&candidate, &production) &&
        sweep_chart_boundaries(&candidate, &production);
    if (!completed) {
        (void)fprintf(stderr, "FAIL: the production codec rejected an in-range sweep sample\n");
        return 1;
    }

    float screen_quantum = 1.0F / 7.0F;
    (void)printf("screen_quantum=%.6f m/s^2\n", screen_quantum);
    print_summary("rejected_three_byte_Q0.7_candidate", &candidate);
    print_summary("production_four_byte_Q0.11_codec", &production);

    if (candidate.maximum_component_error <= screen_quantum) {
        (void)fprintf(stderr, "FAIL: sweep did not expose the Q0.7 candidate's precision loss\n");
        return 1;
    }
    if (production.maximum_component_error >= 0.75F * screen_quantum) {
        (void)fprintf(
            stderr,
            "FAIL: production maximum component error lacks a 25 percent screen-quantum margin\n");
        return 1;
    }
    if (production.maximum_residual_magnitude_error >
        0.5001F * compact_acceleration_magnitude_quantum()) {
        (void)fprintf(stderr, "FAIL: magnitude error exceeds half its exact quantum\n");
        return 1;
    }

    (void)printf("PASS production error remains comfortably below one seventh\n");
    return 0;
}
