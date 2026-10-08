#include "cart.h"

static const gg_product *find_product(const gg_catalog *catalog,
                                     gg_product_id identity) {
    for (size_t index = 0; index < catalog->count; ++index) {
        if (catalog->products[index].identity == identity) {
            return &catalog->products[index];
        }
    }
    return NULL;
}

static size_t find_entry(const gg_cart *cart, gg_product_id identity) {
    for (size_t index = 0; index < cart->count; ++index) {
        if (cart->entries[index].identity == identity) {
            return index;
        }
    }
    return cart->count;
}

static gg_result validate_catalog(const gg_catalog *catalog) {
    if (catalog == NULL) {
        return GG_INVALID_ARGUMENT;
    }
    if (catalog->count > GG_MAX_PRODUCTS ||
        (catalog->count > 0 && catalog->products == NULL)) {
        return GG_INVALID_CATALOG;
    }
    for (size_t index = 0; index < catalog->count; ++index) {
        const gg_product *product = &catalog->products[index];
        if (product->identity == 0 || product->name == NULL || product->unit == NULL) {
            return GG_INVALID_CATALOG;
        }
        for (size_t earlier = 0; earlier < index; ++earlier) {
            if (catalog->products[earlier].identity == product->identity) {
                return GG_INVALID_CATALOG;
            }
        }
    }
    return GG_OK;
}

/* One checked projection used by validation and all mutation boundaries.
 * A missing key is an invariant violation, never an omitted subtotal. */
static gg_result checked_total(const gg_catalog *catalog, const gg_cart *cart,
                               gg_money *total) {
    gg_result result = validate_catalog(catalog);
    if (result != GG_OK) {
        return result;
    }
    if (cart == NULL) {
        return GG_INVALID_ARGUMENT;
    }
    if (cart->count > catalog->count) {
        return GG_INVALID_CART;
    }
    gg_money sum = 0;
    for (size_t index = 0; index < cart->count; ++index) {
        const gg_cart_entry *entry = &cart->entries[index];
        const gg_product *product = find_product(catalog, entry->identity);
        if (entry->amount == 0 || product == NULL) {
            return GG_INVALID_CART;
        }
        for (size_t earlier = 0; earlier < index; ++earlier) {
            if (cart->entries[earlier].identity == entry->identity) {
                return GG_INVALID_CART;
            }
        }
        if (product->price_cents > UINT64_MAX / entry->amount) {
            return GG_MONEY_OVERFLOW;
        }
        gg_money subtotal = product->price_cents * entry->amount;
        if (subtotal > UINT64_MAX - sum) {
            return GG_MONEY_OVERFLOW;
        }
        sum += subtotal;
    }
    *total = sum;
    return GG_OK;
}

gg_result gg_validate(const gg_catalog *catalog, const gg_cart *cart) {
    gg_money ignored;
    return checked_total(catalog, cart, &ignored);
}

gg_result gg_total(const gg_catalog *catalog, const gg_cart *cart,
                   gg_money *total_cents) {
    if (total_cents == NULL) {
        return GG_INVALID_ARGUMENT;
    }
    return checked_total(catalog, cart, total_cents);
}

gg_result gg_amount(const gg_catalog *catalog, const gg_cart *cart,
                    gg_product_id identity, uint32_t *amount) {
    if (amount == NULL) {
        return GG_INVALID_ARGUMENT;
    }
    gg_result result = gg_validate(catalog, cart);
    if (result != GG_OK) {
        return result;
    }
    if (find_product(catalog, identity) == NULL) {
        return GG_UNKNOWN_PRODUCT;
    }
    size_t index = find_entry(cart, identity);
    *amount = index == cart->count ? 0 : cart->entries[index].amount;
    return GG_OK;
}

gg_result gg_set_amount(const gg_catalog *catalog, gg_cart *cart,
                        gg_product_id identity, uint64_t amount) {
    gg_result result = gg_validate(catalog, cart);
    if (result != GG_OK) {
        return result;
    }
    if (find_product(catalog, identity) == NULL) {
        return GG_UNKNOWN_PRODUCT;
    }
    if (amount > UINT32_MAX) {
        return GG_AMOUNT_OUT_OF_RANGE;
    }
    gg_cart candidate = *cart;
    size_t index = find_entry(&candidate, identity);
    if (amount == 0) {
        if (index < candidate.count) {
            for (size_t next = index + 1; next < candidate.count; ++next) {
                candidate.entries[next - 1] = candidate.entries[next];
            }
            --candidate.count;
            candidate.entries[candidate.count] = (gg_cart_entry){0};
        }
    } else if (index < candidate.count) {
        candidate.entries[index].amount = (uint32_t)amount;
    } else {
        if (candidate.count == GG_MAX_PRODUCTS) {
            return GG_CAPACITY;
        }
        candidate.entries[candidate.count++] =
            (gg_cart_entry){.identity = identity, .amount = (uint32_t)amount};
    }
    result = gg_validate(catalog, &candidate);
    if (result == GG_OK) {
        *cart = candidate;
    }
    return result;
}

gg_result gg_add(const gg_catalog *catalog, gg_cart *cart,
                 gg_product_id identity) {
    uint32_t amount;
    gg_result result = gg_amount(catalog, cart, identity, &amount);
    if (result != GG_OK) {
        return result;
    }
    return gg_set_amount(catalog, cart, identity, (uint64_t)amount + 1);
}

gg_result gg_remove(const gg_catalog *catalog, gg_cart *cart,
                    gg_product_id identity) {
    return gg_set_amount(catalog, cart, identity, 0);
}

gg_result gg_rows(const gg_catalog *catalog, const gg_cart *cart,
                  bool selected_only, gg_cart_row *rows, size_t capacity,
                  size_t *row_count) {
    if (row_count == NULL) {
        return GG_INVALID_ARGUMENT;
    }
    gg_result result = gg_validate(catalog, cart);
    if (result != GG_OK) {
        return result;
    }
    size_t needed = selected_only ? cart->count : catalog->count;
    if (capacity < needed) {
        return GG_CAPACITY;
    }
    if (needed > 0 && rows == NULL) {
        return GG_INVALID_ARGUMENT;
    }
    size_t written = 0;
    for (size_t index = 0; index < catalog->count; ++index) {
        const gg_product *product = &catalog->products[index];
        size_t entry = find_entry(cart, product->identity);
        uint32_t amount = entry == cart->count ? 0 : cart->entries[entry].amount;
        if (!selected_only || amount > 0) {
            rows[written++] = (gg_cart_row){
                .product = product,
                .amount = amount,
                .subtotal_cents = product->price_cents * amount
            };
        }
    }
    *row_count = written;
    return GG_OK;
}
