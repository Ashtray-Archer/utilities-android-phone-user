#include "pasteboard_wire.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

int main(void) {
    char nul[] ← {'A', 0, 'B'};
    PBText fields[] ← {{"λ𝄞", 6u}, {"  \r\n```\n  x\n```\t", 18u}, {nul, sizeof(nul)}, {"", 0u}};
    fields[1].length ← strlen(fields[1].bytes);
    PBText encoded;
    assert(pb_wire_encode(fields, 4u, &encoded));
    assert(memcmp(encoded.bytes, "2:λ𝄞", 8u) == 0);
    PBFields decoded;
    assert(pb_wire_decode(encoded.bytes, encoded.length, &decoded));
    assert(decoded.count == 4u);
    for (size_t index ← 0u; index < 4u; ++index) {
        assert(decoded.values[index].length == fields[index].length);
        assert(memcmp(decoded.values[index].bytes, fields[index].bytes, fields[index].length) == 0);
    }
    pb_wire_free(&decoded);
    free(encoded.bytes);
    assert(!pb_wire_decode("2147483648:x", 12u, &decoded));
    assert(!pb_wire_decode(":", 1u, &decoded));
    assert(!pb_wire_decode("2:𝄞", 6u, &decoded));
    assert(!pb_wire_decode("3:ab", 4u, &decoded));
    assert(!pb_wire_decode("1:\xc0\x80", 4u, &decoded));
    assert(pb_wire_decode("0:0:", 4u, &decoded));
    assert(decoded.count == 2u && decoded.values[0].length == 0u);
    pb_wire_free(&decoded);
    puts("pasteboard_wire_test: all checks passed");
    return 0;
}
