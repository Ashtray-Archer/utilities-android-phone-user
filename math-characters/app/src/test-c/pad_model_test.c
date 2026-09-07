#include "pad_model.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static const PadLayout *layout_named(const char *name) {
    for (size_t index = 0; index < pad_layout_count(); ++index) {
        const PadLayout *layout = pad_layout_at(index);
        if (strcmp(layout->name, name) == 0) {
            return layout;
        }
    }
    return NULL;
}

static const PadKey *key_labeled(const char *layout_name, const char *label) {
    const PadLayout *layout = layout_named(layout_name);
    assert(layout != NULL);
    for (size_t row = 0; row < layout->row_count; ++row) {
        for (size_t column = 0; column < layout->rows[row].key_count; ++column) {
            const PadKey *key = &layout->rows[row].keys[column];
            if (strcmp(key->label, label) == 0) {
                return key;
            }
        }
    }
    return NULL;
}

static void apply(PadState *state, const char *layout, const char *label) {
    const PadKey *key = key_labeled(layout, label);
    assert(key != NULL);
    (void)pad_apply_key(state, key);
}

static size_t key_text_count(const char *text) {
    size_t count = 0u;
    for (size_t layout_index = 0; layout_index < pad_layout_count(); ++layout_index) {
        const PadLayout *layout = pad_layout_at(layout_index);
        for (size_t row = 0; row < layout->row_count; ++row) {
            for (size_t column = 0; column < layout->rows[row].key_count; ++column) {
                const char *output = layout->rows[row].keys[column].text;
                if (output != NULL && strcmp(output, text) == 0) {
                    ++count;
                }
            }
        }
    }
    return count;
}

static void test_layout_contract(void) {
    static const char *expected_names[] = {
        "Unicode", "Math", "Punctuation", "Programming",
        "Regular Expressions", "Concept Separation",
        "Incantation Assistance", "Several Pastebins",
    };
    assert(pad_layout_count() == sizeof(expected_names) / sizeof(expected_names[0]));
    for (size_t index = 0; index < pad_layout_count(); ++index) {
        const PadLayout *layout = pad_layout_at(index);
        assert(layout != NULL);
        assert(strcmp(layout->name, expected_names[index]) == 0);
        assert(layout->row_count > 0u);
        assert(pad_is_valid_utf8(layout->name));
        for (size_t row = 0; row < layout->row_count; ++row) {
            assert(layout->rows[row].key_count > 0u);
            for (size_t column = 0; column < layout->rows[row].key_count; ++column) {
                const PadKey *key = &layout->rows[row].keys[column];
                assert(key->label != NULL);
                assert(key->label[0] != '\0');
                assert(pad_is_valid_utf8(key->label));
                if (key->text != NULL) {
                    assert(pad_is_valid_utf8(key->text));
                }
            }
        }
    }
    assert(pad_layout_at(pad_layout_count()) == NULL);

    assert(key_labeled("Programming", "⟦ ⟧\nEVALUATE") != NULL);
    assert(key_labeled("Programming", "≝\nDEFINE") != NULL);
    assert(key_labeled("Programming", "⟵\nASSIGN") != NULL);
    assert(key_labeled("Math", "−\nSUBTRACT") != NULL);
    assert(key_labeled("Punctuation", "–\nEN DASH") != NULL);
    assert(key_labeled("Punctuation", "—\nEM DASH") != NULL);
    assert(key_labeled("Punctuation", "\"\nQUOTE") != NULL);
    assert(key_labeled("Concept Separation", "QUAD\nSPACE") != NULL);
    assert(key_labeled("Concept Separation", "THIN\nSPACE") != NULL);
    assert(key_labeled("Regular Expressions", "INCANTATION RUNES\nCONTROL CHARACTERS") != NULL);
    assert(key_labeled("Incantation Assistance", "CHANT\nHISTORY") == NULL);
    assert(layout_named("Movement") == NULL);
    assert(layout_named("Signals") == NULL);
    assert(key_text_count("λ") >= 2u);
    assert(key_text_count("←") >= 2u);
    assert(key_text_count("⟵") >= 2u);
    assert(key_text_count("→") >= 3u);
}

