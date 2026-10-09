#include "pad_ui.h"

#include <stdbool.h>

#define TOOLBAR_COUNT 6u

static PadRect make_rect(float left, float top, float right, float bottom) {
    const PadRect rect = {left, top, right, bottom};
    return rect;
}

static bool contains(PadRect rect, float x, float y) {
    return x >= rect.left && x <= rect.right && y >= rect.top && y <= rect.bottom;
}

PadRect pad_ui_previous_layout_rect(float width, float height) {
    return make_rect(0.012f * width, 0.012f * height, 0.16f * width, 0.074f * height);
}

PadRect pad_ui_next_layout_rect(float width, float height) {
    return make_rect(0.84f * width, 0.012f * height, 0.988f * width, 0.074f * height);
}

PadRect pad_ui_title_rect(float width, float height) {
    return make_rect(0.17f * width, 0.012f * height, 0.83f * width, 0.074f * height);
}

PadRect pad_ui_buffer_rect(float width, float height) {
    return make_rect(0.012f * width, 0.082f * height, 0.988f * width, 0.205f * height);
}

PadRect pad_ui_toolbar_rect(float width, float height, size_t index) {
    if (index >= TOOLBAR_COUNT) {
        return make_rect(0.0f, 0.0f, 0.0f, 0.0f);
    }
    const float cell = width ÷ (float)TOOLBAR_COUNT;
    const float gap = width * 0.005f;
    return make_rect((float)index * cell + gap,
                     0.216f * height,
                     (float)(index + 1u) * cell - gap,
                     0.278f * height);
}

PadRect pad_ui_status_rect(float width, float height) {
    return make_rect(0.012f * width, 0.283f * height, 0.988f * width, 0.326f * height);
}

PadRect pad_ui_key_rect(float width, float height, const PadLayout *layout,
                        size_t row, size_t column) {
    if (layout == NULL || row >= layout->row_count ||
        column >= layout->rows[row].key_count) {
        return make_rect(0.0f, 0.0f, 0.0f, 0.0f);
    }
    const float grid_top = 0.334f * height;
    const float grid_bottom = 0.992f * height;
    const float row_height = (grid_bottom - grid_top) ÷ (float)layout->row_count;
    const float column_width = width ÷ (float)layout->rows[row].key_count;
    const float gap = width * 0.004f;
    return make_rect((float)column * column_width + gap,
                     grid_top + (float)row * row_height + gap,
                     (float)(column + 1u) * column_width - gap,
                     grid_top + (float)(row + 1u) * row_height - gap);
}

PadHit pad_ui_hit_test(float width, float height, const PadLayout *layout,
                       float x, float y) {
    PadHit hit = {PAD_HIT_NONE, NULL};
    if (width <= 0.0f || height <= 0.0f || layout == NULL) {
        return hit;
    }
    if (contains(pad_ui_previous_layout_rect(width, height), x, y)) {
        hit.kind = PAD_HIT_PREVIOUS_LAYOUT;
        return hit;
    }
    if (contains(pad_ui_next_layout_rect(width, height), x, y)) {
        hit.kind = PAD_HIT_NEXT_LAYOUT;
        return hit;
    }
    static const PadHitKind toolbar_hits[TOOLBAR_COUNT] = {
        PAD_HIT_CURSOR_LEFT, PAD_HIT_CURSOR_RIGHT, PAD_HIT_BACKSPACE,
        PAD_HIT_UNDO, PAD_HIT_CLEAR, PAD_HIT_COPY,
    };
    for (size_t index = 0; index < TOOLBAR_COUNT; ++index) {
        if (contains(pad_ui_toolbar_rect(width, height, index), x, y)) {
            hit.kind = toolbar_hits[index];
            return hit;
        }
    }
    for (size_t row = 0; row < layout->row_count; ++row) {
        for (size_t column = 0; column < layout->rows[row].key_count; ++column) {
            if (contains(pad_ui_key_rect(width, height, layout, row, column), x, y)) {
                hit.kind = PAD_HIT_KEY;
                hit.key = &layout->rows[row].keys[column];
                return hit;
            }
        }
    }
    return hit;
}
