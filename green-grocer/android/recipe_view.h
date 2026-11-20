#ifndef GREEN_GROCER_RECIPE_VIEW_H
#define GREEN_GROCER_RECIPE_VIEW_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Transport/presentation only. UTF-16 lengths are explicit; NUL is data. */
typedef struct { uint16_t *characters; size_t length; } gg_text;
typedef struct { const uint16_t *text; size_t units, cursor; } gg_recipe_fields;
/* Borrow one scalar-framed field, preserving explicit UTF-16 length. */
bool gg_recipe_field(gg_recipe_fields *input, gg_recipe_fields *output);
typedef struct { gg_text label, token; } gg_recipe_control;
typedef struct {
    gg_text text;
    gg_recipe_control *controls;
    size_t control_count;
} gg_recipe_row;
typedef struct {
    gg_text title, notice;
    gg_recipe_row *rows;
    size_t row_count;
} gg_recipe_view;

/* A renderer budget, never a quantity/identity bound in the recipe model. */
#define GG_RECIPE_VIEW_UNITS 1048576u
#define GG_RECIPE_VIEW_ROWS 512u
#define GG_RECIPE_ROW_CONTROLS 3u

bool gg_recipe_view_decode(const uint16_t *input, size_t units,
                           gg_recipe_view *destination);
void gg_recipe_view_dispose(gg_recipe_view *view);
bool gg_text_is_ascii(const gg_text *text, const char *ascii);

#endif