static void test_unicode_editing(void) {
    PadState state;
    pad_state_init(&state);
    apply(&state, "Math", "−\nSUBTRACT");
    apply(&state, "Programming", "λ");
    assert(strcmp(state.text, "−λ") == 0);
    assert(state.cursor == strlen("−λ"));
    assert(pad_is_valid_utf8(state.text));

    const PadKey backspace = {"backspace", PAD_BACKSPACE, NULL, 0u};
    assert(pad_apply_key(&state, &backspace));
    assert(strcmp(state.text, "−") == 0);
    assert(state.cursor == strlen("−"));
    assert(pad_apply_key(&state, &backspace));
    assert(strcmp(state.text, "") == 0);
    assert(state.cursor == 0u);
    assert(!pad_apply_key(&state, &backspace));
}

static void test_paired_insertion(void) {
    PadState state;
    pad_state_init(&state);
    apply(&state, "Programming", "⟦ ⟧\nEVALUATE");
    assert(strcmp(state.text, "⟦⟧") == 0);
    assert(state.cursor == strlen("⟦"));
    apply(&state, "Programming", "λ");
    assert(strcmp(state.text, "⟦λ⟧") == 0);

    PadState regex;
    pad_state_init(&regex);
    apply(&regex, "Regular Expressions", "EXACT ORDER\nGROUP");
    assert(strcmp(regex.text, "()") == 0);
    assert(regex.cursor == 1u);
    apply(&regex, "Regular Expressions", "NUMBER");
    assert(strcmp(regex.text, "(\\d)") == 0);
}

static void test_punctuation(void) {
    PadState state;
    pad_state_init(&state);
    apply(&state, "Punctuation", "−\nMINUS");
    apply(&state, "Punctuation", "–\nEN DASH");
    apply(&state, "Punctuation", "—\nEM DASH");
    apply(&state, "Punctuation", "…\nELLIPSIS");
    assert(strcmp(state.text, "−–—…") == 0);

    const PadLayout *punctuation = layout_named("Punctuation");
    assert(punctuation != NULL);
    for (size_t row = 0; row < punctuation->row_count; ++row) {
        for (size_t column = 0; column < punctuation->rows[row].key_count; ++column) {
            const char *output = punctuation->rows[row].keys[column].text;
            assert(output == NULL || strcmp(output, "-") != 0);
        }
    }

    PadState quotes;
    pad_state_init(&quotes);
    apply(&quotes, "Punctuation", "“ ”\nPAIR");
    assert(strcmp(quotes.text, "“”") == 0);
    assert(quotes.cursor == strlen("“"));
    apply(&quotes, "Math", "λ");
    assert(strcmp(quotes.text, "“λ”") == 0);
}

static void test_exact_spaces(void) {
    PadState state;
    pad_state_init(&state);
    apply(&state, "Concept Separation", "QUAD\nSPACE");
    apply(&state, "Concept Separation", "THIN\nSPACE");
    apply(&state, "Concept Separation", "FOUR\nSPACES");
    apply(&state, "Concept Separation", "DOUBLE\nSPACE");
    apply(&state, "Concept Separation", "SINGLE\nSPACE");
    apply(&state, "Concept Separation", "_\nUNDERSCORE");
    assert(strcmp(state.text, "         _") == 0);
    assert(state.length == strlen("         _"));
    assert(pad_is_valid_utf8(state.text));
}

