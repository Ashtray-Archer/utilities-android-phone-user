# Carpenter Bros. Hardware — Green Grocer draft

This directory is the first customer-facing hardware-store draft of Green
Grocer.

It is deliberately branched from the generic `green-grocer` work. The generic
branch owns the reusable shopper/cart and inventory-boundary design; this branch
shows what that design looks like for one actual neighborhood hardware store.

## Store identity

The store identity is real public information:

- **Carpenter Bros. Hardware**
- 2753 Plymouth Road, Ann Arbor, Michigan 48105
- Do it Best-affiliated neighborhood hardware store
- public categories include hardware, plumbing, paint, electrical, lawn and
  garden/seasonal items, and tools; public listings also describe rental and
  small-engine/service-shop activity.

Sources:

- <https://www.doitbest.com/carpenter-bros-hardware/>
- <https://www.doitbest.com/find-a-store/michigan/>
- <https://www.toro.com/en/toro-location/380044>

The draft does **not** copy or claim the store's actual Epicor inventory,
prices, SKU numbers, aisle codes, photographs, logo, hours, or availability.
Those require an authorized Eagle feed or store-provided data.

## What is real and what is fixture data

Real:

- store name and location;
- the fact that it is a hardware store;
- broad public product/service categories;
- the researched Epicor Eagle inventory semantics inherited from the parent
  Green Grocer work.

Synthetic fixture data:

- every product name in this draft;
- every SKU;
- every price;
- every available quantity;
- every shelf/aisle/bin location;
- every image placeholder;
- the inventory timestamps.

The UI must continue to label the fixture catalog as demo/sample inventory until
an authorized store feed replaces it.

## Draft goal

The customer should be able to answer, quickly:

1. Do you have the thing I need?
2. What does it cost?
3. How many are available?
4. Where is it in the store?
5. Can I put it in my Green Grocer cart while I keep looking?

That is all this draft is trying to prove.

It is not a store-owner application and does not implement purchasing,
receiving, stock adjustments, checkout, payment, reservations, rental
contracts, repair tickets, or writes back to Epicor.

## Type boundary used by the draft

The draft consumes the generic types already settled in the parent branch:

```text
product
    identity  product_identity
    name      Text
    price     money
    unit      Text
    image     product_image

customer_item
    product       product
    availability product_availability
    locations    list shelf_location

customer_catalog
    store         store
    observed_at   time
    source_time   maybe time
    items         list customer_item

cart
    amounts       product_identity ⇀ Number
```

The hardware-store page does not receive `eagle_item`, QOH, committed-order
rules, Compass records, ODBC rows, or any other Epicor-specific representation.
Those belong behind the adapter documented in
[`../customer-inventory-types.md`](../customer-inventory-types.md) and
[`../epicor-eagle-inventory.md`](../epicor-eagle-inventory.md).

## Files

- [`storefront.html`](./storefront.html) — standalone responsive customer-facing
  mock. No framework or build system.
- [`catalog.fixture.json`](./catalog.fixture.json) — synthetic catalog shaped
  like the Green Grocer customer boundary rather than like Eagle.

Serve this directory over any trivial static HTTP server to let
`storefront.html` load the JSON fixture. The page intentionally has no external
runtime dependencies.

## Fixture coverage

The sample catalog includes:

- ordinary positive in-stock inventory;
- an item with several physical locations but one aggregate available amount;
- an out-of-stock item;
- an item whose shopper availability is unknown;
- a decimal stock amount;
- several hardware departments and seller-defined units.

That is enough to exercise the semantic differences discovered in the Eagle
research without pretending that the sample is Carpenter Bros.' real catalog.

## Replacement path for Eagle

When an authorized Eagle/Compass/iNet sample becomes available, do not rewrite
the customer page around Eagle. Replace the fixture producer:

```text
catalog.fixture.json
        ↓ replaced by
authorized Eagle feed
        ↓
Eagle adapter
        ↓
customer_catalog
        ↓
this storefront
```

The acceptance test for the first real integration should be that the
storefront can switch from the fixture `customer_catalog` to the Eagle-backed
`customer_catalog` without learning any new Epicor vocabulary.
