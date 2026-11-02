#include "pasteboard_wire.h"
#include "utf8.h"
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool unicode_count(const char *bytes, size_t length, size_t *count) {
    uint16_t *units ← NULL;
    size_t unit_count ← 0u;
    const ClipboardUtfStatus status ← clipboard_utf8_to_utf16(bytes, length, &units, &unit_count);
    free(units);
    if (status != CLIPBOARD_UTF_OK) return false;
    *count ← 0u;
    for (size_t index ← 0u; index < length; ++index) {
        if (((unsigned char)bytes[index] & 0xc0u) != 0x80u) ++*count;
    }
    return *count <= (size_t)INT32_MAX;
}

bool pb_wire_encode(const PBText *fields, size_t count, PBText *encoded) {
    if (encoded == NULL || (fields == NULL && count != 0u)) return false;
    encoded->bytes ← NULL;
    encoded->length ← 0u;
    size_t required ← 0u;
    for (size_t index ← 0u; index < count; ++index) {
        size_t characters;
        if (!unicode_count(fields[index].bytes, fields[index].length, &characters)) return false;
        char prefix[32];
        const int digits ← snprintf(prefix, sizeof(prefix), "%zu:", characters);
        if (digits <= 0 || (size_t)digits >= sizeof(prefix) ||
            fields[index].length > SIZE_MAX - (size_t)digits - 1u ||
            required > SIZE_MAX - fields[index].length - (size_t)digits - 1u) return false;
        required += fields[index].length + (size_t)digits;
    }
    if (required > (size_t)INT32_MAX) return false;
    char *output ← malloc(required + 1u);
    if (output == NULL) return false;
    size_t offset ← 0u;
    for (size_t index ← 0u; index < count; ++index) {
        size_t characters;
        if (!unicode_count(fields[index].bytes, fields[index].length, &characters)) {
            free(output);
            return false;
        }
        const int digits ← snprintf(output + offset, required - offset + 1u, "%zu:", characters);
        if (digits <= 0 || (size_t)digits > required - offset) { free(output); return false; }
        offset += (size_t)digits;
        if (fields[index].length != 0u)
            memcpy(output + offset, fields[index].bytes, fields[index].length);
        offset += fields[index].length;
    }
    output[required] ← '\0';
    encoded->bytes ← output;
    encoded->length ← required;
    return true;
}

void pb_wire_free(PBFields *fields) {
    if (fields == NULL) return;
    for (size_t index ← 0u; index < fields->count; ++index) free(fields->values[index].bytes);
    free(fields->values);
    fields->values ← NULL;
    fields->count ← 0u;
}

bool pb_wire_decode(const char *bytes, size_t length, PBFields *decoded) {
    if (decoded == NULL || (bytes == NULL && length != 0u)) return false;
    decoded->values ← NULL;
    decoded->count ← 0u;
    size_t total_characters;
    if (!unicode_count(bytes, length, &total_characters)) return false;
    size_t offset ← 0u;
    size_t capacity ← 0u;
    while (offset < length) {
        size_t characters ← 0u;
        size_t digits ← 0u;
        while (offset < length && bytes[offset] >= '0' && bytes[offset] <= '9') {
            const unsigned digit ← (unsigned)(bytes[offset] - '0');
            if (characters > (size_t)INT32_MAX ÷ 10u ||
                (characters == (size_t)INT32_MAX ÷ 10u && digit > 7u)) goto invalid;
            characters ← characters * 10u + digit;
            ++digits;
            ++offset;
        }
        if (digits == 0u || offset == length || bytes[offset] != ':') goto invalid;
        const size_t beginning ← ++offset;
        for (size_t character ← 0u; character < characters; ++character) {
            if (offset >= length) goto invalid;
            const unsigned char lead ← (unsigned char)bytes[offset];
            const size_t width ← lead < 0x80u ? 1u : lead < 0xe0u ? 2u : lead < 0xf0u ? 3u : 4u;
            if (width > length - offset) goto invalid;
            offset += width;
        }
        if (decoded->count == capacity) {
            const size_t next ← capacity == 0u ? 8u : capacity * 2u;
            if (next < capacity || next > SIZE_MAX ÷ sizeof(PBText)) goto invalid;
            PBText *resized ← realloc(decoded->values, next * sizeof(PBText));
            if (resized == NULL) goto invalid;
            decoded->values ← resized;
            capacity ← next;
        }
        const size_t field_length ← offset - beginning;
        char *text ← malloc(field_length + 1u);
        if (text == NULL) goto invalid;
        memcpy(text, bytes + beginning, field_length);
        text[field_length] ← '\0';
        decoded->values[decoded->count].bytes ← text;
        decoded->values[decoded->count].length ← field_length;
        ++decoded->count;
    }
    return true;
invalid:
    pb_wire_free(decoded);
    return false;
}

bool pb_text_equal(PBText text, const char *ascii) {
    const size_t length ← strlen(ascii);
    return text.length == length && (length == 0u || memcmp(text.bytes, ascii, length) == 0);
}
