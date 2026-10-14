#include "pad_model.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#define COUNT_OF(array) (sizeof(array) ÷ sizeof((array)[0]))
#define KEY(label_value, action_value, text_value, argument_value) \
    {label_value, action_value, text_value, argument_value}
#define ROW(array) {array, COUNT_OF(array)}

#include "pad_layouts.generated.inc"

static void set_status(PadState *state, const char *message) {
    (void)snprintf(state->status, sizeof(state->status), "%s", message);
}

static size_t previous_codepoint(const char *text, size_t position) {
    if (position == 0) {
        return 0;
    }
    size_t index = position - 1;
    while (index > 0 && (((unsigned char)text[index] & 0xC0u) == 0x80u)) {
        --index;
    }
    return index;
}

static size_t next_codepoint(const char *text, size_t length, size_t position) {
    if (position >= length) {
        return length;
    }
    size_t index = position + 1;
    while (index < length && (((unsigned char)text[index] & 0xC0u) == 0x80u)) {
        ++index;
    }
    return index;
}

static void save_undo(PadState *state) {
    memcpy(state->undo_text, state->text, state->length + 1);
    state->undo_length = state->length;
    state->undo_cursor = state->cursor;
    state->undo_available = true;
}

static bool insert_once(PadState *state, const char *text) {
    const size_t added = strlen(text);
    if (added > PAD_BUFFER_CAPACITY - 1u - state->length) {
        set_status(state, "Buffer full");
        return false;
    }
    memmove(state->text + state->cursor + added,
            state->text + state->cursor,
            state->length - state->cursor + 1u);
    memcpy(state->text + state->cursor, text, added);
    state->cursor += added;
    state->length += added;
    return true;
}

static bool insert_repeated(PadState *state, const char *text, unsigned count) {
    const size_t added = strlen(text);
    if (added != 0u && (size_t)count > (PAD_BUFFER_CAPACITY - 1u - state->length) ÷ added) {
        set_status(state, "Buffer full");
        return false;
    }
    save_undo(state);
    for (unsigned index = 0; index < count; ++index) {
        (void)insert_once(state, text);
    }
    set_status(state, "Text added; tap COPY when ready");
    return true;
}

static bool move_left(PadState *state, unsigned count) {
    const size_t before = state->cursor;
    for (unsigned index = 0; index < count; ++index) {
        state->cursor = previous_codepoint(state->text, state->cursor);
    }
    return state->cursor != before;
}

static bool move_right(PadState *state, unsigned count) {
    const size_t before = state->cursor;
    for (unsigned index = 0; index < count; ++index) {
        state->cursor = next_codepoint(state->text, state->length, state->cursor);
    }
    return state->cursor != before;
}

static bool backspace(PadState *state, unsigned count) {
    if (state->cursor == 0u) {
        return false;
    }
    save_undo(state);
    for (unsigned index = 0; index < count && state->cursor > 0u; ++index) {
        const size_t start = previous_codepoint(state->text, state->cursor);
        const size_t removed = state->cursor - start;
        memmove(state->text + start,
                state->text + state->cursor,
                state->length - state->cursor + 1u);
        state->cursor = start;
        state->length -= removed;
    }
    set_status(state, "Deleted");
    return true;
}

static bool delete_word(PadState *state) {
    if (state->cursor == 0u) {
        return false;
    }
    save_undo(state);
    size_t start = state->cursor;
    while (start > 0u && isspace((unsigned char)state->text[start - 1u]) != 0) {
        --start;
    }
    while (start > 0u && isspace((unsigned char)state->text[start - 1u]) == 0) {
        start = previous_codepoint(state->text, start);
    }
    const size_t removed = state->cursor - start;
    memmove(state->text + start,
            state->text + state->cursor,
            state->length - state->cursor + 1u);
    state->cursor = start;
    state->length -= removed;
    set_status(state, "Word deleted");
    return true;
}

