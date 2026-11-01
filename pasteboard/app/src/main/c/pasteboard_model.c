#include "pasteboard_model.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static const unsigned char file_magic[8] = {'P', 'B', 'O', 'A', 'R', 'D', '1', '\0'};

static void copy_text(char destination[PASTEBOARD_TEXT_CAPACITY],
                      const char *source, size_t length) {
    memcpy(destination, source, length);
    destination[length] = '\0';
}

static bool valid_board(size_t board_index) {
    return board_index < (size_t)PASTEBOARD_BOARD_COUNT;
}

static void remove_at(PasteboardBoard *board, size_t index) {
    if (index >= board->count) {
        return;
    }
    for (size_t cursor = index; cursor + 1u < board->count; ++cursor) {
        board->entries[cursor] = board->entries[cursor + 1u];
    }
    if (board->count > 0u) {
        --board->count;
        memset(&board->entries[board->count], 0, sizeof(board->entries[board->count]));
    }
}

static void insert_entry(PasteboardBoard *board, const PasteboardEntry *entry) {
    size_t existing = (size_t)PASTEBOARD_SLOT_COUNT;
    for (size_t index = 0u; index < board->count; ++index) {
        if (strcmp(board->entries[index].text, entry->text) == 0) {
            existing = index;
            break;
        }
    }
    if (existing < (size_t)PASTEBOARD_SLOT_COUNT) {
        remove_at(board, existing);
    }

    const size_t retained = board->count < PASTEBOARD_SLOT_COUNT
                                ? (size_t)board->count
                                : (size_t)PASTEBOARD_SLOT_COUNT - 1u;
    for (size_t index = retained; index > 0u; --index) {
        board->entries[index] = board->entries[index - 1u];
    }
    board->entries[0] = *entry;
    if (board->count < PASTEBOARD_SLOT_COUNT) {
        ++board->count;
    }
}

void pasteboard_state_init(PasteboardState *state) {
    memset(state, 0, sizeof(*state));
    state->current_board = PASTEBOARD_STARTING_BOARD;
}

uint64_t pasteboard_hash_text(const char *text) {
    uint64_t hash = UINT64_C(1469598103934665603);
    for (const unsigned char *cursor = (const unsigned char *)text; *cursor != 0u; ++cursor) {
        hash ^= (uint64_t)(*cursor);
        hash *= UINT64_C(1099511628211);
    }
    return hash;
}

bool pasteboard_capture(PasteboardState *state, size_t board_index,
                        const char *text, uint64_t captured_ms) {
    if (!valid_board(board_index) || text == NULL || text[0] == '\0') {
        return false;
    }
    /*
     * The fixed-slot model cannot hold more than 8191 content bytes.
     * Reject at the boundary; never silently shorten a captured clip.
     */
    size_t length = 0u;
    while (length < PASTEBOARD_TEXT_CAPACITY && text[length] != '\0') {
        ++length;
    }
    if (length == PASTEBOARD_TEXT_CAPACITY) {
        return false;
    }
    const uint64_t hash = pasteboard_hash_text(text);
    if (hash == state->last_clipboard_hash) {
        return false;
    }

    PasteboardEntry entry;
    memset(&entry, 0, sizeof(entry));
    copy_text(entry.text, text, length);
    if (entry.text[0] == '\0') {
        return false;
    }
    entry.captured_ms = captured_ms;
    insert_entry(&state->boards[board_index], &entry);
    state->last_clipboard_hash = hash;
    return true;
}

bool pasteboard_move(PasteboardState *state, size_t source_board,
                     size_t source_index, size_t destination_board) {
    if (!valid_board(source_board) || !valid_board(destination_board) ||
        source_board == destination_board) {
        return false;
    }
    PasteboardBoard *source = &state->boards[source_board];
    if (source_index >= source->count) {
        return false;
    }
    const PasteboardEntry moving = source->entries[source_index];
    remove_at(source, source_index);
    insert_entry(&state->boards[destination_board], &moving);
    return true;
}

const PasteboardEntry *pasteboard_entry(const PasteboardState *state,
                                        size_t board_index, size_t entry_index) {
    if (!valid_board(board_index)) {
        return NULL;
    }
    const PasteboardBoard *board = &state->boards[board_index];
    if (entry_index >= board->count) {
        return NULL;
    }
    return &board->entries[entry_index];
}

bool pasteboard_set_current_board(PasteboardState *state, size_t board_index) {
    if (!valid_board(board_index)) {
        return false;
    }
    state->current_board = (uint32_t)board_index;
    return true;
}

void pasteboard_mark_clipboard_seen(PasteboardState *state, const char *text) {
    if (text != NULL) {
        state->last_clipboard_hash = pasteboard_hash_text(text);
    }
}

