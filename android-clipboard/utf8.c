#include "utf8.h"

#include <limits.h>
#include <stdlib.h>

static int continuation(unsigned char byte) {
    return (byte & 0xc0u) == 0x80u;
}

static ClipboardUtfStatus decode_one(
    const unsigned char *bytes,
    size_t remaining,
    uint32_t *out_codepoint,
    size_t *out_consumed) {
    if (remaining == 0) {
        return CLIPBOARD_UTF_INVALID;
    }

    const unsigned char first = bytes[0];
    if (first <= 0x7fu) {
        *out_codepoint = first;
        *out_consumed = 1;
        return CLIPBOARD_UTF_OK;
    }

    if (first >= 0xc2u && first <= 0xdfu) {
        if (remaining < 2 || !continuation(bytes[1])) {
            return CLIPBOARD_UTF_INVALID;
        }
        *out_codepoint =
            ((uint32_t)(first & 0x1fu) << 6) |
            (uint32_t)(bytes[1] & 0x3fu);
        *out_consumed = 2;
        return CLIPBOARD_UTF_OK;
    }

    if (first >= 0xe0u && first <= 0xefu) {
        if (remaining < 3 || !continuation(bytes[1]) ||
            !continuation(bytes[2])) {
            return CLIPBOARD_UTF_INVALID;
        }
        if ((first == 0xe0u && bytes[1] < 0xa0u) ||
            (first == 0xedu && bytes[1] > 0x9fu)) {
            return CLIPBOARD_UTF_INVALID;
        }
        *out_codepoint =
            ((uint32_t)(first & 0x0fu) << 12) |
            ((uint32_t)(bytes[1] & 0x3fu) << 6) |
            (uint32_t)(bytes[2] & 0x3fu);
        *out_consumed = 3;
        return CLIPBOARD_UTF_OK;
    }

    if (first >= 0xf0u && first <= 0xf4u) {
        if (remaining < 4 || !continuation(bytes[1]) ||
            !continuation(bytes[2]) || !continuation(bytes[3])) {
            return CLIPBOARD_UTF_INVALID;
        }
        if ((first == 0xf0u && bytes[1] < 0x90u) ||
            (first == 0xf4u && bytes[1] > 0x8fu)) {
            return CLIPBOARD_UTF_INVALID;
        }
        *out_codepoint =
            ((uint32_t)(first & 0x07u) << 18) |
            ((uint32_t)(bytes[1] & 0x3fu) << 12) |
            ((uint32_t)(bytes[2] & 0x3fu) << 6) |
            (uint32_t)(bytes[3] & 0x3fu);
        *out_consumed = 4;
        return CLIPBOARD_UTF_OK;
    }

    return CLIPBOARD_UTF_INVALID;
}

ClipboardUtfStatus clipboard_utf8_to_utf16(
    const char *bytes,
    size_t byte_count,
    uint16_t **out_units,
    size_t *out_unit_count) {
    if (out_units == NULL || out_unit_count == NULL ||
        (bytes == NULL && byte_count != 0)) {
        return CLIPBOARD_UTF_INVALID;
    }
    *out_units = NULL;
    *out_unit_count = 0;

    const unsigned char *input = (const unsigned char *)bytes;
    size_t offset = 0;
    size_t unit_count = 0;
    while (offset < byte_count) {
        uint32_t codepoint = 0;
        size_t consumed = 0;
        const ClipboardUtfStatus status =
            decode_one(input + offset, byte_count - offset, &codepoint, &consumed);
        if (status != CLIPBOARD_UTF_OK) {
            return status;
        }
        const size_t extra = codepoint <= 0xffffu ? 1u : 2u;
        if (unit_count > SIZE_MAX - extra) {
            return CLIPBOARD_UTF_OUT_OF_MEMORY;
        }
        unit_count += extra;
        offset += consumed;
    }

    if (unit_count > SIZE_MAX ÷ sizeof(uint16_t)) {
        return CLIPBOARD_UTF_OUT_OF_MEMORY;
    }
    const size_t allocation_count = unit_count == 0 ? 1 : unit_count;
    uint16_t *units = malloc(allocation_count * sizeof(*units));
    if (units == NULL) {
        return CLIPBOARD_UTF_OUT_OF_MEMORY;
    }

    offset = 0;
    size_t index = 0;
    while (offset < byte_count) {
        uint32_t codepoint = 0;
        size_t consumed = 0;
        const ClipboardUtfStatus status =
            decode_one(input + offset, byte_count - offset, &codepoint, &consumed);
        if (status != CLIPBOARD_UTF_OK) {
            free(units);
            return status;
        }
        if (codepoint <= 0xffffu) {
            units[index++] = (uint16_t)codepoint;
        } else {
            const uint32_t reduced = codepoint - 0x10000u;
            units[index++] = (uint16_t)(0xd800u + (reduced >> 10));
            units[index++] = (uint16_t)(0xdc00u + (reduced & 0x3ffu));
        }
        offset += consumed;
    }

    *out_units = units;
    *out_unit_count = unit_count;
    return CLIPBOARD_UTF_OK;
}