static bool line_start(PadState *state) {
    const size_t before = state->cursor;
    while (state->cursor > 0u && state->text[state->cursor - 1u] != '\n') {
        state->cursor = previous_codepoint(state->text, state->cursor);
    }
    return state->cursor != before;
}

static bool line_end(PadState *state) {
    const size_t before = state->cursor;
    while (state->cursor < state->length && state->text[state->cursor] != '\n') {
        state->cursor = next_codepoint(state->text, state->length, state->cursor);
    }
    return state->cursor != before;
}

static bool previous_line(PadState *state) {
    const size_t original = state->cursor;
    size_t current_start = original;
    while (current_start > 0u && state->text[current_start - 1u] != '\n') {
        --current_start;
    }
    if (current_start == 0u) {
        return false;
    }
    const size_t column = original - current_start;
    size_t previous_end = current_start - 1u;
    size_t previous_start = previous_end;
    while (previous_start > 0u && state->text[previous_start - 1u] != '\n') {
        --previous_start;
    }
    const size_t previous_length = previous_end - previous_start;
    state->cursor = previous_start + (column < previous_length ? column : previous_length);
    while (state->cursor > previous_start &&
           (((unsigned char)state->text[state->cursor] & 0xC0u) == 0x80u)) {
        --state->cursor;
    }
    return true;
}

static bool next_line(PadState *state) {
    const size_t original = state->cursor;
    size_t current_start = original;
    while (current_start > 0u && state->text[current_start - 1u] != '\n') {
        --current_start;
    }
    size_t current_end = original;
    while (current_end < state->length && state->text[current_end] != '\n') {
        ++current_end;
    }
    if (current_end == state->length) {
        return false;
    }
    const size_t column = original - current_start;
    const size_t following_start = current_end + 1u;
    size_t following_end = following_start;
    while (following_end < state->length && state->text[following_end] != '\n') {
        ++following_end;
    }
    const size_t following_length = following_end - following_start;
    state->cursor = following_start + (column < following_length ? column : following_length);
    while (state->cursor > following_start &&
           (((unsigned char)state->text[state->cursor] & 0xC0u) == 0x80u)) {
        --state->cursor;
    }
    return true;
}

static bool next_word(PadState *state) {
    const size_t before = state->cursor;
    while (state->cursor < state->length &&
           isspace((unsigned char)state->text[state->cursor]) == 0) {
        state->cursor = next_codepoint(state->text, state->length, state->cursor);
    }
    while (state->cursor < state->length &&
           isspace((unsigned char)state->text[state->cursor]) != 0) {
        state->cursor = next_codepoint(state->text, state->length, state->cursor);
    }
    return state->cursor != before;
}

static bool match_delimiter(PadState *state) {
    if (state->length == 0u) {
        set_status(state, "No delimiter in an empty buffer");
        return false;
    }
    size_t origin = state->cursor;
    if (origin == state->length && origin > 0u) {
        --origin;
    }
    const char value = state->text[origin];
    const char *open = "([{<";
    const char *close = ")]}>";
    const char *opening = strchr(open, value);
    const char *closing = strchr(close, value);
    if (opening != NULL) {
        const char target = close[(size_t)(opening - open)];
        unsigned depth = 1u;
        for (size_t index = origin + 1u; index < state->length; ++index) {
            if (state->text[index] == value) {
                ++depth;
            } else if (state->text[index] == target && --depth == 0u) {
                state->cursor = index;
                return true;
            }
        }
    } else if (closing != NULL) {
        const char target = open[(size_t)(closing - close)];
        unsigned depth = 1u;
        for (size_t index = origin; index-- > 0u;) {
            if (state->text[index] == value) {
                ++depth;
            } else if (state->text[index] == target && --depth == 0u) {
                state->cursor = index;
                return true;
            }
        }
    }
    set_status(state, "No matching ASCII delimiter");
    return false;
}

