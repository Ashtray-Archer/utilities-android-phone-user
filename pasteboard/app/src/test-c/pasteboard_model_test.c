#include "pasteboard_model.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void test_trickle(void) {
    PasteboardState state;
    pasteboard_state_init(&state);
    for (size_t index = 0u; index < 18u; ++index) {
        char text[32];
        (void)snprintf(text, sizeof(text), "item-%zu", index);
        state.last_clipboard_hash = 0u;
        assert(pasteboard_capture(&state, 1u, text, 1000u + index));
    }
    assert(state.boards[1].count == 16u);
    assert(strcmp(state.boards[1].entries[0].text, "item-17") == 0);
    assert(strcmp(state.boards[1].entries[15].text, "item-2") == 0);
}

static void test_move_between_boards(void) {
    PasteboardState state;
    pasteboard_state_init(&state);
    assert(pasteboard_capture(&state, 1u, "school signature", 42u));
    state.last_clipboard_hash = 0u;
    assert(pasteboard_capture(&state, 1u, "math characters", 43u));
    assert(pasteboard_move(&state, 1u, 1u, 0u));
    assert(state.boards[1].count == 1u);
    assert(strcmp(state.boards[1].entries[0].text, "math characters") == 0);
    assert(state.boards[0].count == 1u);
    assert(strcmp(state.boards[0].entries[0].text, "school signature") == 0);
}

static void test_seen_clipboard_does_not_duplicate(void) {
    PasteboardState state;
    pasteboard_state_init(&state);
    assert(pasteboard_capture(&state, 1u, "same", 1u));
    assert(!pasteboard_capture(&state, 1u, "same", 2u));
    assert(state.boards[1].count == 1u);
}

static void test_round_trip(void) {
    PasteboardState state;
    pasteboard_state_init(&state);
    assert(pasteboard_capture(&state, 1u, "alpha", 1u));
    state.last_clipboard_hash = 0u;
    assert(pasteboard_capture(&state, 2u, "βeta", 2u));
    assert(pasteboard_set_current_board(&state, 2u));

    char path[] = "/tmp/pasteboard-model-test-XXXXXX";
    const int descriptor = mkstemp(path);
    assert(descriptor >= 0);
    assert(close(descriptor) == 0);
    assert(pasteboard_save(&state, path));

    PasteboardState loaded;
    pasteboard_state_init(&loaded);
    assert(pasteboard_load(&loaded, path));
    assert(loaded.current_board == 2u);
    assert(strcmp(loaded.boards[1].entries[0].text, "alpha") == 0);
    assert(strcmp(loaded.boards[2].entries[0].text, "βeta") == 0);
    assert(unlink(path) == 0);
}

int main(void) {
    test_trickle();
    test_move_between_boards();
    test_seen_clipboard_does_not_duplicate();
    test_round_trip();
    puts("pasteboard_model_test: all checks passed");
    return 0;
}
