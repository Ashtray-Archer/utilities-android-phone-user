#include "recipe_view.h"

#include <stdlib.h>
#include <string.h>

typedef gg_recipe_fields fields;

bool gg_recipe_field(fields *input, fields *output) {
    size_t characters = 0;
    bool digit = false;
    while (input->cursor < input->units) {
        uint16_t value = input->text[input->cursor++];
        if (value == ':') {
            if (!digit) return false;
            size_t start = input->cursor;
            for (size_t index = 0; index < characters; ++index) {
                if (input->cursor == input->units) return false;
                uint16_t character = input->text[input->cursor++];
                if (character >= 0xD800 && character <= 0xDBFF) {
                    if (input->cursor == input->units) return false;
                    uint16_t low = input->text[input->cursor++];
                    if (low < 0xDC00 || low > 0xDFFF) return false;
                } else if (character >= 0xDC00 && character <= 0xDFFF) return false;
            }
            *output = (fields){input->text + start, input->cursor - start, 0};
            return true;
        }
        if (value < '0' || value > '9' || characters > (SIZE_MAX - (value - '0')) / 10)
            return false;
        digit = true;
        characters = characters * 10 + (value - '0');
        if (characters > input->units) return false;
    }
    return false;
}

static bool text_field(fields *input, gg_text *output) {
    fields value;
    if (!gg_recipe_field(input, &value)) return false;
    uint16_t *copy = malloc((value.units + 1) * sizeof(*copy));
    if (copy == NULL) return false;
    if (value.units != 0) memcpy(copy, value.text, value.units * sizeof(*copy));
    copy[value.units] = 0;
    *output = (gg_text){copy, value.units};
    return true;
}

bool gg_text_is_ascii(const gg_text *text, const char *ascii) {
    size_t length = strlen(ascii);
    if (text->length != length) return false;
    for (size_t index = 0; index < length; ++index)
        if (text->characters[index] != (unsigned char)ascii[index]) return false;
    return true;
}

void gg_recipe_view_dispose(gg_recipe_view *view) {
    free(view->title.characters);
    free(view->notice.characters);
    for (size_t row = 0; row < view->row_count; ++row) {
        gg_recipe_row *current = &view->rows[row];
        free(current->text.characters);
        for (size_t control = 0; control < current->control_count; ++control) {
            free(current->controls[control].label.characters);
            free(current->controls[control].token.characters);
        }
        free(current->controls);
    }
    free(view->rows);
    memset(view, 0, sizeof(*view));
}

static bool controls(fields *input, gg_recipe_row *row) {
    while (input->cursor != input->units) {
        if (row->control_count == GG_RECIPE_ROW_CONTROLS) return false;
        gg_recipe_control *grown = realloc(row->controls,
            (row->control_count + 1) * sizeof(*grown));
        if (grown == NULL) return false;
        row->controls = grown;
        gg_recipe_control *button = &grown[row->control_count++];
        memset(button, 0, sizeof(*button));
        fields pair;
        if (!gg_recipe_field(input, &pair) || !text_field(&pair, &button->label) ||
            !text_field(&pair, &button->token) || pair.cursor != pair.units ||
            button->token.length == 0) return false;
    }
    return true;
}

bool gg_recipe_view_decode(const uint16_t *input, size_t units,
                           gg_recipe_view *destination) {
    if (input == NULL || destination == NULL || units > GG_RECIPE_VIEW_UNITS) return false;
    gg_recipe_view candidate = {0};
    gg_text version = {0};
    fields top = {input, units, 0}, rows;
    bool valid = false;
    if (!text_field(&top, &version) || !gg_text_is_ascii(&version, "green-grocer-view-v1") ||
        !text_field(&top, &candidate.title) || !text_field(&top, &candidate.notice) ||
        !gg_recipe_field(&top, &rows) || top.cursor != top.units) goto done;
    while (rows.cursor != rows.units) {
        if (candidate.row_count == GG_RECIPE_VIEW_ROWS) goto done;
        gg_recipe_row *grown = realloc(candidate.rows,
            (candidate.row_count + 1) * sizeof(*grown));
        if (grown == NULL) goto done;
        candidate.rows = grown;
        gg_recipe_row *row = &grown[candidate.row_count++];
        memset(row, 0, sizeof(*row));
        fields encoded, buttons;
        if (!gg_recipe_field(&rows, &encoded) || !text_field(&encoded, &row->text) ||
            !gg_recipe_field(&encoded, &buttons) || encoded.cursor != encoded.units ||
            !controls(&buttons, row)) goto done;
    }
    gg_recipe_view_dispose(destination);
    *destination = candidate;
    memset(&candidate, 0, sizeof(candidate));
    valid = true;
done:
    free(version.characters);
    gg_recipe_view_dispose(&candidate);
    return valid;
}