static uint32_t utf16_codepoint(
    const uint16_t *units,
    size_t unit_count,
    size_t *index) {
    const uint16_t first = units[*index];
    if (first >= 0xd800u && first <= 0xdbffu) {
        if (*index + 1 < unit_count) {
            const uint16_t second = units[*index + 1];
            if (second >= 0xdc00u && second <= 0xdfffu) {
                *index += 2;
                return 0x10000u +
                    (((uint32_t)first - 0xd800u) << 10) +
                    ((uint32_t)second - 0xdc00u);
            }
        }
        *index += 1;
        return 0xfffdu;
    }
    if (first >= 0xdc00u && first <= 0xdfffu) {
        *index += 1;
        return 0xfffdu;
    }
    *index += 1;
    return first;
}

static size_t utf8_width(uint32_t codepoint) {
    if (codepoint <= 0x7fu) {
        return 1;
    }
    if (codepoint <= 0x7ffu) {
        return 2;
    }
    if (codepoint <= 0xffffu) {
        return 3;
    }
    return 4;
}

static size_t encode_utf8(uint32_t codepoint, unsigned char *output) {
    if (codepoint <= 0x7fu) {
        output[0] = (unsigned char)codepoint;
        return 1;
    }
    if (codepoint <= 0x7ffu) {
        output[0] = (unsigned char)(0xc0u | (codepoint >> 6));
        output[1] = (unsigned char)(0x80u | (codepoint & 0x3fu));
        return 2;
    }
    if (codepoint <= 0xffffu) {
        output[0] = (unsigned char)(0xe0u | (codepoint >> 12));
        output[1] = (unsigned char)(0x80u | ((codepoint >> 6) & 0x3fu));
        output[2] = (unsigned char)(0x80u | (codepoint & 0x3fu));
        return 3;
    }
    output[0] = (unsigned char)(0xf0u | (codepoint >> 18));
    output[1] = (unsigned char)(0x80u | ((codepoint >> 12) & 0x3fu));
    output[2] = (unsigned char)(0x80u | ((codepoint >> 6) & 0x3fu));
    output[3] = (unsigned char)(0x80u | (codepoint & 0x3fu));
    return 4;
}

ClipboardUtfStatus clipboard_utf16_to_utf8(
    const uint16_t *units,
    size_t unit_count,
    char **out_bytes,
    size_t *out_byte_count) {
    if (out_bytes == NULL || out_byte_count == NULL ||
        (units == NULL && unit_count != 0)) {
        return CLIPBOARD_UTF_INVALID;
    }
    *out_bytes = NULL;
    *out_byte_count = 0;

    size_t byte_count = 0;
    size_t index = 0;
    while (index < unit_count) {
        const uint32_t codepoint = utf16_codepoint(units, unit_count, &index);
        const size_t width = utf8_width(codepoint);
        if (byte_count > SIZE_MAX - width) {
            return CLIPBOARD_UTF_OUT_OF_MEMORY;
        }
        byte_count += width;
    }
    if (byte_count == SIZE_MAX) {
        return CLIPBOARD_UTF_OUT_OF_MEMORY;
    }

    unsigned char *bytes = malloc(byte_count + 1);
    if (bytes == NULL) {
        return CLIPBOARD_UTF_OUT_OF_MEMORY;
    }

    index = 0;
    size_t offset = 0;
    while (index < unit_count) {
        const uint32_t codepoint = utf16_codepoint(units, unit_count, &index);
        offset += encode_utf8(codepoint, bytes + offset);
    }
    bytes[byte_count] = 0;

    *out_bytes = (char *)bytes;
    *out_byte_count = byte_count;
    return CLIPBOARD_UTF_OK;
}