static void test_editing_actions_and_undo(void) {
    PadState state;
    pad_state_init(&state);
    const PadKey repeat = {"repeat", PAD_SET_REPEAT, NULL, 7u};
    const PadKey add = {"add", PAD_INSERT, "+", 0u};
    const PadKey undo = {"undo", PAD_UNDO, NULL, 0u};
    assert(pad_apply_key(&state, &repeat));
    assert(pad_apply_key(&state, &add));
    assert(strcmp(state.text, "+++++++") == 0);
    assert(state.repeat_count == 1u);
    assert(pad_apply_key(&state, &undo));
    assert(strcmp(state.text, "") == 0);
    assert(pad_apply_key(&state, &undo));
    assert(strcmp(state.text, "+++++++") == 0);

    const PadKey newline_text = {"fixture", PAD_INSERT, "\nabc\ndef", 0u};
    assert(pad_apply_key(&state, &newline_text));
    const PadKey previous_line_key = {"previous line", PAD_PREVIOUS_LINE, NULL, 0u};
    const PadKey next_line_key = {"next line", PAD_NEXT_LINE, NULL, 0u};
    const PadKey line_start_key = {"line start", PAD_LINE_START, NULL, 0u};
    const PadKey line_end_key = {"line end", PAD_LINE_END, NULL, 0u};
    assert(pad_apply_key(&state, &previous_line_key));
    const size_t previous = state.cursor;
    assert(pad_apply_key(&state, &next_line_key));
    assert(state.cursor > previous);
    assert(pad_apply_key(&state, &line_start_key));
    assert(state.cursor == state.length - strlen("def"));
    assert(pad_apply_key(&state, &line_end_key));
    assert(state.cursor == state.length);
}

static void test_pastebins_and_completion(void) {
    PadState state;
    pad_state_init(&state);
    const PadKey alpha = {"alpha", PAD_INSERT, "α", 0u};
    const PadKey clear = {"clear", PAD_CLEAR, NULL, 0u};
    assert(pad_apply_key(&state, &alpha));
    apply(&state, "Several Pastebins", "REMEMBER₁");
    assert(pad_apply_key(&state, &clear));
    apply(&state, "Several Pastebins", "WRITE₁");
    assert(strcmp(state.text, "α") == 0);
    pad_mark_copied(&state);
    assert(strcmp(state.copied_text, "α") == 0);

    PadState completion;
    pad_state_init(&completion);
    apply(&completion, "Incantation Assistance", "TAB /\nCOMPLETE");
    apply(&completion, "Incantation Assistance", "FINISH\nINCANTATION");
    assert(strcmp(completion.text, "\t\n") == 0);
}

static void test_capacity_is_atomic(void) {
    PadState state;
    pad_state_init(&state);
    char almost_full[PAD_BUFFER_CAPACITY - 1u];
    memset(almost_full, 'x', sizeof(almost_full) - 1u);
    almost_full[sizeof(almost_full) - 1u] = '\0';
    const PadKey fill = {"fill", PAD_INSERT, almost_full, 0u};
    assert(pad_apply_key(&state, &fill));
    const size_t before = state.length;
    const PadKey too_much = {"too much", PAD_INSERT, "ab", 0u};
    assert(!pad_apply_key(&state, &too_much));
    assert(state.length == before);
    assert(state.text[state.length] == '\0');
}

static void test_utf8_rejection(void) {
    const char truncated[] = {(char)0xE2, (char)0x82, '\0'};
    const char continuation[] = {(char)0x80, '\0'};
    const char overlong[] = {(char)0xC0, (char)0xAF, '\0'};
    assert(!pad_is_valid_utf8(truncated));
    assert(!pad_is_valid_utf8(continuation));
    assert(!pad_is_valid_utf8(overlong));
    assert(pad_is_valid_utf8("λ≤⟦x⟧"));
}

int main(void) {
    test_layout_contract();
    test_unicode_editing();
    test_paired_insertion();
    test_punctuation();
    test_exact_spaces();
    test_editing_actions_and_undo();
    test_pastebins_and_completion();
    test_capacity_is_atomic();
    test_utf8_rejection();
    puts("pad_model_test: all checks passed");
    return 0;
}
