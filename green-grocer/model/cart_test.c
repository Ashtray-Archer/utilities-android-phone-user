#include "cart.h"
#include "../fixtures/catalog.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #condition); \
        exit(EXIT_FAILURE); \
    } \
} while (0)

static void check_total(const gg_catalog *catalog, const gg_cart *cart,
                        gg_money expected) {
    gg_money observed = UINT64_MAX;
    CHECK(gg_total(catalog, cart, &observed) == GG_OK);
    CHECK(observed == expected);
}

static void check_amount(const gg_catalog *catalog, const gg_cart *cart,
                         gg_product_id identity, uint32_t expected) {
    uint32_t observed = UINT32_MAX;
    CHECK(gg_amount(catalog, cart, identity, &observed) == GG_OK);
    CHECK(observed == expected);
}

static void basic_transitions(void) {
    const gg_catalog *catalog = &gg_sample_catalog;
    gg_cart cart = {0};
    CHECK(gg_validate(catalog, &cart) == GG_OK);
    check_total(catalog, &cart, 0);
    CHECK(gg_add(catalog, &cart, GG_BANANAS) == GG_OK);
    CHECK(gg_add(catalog, &cart, GG_BANANAS) == GG_OK);
    CHECK(cart.count == 1);
    check_amount(catalog, &cart, GG_BANANAS, 2);
    check_total(catalog, &cart, 258);
    CHECK(gg_set_amount(catalog, &cart, GG_BANANAS, 1) == GG_OK);
    CHECK(gg_set_amount(catalog, &cart, GG_APPLE_BAGS, 2) == GG_OK);
    CHECK(gg_set_amount(catalog, &cart, GG_CEREAL, 3) == GG_OK);
    /* Independent expected monetary value, not computed by the model API. */
    check_total(catalog, &cart, 2264);
    CHECK(cart.count == 3);
    CHECK(gg_set_amount(catalog, &cart, GG_APPLE_BAGS, 0) == GG_OK);
    CHECK(cart.count == 2);
    check_amount(catalog, &cart, GG_APPLE_BAGS, 0);
    check_amount(catalog, &cart, GG_CEREAL, 3);
    check_total(catalog, &cart, 1266);
    CHECK(gg_remove(catalog, &cart, GG_BANANAS) == GG_OK);
    CHECK(gg_remove(catalog, &cart, GG_CEREAL) == GG_OK);
    gg_cart empty;
    memcpy(&empty, &cart, sizeof cart);
    CHECK(gg_remove(catalog, &cart, GG_CEREAL) == GG_OK);
    CHECK(memcmp(&empty, &cart, sizeof cart) == 0);
    CHECK(cart.count == 0);
    check_total(catalog, &cart, 0);
    CHECK(gg_add(catalog, &cart, GG_CEREAL) == GG_OK);
    check_total(catalog, &cart, 379);
}

static void identity_and_projections(void) {
    /* Duplicate labels and prices are legal; identities remain distinct. */
    const gg_product products[] = {
        {81, "Same name", "1 package", 25, NULL},
        {13, "Same name", "1 package", 25, NULL},
        {57, "Third", "1 package", 50, NULL}
    };
    const gg_catalog catalog = {products, 3};
    gg_cart cart = {0};
    CHECK(gg_add(&catalog, &cart, 13) == GG_OK);
    CHECK(gg_add(&catalog, &cart, 81) == GG_OK);
    CHECK(gg_add(&catalog, &cart, 13) == GG_OK);
    check_total(&catalog, &cart, 75);
    check_amount(&catalog, &cart, 81, 1);
    check_amount(&catalog, &cart, 13, 2);
    gg_cart_row rows[GG_MAX_PRODUCTS];
    size_t count = 99;
    CHECK(gg_rows(&catalog, &cart, false, rows, GG_MAX_PRODUCTS, &count) == GG_OK);
    CHECK(count == 3);
    CHECK(rows[0].product->identity == 81 && rows[0].amount == 1);
    CHECK(rows[1].product->identity == 13 && rows[1].subtotal_cents == 50);
    CHECK(rows[2].product->identity == 57 && rows[2].amount == 0);
    CHECK(gg_rows(&catalog, &cart, true, rows, GG_MAX_PRODUCTS, &count) == GG_OK);
    CHECK(count == 2 && rows[0].product->identity == 81);
    const gg_product reordered[] = {products[2], products[1], products[0]};
    const gg_catalog reverse = {reordered, 3};
    check_total(&reverse, &cart, 75);
    check_amount(&reverse, &cart, 13, 2);
    CHECK(gg_rows(&reverse, &cart, true, rows, GG_MAX_PRODUCTS, &count) == GG_OK);
    CHECK(count == 2 && rows[0].product->identity == 13);
    CHECK(rows[1].product->identity == 81);
    gg_cart before;
    memcpy(&before, &cart, sizeof cart);
    count = 77;
    rows[0].amount = 77;
    CHECK(gg_rows(&catalog, &cart, false, rows, 1, &count) == GG_CAPACITY);
    CHECK(count == 77 && rows[0].amount == 77);
    CHECK(memcmp(&before, &cart, sizeof cart) == 0);
    CHECK(gg_remove(&catalog, &cart, 13) == GG_OK);
    CHECK(gg_remove(&catalog, &cart, 81) == GG_OK);
    CHECK(gg_rows(&catalog, &cart, true, NULL, 0, &count) == GG_OK);
    CHECK(count == 0);
}

