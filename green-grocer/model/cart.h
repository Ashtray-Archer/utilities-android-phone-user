#ifndef GREEN_GROCER_CART_H
#define GREEN_GROCER_CART_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Bounded specimen storage, not a commerce-wide catalog limit. */
#define GG_MAX_PRODUCTS 64u

typedef uint32_t gg_product_id;
typedef uint64_t gg_money;

typedef struct {
    gg_product_id identity; /* Stable fixture token; zero is reserved. */
    const char *name;
    const char *unit;
    gg_money price_cents;   /* One currency: sample USD cents. */
    const char *image_key;  /* NULL requests a local placeholder. */
} gg_product;

typedef struct {
    const gg_product *products; /* Immutable for the cart's lifetime. */
    size_t count;
} gg_catalog;

typedef struct {
    gg_product_id identity;
    uint32_t amount; /* Stored entries are strictly positive. */
} gg_cart_entry;

typedef struct {
    gg_cart_entry entries[GG_MAX_PRODUCTS];
    size_t count; /* Only this prefix is semantic state. */
} gg_cart;

typedef struct {
    const gg_product *product;
    uint32_t amount; /* Zero is allowed in a derived browse row. */
    gg_money subtotal_cents;
} gg_cart_row;

typedef enum {
    GG_OK,
    GG_INVALID_ARGUMENT,
    GG_INVALID_CATALOG,
    GG_INVALID_CART,
    GG_UNKNOWN_PRODUCT,
    GG_AMOUNT_OUT_OF_RANGE,
    GG_MONEY_OVERFLOW,
    GG_CAPACITY
} gg_result;

/* Zero initialization is the empty cart. Public storage is inspectable, but
 * every operation validates it. Outputs and cart bytes change only on GG_OK.
 * Output buffers must not alias catalog/cart storage. No allocator or Android
 * headers, global state, floating point, cached totals or presentation flags. */
gg_result gg_validate(const gg_catalog *catalog, const gg_cart *cart);
gg_result gg_amount(const gg_catalog *catalog, const gg_cart *cart,
                    gg_product_id identity, uint32_t *amount);
gg_result gg_total(const gg_catalog *catalog, const gg_cart *cart,
                   gg_money *total_cents);
gg_result gg_set_amount(const gg_catalog *catalog, gg_cart *cart,
                        gg_product_id identity, uint64_t amount);
gg_result gg_add(const gg_catalog *catalog, gg_cart *cart,
                 gg_product_id identity);
gg_result gg_remove(const gg_catalog *catalog, gg_cart *cart,
                    gg_product_id identity);
/* Derived rows follow catalog order. selected_only filters those same rows. */
gg_result gg_rows(const gg_catalog *catalog, const gg_cart *cart,
                  bool selected_only, gg_cart_row *rows, size_t capacity,
                  size_t *row_count);

#endif