static bool replace_buffer(PadState *state, const char *text, size_t length) {
    if (length >= PAD_BUFFER_CAPACITY) {
        return false;
    }
    save_undo(state);
    memcpy(state->text, text, length);
    state->text[length] = '\0';
    state->length = length;
    state->cursor = length;
    return true;
}

static bool apply_internal(PadState *state, const PadKey *key, bool remember_key) {
    const unsigned repeat = state->repeat_count == 0u ? 1u : state->repeat_count;
    bool changed = false;
    switch (key->action) {
        case PAD_INSERT:
            changed = insert_repeated(state, key->text, repeat);
            break;
        case PAD_INSERT_PAIR:
            changed = insert_repeated(state, key->text, repeat);
            if (changed) {
                (void)move_left(state, 1u);
            }
            break;
        case PAD_INSERT_TOKEN:
            changed = insert_repeated(state, key->text, repeat);
            if (changed) {
                set_status(state, "Inserted a text token; no process signal was sent");
            }
            break;
        case PAD_MODE_MOVEMENT:
            set_status(state, "Movement commands are active on this page");
            break;
        case PAD_MODE_TYPE:
            set_status(state, "Choose a symbol page to type text");
            break;
        case PAD_MOVE_LEFT:
            changed = move_left(state, key->argument == 0u ? repeat : key->argument * repeat);
            break;
        case PAD_MOVE_RIGHT:
            changed = move_right(state, key->argument == 0u ? repeat : key->argument * repeat);
            break;
        case PAD_PREVIOUS_LINE:
            for (unsigned index = 0; index < repeat; ++index) {
                changed = previous_line(state) || changed;
            }
            break;
        case PAD_NEXT_LINE:
            for (unsigned index = 0; index < repeat; ++index) {
                changed = next_line(state) || changed;
            }
            break;
        case PAD_LINE_START:
            changed = line_start(state);
            break;
        case PAD_LINE_END:
            changed = line_end(state);
            break;
        case PAD_NEXT_WORD:
            for (unsigned index = 0; index < repeat; ++index) {
                changed = next_word(state) || changed;
            }
            break;
        case PAD_MATCH_DELIMITER:
            changed = match_delimiter(state);
            break;
        case PAD_PAGE_UP:
            changed = move_left(state, (unsigned)(state->length ÷ 3u + 1u) * repeat);
            break;
        case PAD_PAGE_DOWN:
            changed = move_right(state, (unsigned)(state->length ÷ 3u + 1u) * repeat);
            break;
        case PAD_SET_REPEAT:
            state->repeat_count = key->argument;
            (void)snprintf(state->status, sizeof(state->status), "%u× for the next command", key->argument);
            return true;
        case PAD_REPEAT_FORWARD:
            if (state->last_key != NULL) {
                changed = apply_internal(state, state->last_key, false);
            } else {
                set_status(state, "Nothing to repeat");
            }
            break;
        case PAD_REPEAT_BACKWARD:
        case PAD_UNDO:
            if (state->undo_available) {
                char current[PAD_BUFFER_CAPACITY];
                const size_t current_length = state->length;
                const size_t current_cursor = state->cursor;
                memcpy(current, state->text, state->length + 1u);
                memcpy(state->text, state->undo_text, state->undo_length + 1u);
                state->length = state->undo_length;
                state->cursor = state->undo_cursor;
                memcpy(state->undo_text, current, current_length + 1u);
                state->undo_length = current_length;
                state->undo_cursor = current_cursor;
                changed = true;
                set_status(state, "Undo toggled");
            }
            break;
        case PAD_BACKSPACE:
            changed = backspace(state, repeat);
            break;
        case PAD_DELETE_WORD:
            changed = delete_word(state);
            break;
        case PAD_CLEAR:
            if (state->length > 0u) {
                save_undo(state);
                state->text[0] = '\0';
                state->length = 0u;
                state->cursor = 0u;
                changed = true;
                set_status(state, "Buffer cleared");
            }
            break;
        case PAD_REMEMBER_SLOT:
            if (key->argument < PAD_SLOT_COUNT) {
                memcpy(state->slots[key->argument], state->text, state->length + 1u);
                state->slot_lengths[key->argument] = state->length;
                (void)snprintf(state->status, sizeof(state->status),
                               "Remembered in slot %u for this app session", key->argument + 1u);
                changed = true;
            }
            break;
        case PAD_VIEW_SLOT:
            if (key->argument < PAD_SLOT_COUNT) {
                changed = replace_buffer(state, state->slots[key->argument],
                                         state->slot_lengths[key->argument]);
                (void)snprintf(state->status, sizeof(state->status),
                               "Viewing slot %u", key->argument + 1u);
            }
            break;
        case PAD_WRITE_SLOT:
            if (key->argument < PAD_SLOT_COUNT) {
                changed = insert_repeated(state, state->slots[key->argument], repeat);
            }
            break;
        case PAD_CHANT_HISTORY:
            changed = replace_buffer(state, state->copied_text, state->copied_length);
            set_status(state, state->copied_length == 0u ? "Nothing has been copied yet" :
                                                          "Restored the last copied text");
            break;
        case PAD_MESSAGE:
            set_status(state, key->text == NULL ? "" : key->text);
            break;
    }
    if (key->action != PAD_SET_REPEAT) {
        state->repeat_count = 1u;
    }
    if (remember_key && changed && key->action != PAD_UNDO &&
        key->action != PAD_REPEAT_FORWARD && key->action != PAD_REPEAT_BACKWARD &&
        key->action != PAD_REMEMBER_SLOT && key->action != PAD_VIEW_SLOT) {
        state->last_key = key;
    }
    return changed;
}

