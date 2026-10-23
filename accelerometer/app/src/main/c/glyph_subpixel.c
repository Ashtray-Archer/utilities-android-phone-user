#include "glyph_subpixel.h"

#include "subpixel/path.h"

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

/* A directly authored stroke, not a trace or interpolation of 5x7 bitmap
 * samples. The baseline source is deliberately a single straight line.
 * Round endpoints stay inside the five-by-seven cell box. */
#define SLASH_START_X 0.60
#define SLASH_START_Y 6.35
#define SLASH_END_X 4.40
#define SLASH_END_Y 0.65
#define SLASH_WIDTH 0.72

static unsigned count_bits(uint64_t value)
{
    unsigned result = 0U;
    while (value != UINT64_C(0)) {
        value &= value - UINT64_C(1);
        ++result;
    }
    return result;
}

bool subpixel_slash_mask(uint8_t *coverage, unsigned scale)
{
    if (coverage == NULL || scale < 2U || scale > 20U) {
        return false;
    }

    const unsigned width = 5U * scale;
    const unsigned height = 7U * scale;
    const size_t pixel_count = (size_t)width * (size_t)height;

    /* Three disjoint 64-sample planes: destination and the whole-path
     * producer's candidate and independent-reference work areas. */
    uint64_t *masks = (uint64_t *)calloc(3U * pixel_count, sizeof(uint64_t));
    if (masks == NULL) {
        return false;
    }

    cp_coverage_plane destination = {
        .occupied = masks,
        .capacity_words = pixel_count,
        .stride_words = width,
        .width = width,
        .height = height,
        .origin_x = 0,
        .origin_y = 0
    };
    cp_path_workspace workspace = {
        .candidate = masks + pixel_count,
        .reference = masks + 2U * pixel_count,
        .capacity_words = pixel_count
    };

    const cp_path_segment slash = {
        .degree = 1U,
        .controls = {
            {SLASH_START_X, SLASH_START_Y},
            {SLASH_END_X, SLASH_END_Y}
        }
    };
    /* Subpixel pixel centers are at integer coordinates. Material cell
     * boundaries therefore map to screen coordinates ending in -0.5. */
    const cp_path_options options = {
        .transform = {(double)scale, 0.0, 0.0, (double)scale, -0.5, -0.5},
        .width = SLASH_WIDTH * (double)scale,
        .tolerance = 0.01,
        .max_pixel_error = 2U,
        .max_depth = 20U,
        .max_patches = 50000U,
        .max_sample_tests = 20000000U,
        .strategy = CP_PATH_AUTO,
        .cancelled = NULL,
        .cancel_context = NULL
    };
    cp_path_report report;
    cp_path_status status = cp_path_rasterize(
        &destination, &slash, 1U, &options, &workspace, &report);

    if (status == CP_PATH_OK) {
        /* Resolve once per glyph, after accumulating all sample masks.
         * The window's foreground color is still composed by the caller. */
        for (size_t index = 0U; index < pixel_count; ++index) {
            unsigned samples = count_bits(masks[index]);
            coverage[index] = (uint8_t)((samples * 255U + 32U) / 64U);
        }
    }

    free(masks);
    return status == CP_PATH_OK;
}
