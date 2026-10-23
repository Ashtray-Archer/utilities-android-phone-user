#ifndef ACCELEROMETER_GLYPH_SUBPIXEL_H
#define ACCELEROMETER_GLYPH_SUBPIXEL_H

#include <stdbool.h>
#include <stdint.h>

/* One 5x7-cell '/' master rendered as a continuous straight stroke.
 * Output is 5*scale by 7*scale, row-major 8-bit pixel coverage.
 * The caller owns all memory. Failure never changes coverage.
 * No Android types enter this reusable text-rendering boundary. */
bool subpixel_slash_mask(uint8_t *coverage, unsigned scale);

#endif
