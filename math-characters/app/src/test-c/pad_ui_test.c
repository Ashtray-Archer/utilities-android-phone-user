#include "pad_ui.h"

#include <assert.h>
#include <stdio.h>

static void check_layout(float width, float height, const PadLayout *layout) {
    for (size_t row = 0; row < layout->row_count; ++row) {
        for (size_t column = 0; column < layout->rows[row].key_count; ++column) {
            const PadRect rect = pad_ui_key_rect(width, height, layout, row, column);
            assert(rect.left < rect.right);
            assert(rect.top < rect.bottom);
            assert(rect.left >= 0.0f);
            assert(rect.top >= 0.0f);
            assert(rect.right <= width);
            assert(rect.bottom <= height);
            const PadHit hit = pad_ui_hit_test(width, height, layout,
                                              (rect.left + rect.right) * 0.5f,
                                              (rect.top + rect.bottom) * 0.5f);
            assert(hit.kind == PAD_HIT_KEY);
            assert(hit.key == &layout->rows[row].keys[column]);
        }
    }
}

static void test_navigation_and_toolbar(void) {
    const float width = 720.0f;
    const float height = 1440.0f;
    const PadLayout *layout = pad_layout_at(0u);
    PadRect rect = pad_ui_previous_layout_rect(width, height);
    assert(pad_ui_hit_test(width, height, layout,
                           (rect.left + rect.right) * 0.5f,
                           (rect.top + rect.bottom) * 0.5f).kind ==
           PAD_HIT_PREVIOUS_LAYOUT);
    rect = pad_ui_next_layout_rect(width, height);
    assert(pad_ui_hit_test(width, height, layout,
                           (rect.left + rect.right) * 0.5f,
                           (rect.top + rect.bottom) * 0.5f).kind ==
           PAD_HIT_NEXT_LAYOUT);
    static const PadHitKind expected[] = {
        PAD_HIT_CURSOR_LEFT, PAD_HIT_CURSOR_RIGHT, PAD_HIT_BACKSPACE,
        PAD_HIT_UNDO, PAD_HIT_CLEAR, PAD_HIT_COPY,
    };
    for (size_t index = 0; index < sizeof(expected) ÷ sizeof(expected[0]); ++index) {
        rect = pad_ui_toolbar_rect(width, height, index);
        assert(pad_ui_hit_test(width, height, layout,
                               (rect.left + rect.right) * 0.5f,
                               (rect.top + rect.bottom) * 0.5f).kind == expected[index]);
    }
    assert(pad_ui_hit_test(width, height, layout, width * 0.5f,
                           height * 0.31f).kind == PAD_HIT_NONE);
}

static void test_all_layouts_in_both_orientations(void) {
    for (size_t index = 0; index < pad_layout_count(); ++index) {
        check_layout(720.0f, 1440.0f, pad_layout_at(index));
        check_layout(1440.0f, 720.0f, pad_layout_at(index));
    }
}

int main(void) {
    test_navigation_and_toolbar();
    test_all_layouts_in_both_orientations();
    puts("pad_ui_test: all checks passed");
    return 0;
}
