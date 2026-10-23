#include "glyph_subpixel.h"

#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures;

static void require(bool condition, const char *description)
{
    if (!condition) {
        fprintf(stderr, "FAIL: %s\n", description);
        ++failures;
    }
}

/* Independent oracle: distance to an exact finite Euclidean line segment
 * at every 8x8 pixel midpoint. No Subpixel patch/whole-path code is used. */
static uint8_t analytic_coverage(unsigned scale, unsigned pixel_x, unsigned pixel_y)
{
    const double ax = 0.60 * (double)scale - 0.5;
    const double ay = 6.35 * (double)scale - 0.5;
    const double bx = 4.40 * (double)scale - 0.5;
    const double by = 0.65 * (double)scale - 0.5;
    const double dx = bx - ax, dy = by - ay;
    const double length_squared = dx * dx + dy * dy;
    const double radius_squared = (0.36 * (double)scale) *
                                  (0.36 * (double)scale);
    unsigned inside = 0U;
    for (unsigned sy = 0U; sy < 8U; ++sy) {
        for (unsigned sx = 0U; sx < 8U; ++sx) {
            const double px = (double)pixel_x + ((double)sx + 0.5) / 8.0 - 0.5;
            const double py = (double)pixel_y + ((double)sy + 0.5) / 8.0 - 0.5;
            double t = ((px - ax) * dx + (py - ay) * dy) / length_squared;
            if (t < 0.0) t = 0.0;
            if (t > 1.0) t = 1.0;
            const double ex = px - (ax + t * dx);
            const double ey = py - (ay + t * dy);
            if (ex * ex + ey * ey <= radius_squared) {
                ++inside;
            }
        }
    }
    return (uint8_t)((inside * 255U + 32U) / 64U);
}

static void compare_at_scale(unsigned scale)
{
    const unsigned width = 5U * scale, height = 7U * scale;
    const size_t count = (size_t)width * (size_t)height;
    uint8_t *raster = (uint8_t *)malloc(count);
    require(raster != NULL, "glyph raster allocation succeeds");
    if (raster == NULL) return;

    memset(raster, 0xa5, count);
    require(subpixel_slash_mask(raster, scale), "Subpixel slash renders");
    unsigned covered_pixels = 0U;
    unsigned partial_pixels = 0U;
    unsigned worst_difference = 0U;
    for (unsigned y = 0U; y < height; ++y) {
        for (unsigned x = 0U; x < width; ++x) {
            const size_t index = (size_t)y * width + x;
            const unsigned actual = raster[index];
            const unsigned expected = analytic_coverage(scale, x, y);
            unsigned difference = actual > expected ? actual - expected : expected - actual;
            if (difference > worst_difference) worst_difference = difference;
            if (actual != 0U) ++covered_pixels;
            if (actual > 0U && actual < 255U) ++partial_pixels;
        }
    }
    /* Two 8x8 sample disagreements correspond to at most eight 8-bit
     * coverage levels after rounding, as declared by the path producer. */
    require(worst_difference <= 8U, "slash follows exact straight-line tube");
    require(covered_pixels > 0U, "slash is visible");
    require(partial_pixels > 0U, "diagonal has fractional edge coverage");
    free(raster);
}

int main(void)
{
    const unsigned scales[] = {3U, 5U, 8U, 10U, 14U};
    for (size_t i = 0U; i < sizeof(scales) / sizeof(scales[0]); ++i) {
        compare_at_scale(scales[i]);
    }

    uint8_t unchanged[35];
    memset(unchanged, 0x5a, sizeof unchanged);
    require(!subpixel_slash_mask(unchanged, 0U), "zero scale is refused");
    require(!subpixel_slash_mask(unchanged, 21U), "oversize scale is refused");
    require(!subpixel_slash_mask(NULL, 3U), "null output is refused");
    for (size_t i = 0U; i < sizeof unchanged; ++i) {
        require(unchanged[i] == 0x5aU, "failed calls do not modify output");
    }

    if (failures != 0) {
        fprintf(stderr, "%d slash raster test(s) failed\n", failures);
        return 1;
    }
    puts("PASS continuous straight slash with 8x8 Subpixel coverage");
    return 0;
}