static bool write_exact(FILE *file, const void *buffer, size_t bytes) {
    return fwrite(buffer, 1u, bytes, file) == bytes;
}

static bool read_exact(FILE *file, void *buffer, size_t bytes) {
    unsigned char *output = buffer;
    for (size_t index = 0u; index < bytes; ++index) {
        const int next = fgetc(file);
        if (next == EOF) return false;
        output[index] = (unsigned char)next;
    }
    return true;
}

bool pasteboard_save(const PasteboardState *state, const char *path) {
    if (state == NULL || path == NULL || path[0] == '\0') {
        return false;
    }
    const size_t path_length = strlen(path);
    static const char suffix[] = ".tmp.XXXXXX";
    if (path_length > SIZE_MAX - sizeof(suffix)) {
        return false;
    }
    char *temporary = malloc(path_length + sizeof(suffix));
    if (temporary == NULL) {
        return false;
    }
    memcpy(temporary, path, path_length);
    memcpy(temporary + path_length, suffix, sizeof(suffix));

    /*
     * Write a distinct private sibling, sync it and rename over the old
     * state only after the whole new image has been written successfully.
     * A crash before rename leaves the last committed state intact.
     */
    const int descriptor = mkstemp(temporary);
    FILE *file = descriptor < 0 ? NULL : fdopen(descriptor, "wb");
    if (file == NULL) {
        if (descriptor >= 0) {
            (void)close(descriptor);
            (void)unlink(temporary);
        }
        free(temporary);
        return false;
    }
    const uint32_t version = PASTEBOARD_FILE_VERSION;
    bool ok = write_exact(file, file_magic, sizeof(file_magic)) &&
              write_exact(file, &version, sizeof(version)) &&
              write_exact(file, &state->current_board, sizeof(state->current_board)) &&
              write_exact(file, &state->last_clipboard_hash, sizeof(state->last_clipboard_hash));

    for (size_t board_index = 0u; ok && board_index < PASTEBOARD_BOARD_COUNT; ++board_index) {
        const PasteboardBoard *board = &state->boards[board_index];
        ok = write_exact(file, &board->count, sizeof(board->count));
        for (size_t index = 0u; ok && index < board->count; ++index) {
            const PasteboardEntry *entry = &board->entries[index];
            const uint32_t length = (uint32_t)strlen(entry->text);
            ok = write_exact(file, &entry->captured_ms, sizeof(entry->captured_ms)) &&
                 write_exact(file, &length, sizeof(length)) &&
                 write_exact(file, entry->text, length);
        }
    }
    if (fflush(file) != 0 || fsync(descriptor) != 0) {
        ok = false;
    }
    if (fclose(file) != 0) {
        ok = false;
    }
    if (ok && rename(temporary, path) != 0) {
        ok = false;
    }
    if (!ok) {
        (void)unlink(temporary);
    }
    free(temporary);
    return ok;
}
bool pasteboard_load(PasteboardState *state, const char *path) {
    FILE *file = fopen(path, "rb");
    if (file == NULL) {
        return false;
    }
    PasteboardState loaded;
    pasteboard_state_init(&loaded);
    unsigned char magic[sizeof(file_magic)];
    uint32_t version = 0u;
    bool ok = read_exact(file, magic, sizeof(magic)) &&
              memcmp(magic, file_magic, sizeof(magic)) == 0 &&
              read_exact(file, &version, sizeof(version)) &&
              version == PASTEBOARD_FILE_VERSION &&
              read_exact(file, &loaded.current_board, sizeof(loaded.current_board)) &&
              loaded.current_board < PASTEBOARD_BOARD_COUNT &&
              read_exact(file, &loaded.last_clipboard_hash, sizeof(loaded.last_clipboard_hash));

    for (size_t board_index = 0u; ok && board_index < PASTEBOARD_BOARD_COUNT; ++board_index) {
        PasteboardBoard *board = &loaded.boards[board_index];
        ok = read_exact(file, &board->count, sizeof(board->count)) &&
             board->count <= PASTEBOARD_SLOT_COUNT;
        for (size_t index = 0u; ok && index < board->count; ++index) {
            PasteboardEntry *entry = &board->entries[index];
            uint32_t length = 0u;
            ok = read_exact(file, &entry->captured_ms, sizeof(entry->captured_ms)) &&
                 read_exact(file, &length, sizeof(length)) &&
                 length < PASTEBOARD_TEXT_CAPACITY;
            if (ok) {
                ok = read_exact(file, entry->text, length);
                entry->text[length] = '\0';
            }
        }
    }
    if (fclose(file) != 0) {
        ok = false;
    }
    if (!ok) {
        return false;
    }
    *state = loaded;
    return true;
}
