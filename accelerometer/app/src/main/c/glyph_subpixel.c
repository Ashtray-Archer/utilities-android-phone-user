#include "glyph_subpixel.h"
#include "subpixel/font.h"

/* Compatibility entry point for the original independent slash regression.
 * The geometry and sampler now live entirely in rough.framebuffer. */
bool subpixel_slash_mask(uint8_t *coverage, unsigned scale)
{
    if (scale < 2 || scale > 20) return false;
    return sf57_render_coverage('/', scale, 0, 0, coverage,
        35U * (size_t)scale * (size_t)scale) != 0;
}
