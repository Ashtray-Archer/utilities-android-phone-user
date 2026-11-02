#ifndef PASTEBOARD_WIRE_H
#define PASTEBOARD_WIRE_H
#include <stdbool.h>
#include <stddef.h>

/* Protocol adapter only: decimal Unicode-scalar length, ':', exact UTF-8.
 * A field may contain colons, line breaks, significant whitespace or NUL. */
typedef struct {
    char *bytes;
    size_t length;
} PBText;
typedef struct {
    PBText *values;
    size_t count;
} PBFields;

bool pb_wire_encode(const PBText *fields, size_t count, PBText *encoded);
bool pb_wire_decode(const char *bytes, size_t length, PBFields *decoded);
void pb_wire_free(PBFields *fields);
bool pb_text_equal(PBText text, const char *ascii);
#endif