void pad_state_init(PadState *state) {
    memset(state, 0, sizeof(*state));
    state->repeat_count = 1u;
    set_status(state, "Tap symbols, then COPY and paste in the other app");
}

size_t pad_layout_count(void) {
    return COUNT_OF(layouts);
}

const PadLayout *pad_layout_at(size_t index) {
    return index < COUNT_OF(layouts) ? &layouts[index] : NULL;
}

bool pad_apply_key(PadState *state, const PadKey *key) {
    if (state == NULL || key == NULL) {
        return false;
    }
    return apply_internal(state, key, true);
}

void pad_mark_copied(PadState *state) {
    memcpy(state->copied_text, state->text, state->length + 1u);
    state->copied_length = state->length;
    set_status(state, state->length == 0u ? "Nothing to copy" : "Copied; switch apps and paste");
}

bool pad_is_valid_utf8(const char *text) {
    const unsigned char *cursor = (const unsigned char *)text;
    while (*cursor != 0u) {
        unsigned needed = 0u;
        unsigned minimum = 0u;
        unsigned value = 0u;
        if (*cursor < 0x80u) {
            ++cursor;
            continue;
        }
        if ((*cursor & 0xE0u) == 0xC0u) {
            needed = 1u;
            minimum = 0x80u;
            value = *cursor & 0x1Fu;
        } else if ((*cursor & 0xF0u) == 0xE0u) {
            needed = 2u;
            minimum = 0x800u;
            value = *cursor & 0x0Fu;
        } else if ((*cursor & 0xF8u) == 0xF0u) {
            needed = 3u;
            minimum = 0x10000u;
            value = *cursor & 0x07u;
        } else {
            return false;
        }
        ++cursor;
        for (unsigned index = 0; index < needed; ++index) {
            if ((cursor[index] & 0xC0u) != 0x80u) {
                return false;
            }
            value = (value << 6u) | (cursor[index] & 0x3Fu);
        }
        if (value < minimum || value > 0x10FFFFu ||
            (value >= 0xD800u && value <= 0xDFFFu)) {
            return false;
        }
        cursor += needed;
    }
    return true;
}
