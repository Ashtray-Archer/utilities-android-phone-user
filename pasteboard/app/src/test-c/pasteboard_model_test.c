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

static void test_oversize_is_refused_not_truncated(void) {
    PasteboardState state;
    pasteboard_state_init(&state);
    char too_large[PASTEBOARD_TEXT_CAPACITY + 1u];
    memset(too_large, 'a', sizeof(too_large) - 1u);
    too_large[sizeof(too_large) - 1u] = '\0';
    assert(!pasteboard_capture(&state, 1u, too_large, 3u));
    assert(state.boards[1].count == 0u);
    assert(state.last_clipboard_hash == 0u);

    char limit[PASTEBOARD_TEXT_CAPACITY];
    memset(limit, 'b', sizeof(limit) - 1u);
    limit[sizeof(limit) - 1u] = '\0';
    assert(pasteboard_capture(&state, 1u, limit, 4u));
    assert(state.boards[1].count == 1u);
    assert(strlen(state.boards[1].entries[0].text) ==
           PASTEBOARD_TEXT_CAPACITY - 1u);
}

static void test_atomic_replacement_and_stale_temporary(void) {
    PasteboardState state;
    pasteboard_state_init(&state);
    char path[] = "/tmp/pasteboard-replacement-XXXXXX";
    const int descriptor = mkstemp(path);
    assert(descriptor >= 0);
    assert(close(descriptor) == 0);

    assert(pasteboard_capture(&state, 1u, "committed-old", 1u));
    assert(pasteboard_save(&state, path));

    char stale_path[sizeof(path) + 20u];
    const int result = snprintf(stale_path, sizeof(stale_path),
                                "%s.tmp.interrupted", path);
    assert(result > 0 && (size_t)result < sizeof(stale_path));
    FILE *stale = fopen(stale_path, "wb");
    assert(stale != NULL);
    assert(fwrite("partial", 1u, 7u, stale) == 7u);
    assert(fclose(stale) == 0);

    PasteboardState loaded;
    pasteboard_state_init(&loaded);
    assert(pasteboard_load(&loaded, path));
    assert(strcmp(loaded.boards[1].entries[0].text, "committed-old") == 0);

    state.last_clipboard_hash = 0u;
    assert(pasteboard_capture(&state, 1u, "committed-new", 2u));
    assert(pasteboard_save(&state, path));
    pasteboard_state_init(&loaded);
    assert(pasteboard_load(&loaded, path));
    assert(strcmp(loaded.boards[1].entries[0].text, "committed-new") == 0);
    assert(strcmp(loaded.boards[1].entries[1].text, "committed-old") == 0);
    assert(unlink(stale_path) == 0);
    assert(unlink(path) == 0);
}

int main(void) {
    test_trickle();
    test_move_between_boards();
    test_seen_clipboard_does_not_duplicate();
    test_round_trip();
    test_oversize_is_refused_not_truncated();
    test_atomic_replacement_and_stale_temporary();
    puts("pasteboard_model_test: all checks passed");
    return 0;
}