static void rejected_transitions_preserve_state(void) {
    const gg_catalog *catalog = &gg_sample_catalog;
    gg_cart cart = {0};
    CHECK(gg_add(catalog, &cart, GG_BANANAS) == GG_OK);
    gg_cart before;
    memcpy(&before, &cart, sizeof cart);
    CHECK(gg_add(catalog, &cart, 999) == GG_UNKNOWN_PRODUCT);
    CHECK(memcmp(&before, &cart, sizeof cart) == 0);
    CHECK(gg_remove(catalog, &cart, 999) == GG_UNKNOWN_PRODUCT);
    CHECK(memcmp(&before, &cart, sizeof cart) == 0);
    CHECK(gg_set_amount(catalog, &cart, GG_BANANAS,
                        (uint64_t)UINT32_MAX + 1) == GG_AMOUNT_OUT_OF_RANGE);
    CHECK(memcmp(&before, &cart, sizeof cart) == 0);
    CHECK(gg_set_amount(catalog, &cart, GG_BANANAS, UINT32_MAX) == GG_OK);
    /* Literal oracle: 129 cents times the largest 32-bit amount. */
    check_total(catalog, &cart, UINT64_C(554050781055));
    memcpy(&before, &cart, sizeof cart);
    CHECK(gg_add(catalog, &cart, GG_BANANAS) == GG_AMOUNT_OUT_OF_RANGE);
    CHECK(memcmp(&before, &cart, sizeof cart) == 0);
}

static void exact_money_bounds(void) {
    const gg_product products[] = {
        {1, "Boundary", "1", UINT64_MAX, NULL},
        {2, "One cent", "1", 1, NULL},
        {3, "Free", "1", 0, NULL}
    };
    const gg_catalog catalog = {products, 3};
    gg_cart cart = {0};
    CHECK(gg_add(&catalog, &cart, 1) == GG_OK);
    check_total(&catalog, &cart, UINT64_MAX);
    gg_cart before;
    memcpy(&before, &cart, sizeof cart);
    CHECK(gg_add(&catalog, &cart, 1) == GG_MONEY_OVERFLOW); /* Multiplication. */
    CHECK(memcmp(&before, &cart, sizeof cart) == 0);
    CHECK(gg_add(&catalog, &cart, 2) == GG_MONEY_OVERFLOW); /* Sum. */
    CHECK(memcmp(&before, &cart, sizeof cart) == 0);
    CHECK(gg_set_amount(&catalog, &cart, 1, 2) == GG_MONEY_OVERFLOW);
    CHECK(memcmp(&before, &cart, sizeof cart) == 0);
    CHECK(gg_set_amount(&catalog, &cart, 3, UINT32_MAX) == GG_OK);
    check_total(&catalog, &cart, UINT64_MAX);
    CHECK(gg_remove(&catalog, &cart, 1) == GG_OK);
    check_total(&catalog, &cart, 0);
    CHECK(gg_add(&catalog, &cart, 2) == GG_OK);
    check_total(&catalog, &cart, 1);
}

