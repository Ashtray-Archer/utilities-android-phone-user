#include "recipe_view.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <uchar.h>

static void require(bool condition, const char *reason) {
    if (condition) return;
    (void)fprintf(stderr, "FAIL native recipe view transport: %s\n", reason);
    exit(81);
}

/* Independent wire literal: lengths count code points, JNI exposes UTF-16.
 * The row contains an owl, an embedded NUL, and text after that NUL. */
static const char16_t fixture[] =
    u"20:green-grocer-view-v17:Recipes0:22:19:4:A🦉\0B10:8:2:Go2:go";

int main(void) {
    const size_t units = sizeof(fixture) / sizeof(fixture[0]) - 1;
    gg_recipe_view view = {0};
    require(gg_recipe_view_decode((const uint16_t *)fixture, units, &view), "Unicode/NUL fixture admitted");
    require(gg_text_is_ascii(&view.title, "Recipes") && view.row_count == 1, "title and exact row count");
    require(view.rows[0].text.length == 5 && view.rows[0].text.characters[0] == 'A' &&
        view.rows[0].text.characters[1] == 0xD83E && view.rows[0].text.characters[2] == 0xDD89 &&
        view.rows[0].text.characters[3] == 0 && view.rows[0].text.characters[4] == 'B', "full UTF-16 row retained");
    require(view.rows[0].control_count == 1 && gg_text_is_ascii(&view.rows[0].controls[0].label, "Go") &&
        gg_text_is_ascii(&view.rows[0].controls[0].token, "go"), "opaque label and token retained");
    uint16_t *retained = view.title.characters;
    for (size_t prefix = 0; prefix < units; ++prefix) {
        require(!gg_recipe_view_decode((const uint16_t *)fixture, prefix, &view), "every truncated prefix refused");
        require(view.title.characters == retained, "truncation preserves the prior view");
    }
    char16_t damaged[sizeof(fixture) / sizeof(fixture[0])];
    memcpy(damaged, fixture, sizeof(fixture));
    damaged[3] = 'X';
    require(!gg_recipe_view_decode((const uint16_t *)damaged, units, &view), "unknown view schema refused");
    memcpy(damaged, fixture, sizeof(fixture));
    for (size_t index = 0; index < units; ++index) if (damaged[index] == 0xDD89) damaged[index] = 'X';
    require(!gg_recipe_view_decode((const uint16_t *)damaged, units, &view), "unpaired surrogate refused");
    static const char16_t overflow[] = u"999999999999999999999999999999999999999999999999999999:bad";
    require(!gg_recipe_view_decode((const uint16_t *)overflow, sizeof(overflow) / 2 - 1, &view), "length overflow refused");
    require(!gg_recipe_view_decode((const uint16_t *)fixture, GG_RECIPE_VIEW_UNITS + 1, &view), "view budget checked before reading");
    require(view.title.characters == retained && view.rows[0].text.characters[4] == 'B', "all refusals preserve the old projection");
    gg_recipe_view_dispose(&view);
    require(view.rows == NULL && view.row_count == 0, "transport storage released");
    puts("PASS native recipe view Unicode, NUL, controls, prefix refusals, schema, surrogate and budget checks");
    return 0;
}
