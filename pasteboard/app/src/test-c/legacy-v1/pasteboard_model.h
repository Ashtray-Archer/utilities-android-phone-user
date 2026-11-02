#ifndef PASTEBOARD_MODEL_H
#define PASTEBOARD_MODEL_H

#include "pasteboard_config.generated.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define PASTEBOARD_TEXT_CAPACITY 8192u
#define PASTEBOARD_FILE_VERSION 1u

typedef struct {
    char text[PASTEBOARD_TEXT_CAPACITY];
    uint64_t captured_ms;
} PasteboardEntry;

typedef struct {
    PasteboardEntry entries[PASTEBOARD_SLOT_COUNT];
    uint32_t count;
} PasteboardBoard;

typedef struct {
    PasteboardBoard boards[PASTEBOARD_BOARD_COUNT];
    uint32_t current_board;
    uint64_t last_clipboard_hash;
} PasteboardState;

void pasteboard_state_init(PasteboardState *state);
uint64_t pasteboard_hash_text(const char *text);
bool pasteboard_capture(PasteboardState *state, size_t board_index,
                        const char *text, uint64_t captured_ms);
bool pasteboard_move(PasteboardState *state, size_t source_board,
                     size_t source_index, size_t destination_board);
const PasteboardEntry *pasteboard_entry(const PasteboardState *state,
                                        size_t board_index, size_t entry_index);
bool pasteboard_set_current_board(PasteboardState *state, size_t board_index);
void pasteboard_mark_clipboard_seen(PasteboardState *state, const char *text);
bool pasteboard_save(const PasteboardState *state, const char *path);
bool pasteboard_load(PasteboardState *state, const char *path);

#endif