static void known_bad_states(void) {
    const gg_catalog *catalog = &gg_sample_catalog;
    gg_cart bad = {.entries = {{GG_BANANAS, 0}}, .count = 1};
    /* Targeted historical defect: retaining a zero entry must fail. */
    CHECK(gg_validate(catalog, &bad) == GG_INVALID_CART);
    gg_cart before;
    memcpy(&before, &bad, sizeof bad);
    CHECK(gg_add(catalog, &bad, GG_BANANAS) == GG_INVALID_CART);
    CHECK(memcmp(&before, &bad, sizeof bad) == 0);
    bad.entries[0] = (gg_cart_entry){999, 1};
    gg_money total = 77;
    /* Targeted historical defect: silently omitting an unresolved key. */
    CHECK(gg_total(catalog, &bad, &total) == GG_INVALID_CART);
    CHECK(total == 77);
    gg_cart_row row = {.amount = 77};
    size_t count = 77;
    CHECK(gg_rows(catalog, &bad, true, &row, 1, &count) == GG_INVALID_CART);
    CHECK(row.amount == 77 && count == 77);
    bad.entries[0] = (gg_cart_entry){GG_BANANAS, 1};
    bad.entries[1] = bad.entries[0];
    bad.count = 2;
    CHECK(gg_validate(catalog, &bad) == GG_INVALID_CART);
    bad.count = GG_MAX_PRODUCTS + 1;
    CHECK(gg_validate(catalog, &bad) == GG_INVALID_CART);
    const gg_product duplicate_ids[] = {
        {1, "A", "1", 10, NULL}, {1, "B", "1", 20, NULL}
    };
    const gg_catalog invalid = {duplicate_ids, 2};
    gg_cart empty = {0};
    CHECK(gg_validate(&invalid, &empty) == GG_INVALID_CATALOG);
    CHECK(gg_validate(NULL, &empty) == GG_INVALID_ARGUMENT);
    CHECK(gg_validate(catalog, NULL) == GG_INVALID_ARGUMENT);
    CHECK(gg_total(catalog, &empty, NULL) == GG_INVALID_ARGUMENT);
    const gg_catalog empty_catalog = {NULL, 0};
    check_total(&empty_catalog, &empty, 0);
    CHECK(gg_add(&empty_catalog, &empty, 1) == GG_UNKNOWN_PRODUCT);
}

static void storage_bound(void) {
    gg_product products[GG_MAX_PRODUCTS];
    for (size_t index = 0; index < GG_MAX_PRODUCTS; ++index) {
        products[index] = (gg_product){
            .identity = (gg_product_id)index + 1,
            .name = "Same label",
            .unit = "1 package",
            .price_cents = 1,
            .image_key = NULL
        };
    }
    const gg_catalog catalog = {products, GG_MAX_PRODUCTS};
    gg_cart cart = {0};
    for (size_t index = 0; index < GG_MAX_PRODUCTS; ++index) {
        CHECK(gg_add(&catalog, &cart, products[index].identity) == GG_OK);
    }
    CHECK(cart.count == GG_MAX_PRODUCTS);
    check_total(&catalog, &cart, 64);
    CHECK(gg_add(&catalog, &cart, 1) == GG_OK); /* Full cart can update. */
    check_total(&catalog, &cart, 65);
    CHECK(gg_remove(&catalog, &cart, 32) == GG_OK);
    CHECK(cart.count == GG_MAX_PRODUCTS - 1);
    CHECK(gg_add(&catalog, &cart, 32) == GG_OK);
    check_total(&catalog, &cart, 65);
    const gg_catalog too_large = {products, GG_MAX_PRODUCTS + 1};
    CHECK(gg_validate(&too_large, &cart) == GG_INVALID_CATALOG);
}

int main(void) {
    basic_transitions();
    identity_and_projections();
    rejected_transitions_preserve_state();
    exact_money_bounds();
    known_bad_states();
    storage_bound();
    puts("PASS Green Grocer cart semantic cases");
    return EXIT_SUCCESS;
}
