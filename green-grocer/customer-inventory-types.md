# Green Grocer customer inventory type specification

This specification defines the narrow read-only boundary from an Epicor Eagle
inventory feed into the existing customer-facing Green Grocer product list and
cart.

It builds on:

- [`cart-types.md`](./cart-types.md), which settles the minimal customer-facing
  `product` and `cart`;
- [`epicor-eagle-inventory.md`](./epicor-eagle-inventory.md), which records the
  Eagle research and the constraints discovered there.

The goal is not to model Epicor as a whole. The goal is to let Green Grocer say,
for a product shown to a customer:

> this is the item, this is its current display price, this much is available,
> and these are the places in the store where it can be found.

No store-owner UI, purchasing, receiving, stock adjustment, checkout, payment,
or write-back to Eagle belongs in this boundary.

## Existing front-end types remain

The existing front end already has the right small customer concepts:

```text
product
    identity  product_identity
    name      Text
    price     money
    unit      Text
    image     product_image

cart
    amounts   product_identity ⇀ Number
```

These remain unchanged for the current mock and renderer.

Inventory is **adjacent to** `product`; it is not a new field of `cart` and it is
not a reason to turn `product` into an Epicor record.

## Customer-facing inventory types

The front end needs only these additional semantic types:

```text
store
    identity  store_identity
    name      Text

customer_item
    product       product
    availability product_availability
    locations    list shelf_location

customer_catalog
    store         store
    observed_at   time
    source_time   maybe time
    items         list customer_item
```

`customer_catalog` is the complete read-only view for one selected store at one
observed point in time.

`customer_item` is what the product-list renderer consumes. It deliberately
contains the already-settled `product` rather than duplicating name, price,
unit, or image fields.

### Availability

```text
product_availability
    unknown
    out_of_stock
    in_stock stock_amount
```

`stock_amount` means an **exact positive amount of seller-defined units**.
It is not assumed to be an integer. Eagle permits decimal inventory/sale
quantities, so this semantic type must eventually support values such as `0.5`
as well as `1`, `2`, and `17`.

The representation is intentionally not fixed here. The required law is:

```text
stock_amount > 0
```

Binary floating point is not an acceptable fundamental representation for this
business quantity.

The three cases have distinct meanings:

- `unknown`: Green Grocer does not have an authoritative shopper-availability
  value for this item in the current snapshot;
- `out_of_stock`: Eagle's resolved available amount is zero or negative;
- `in_stock amount`: Eagle's resolved available amount is positive.

A negative Eagle quantity is therefore never displayed as negative customer
stock.

### Physical location

```text
shelf_location
    label  Text
```

A location is presently only the store's human-readable location code or label,
for example `Aisle 7`, `PLUMBING`, or `Endcap 2`.

The customer type is deliberately a list:

```text
locations : list shelf_location
```

not a mapping from location to amount. Eagle can associate several physical
locations with one SKU while maintaining only one aggregate store-level stock
amount. Green Grocer must not invent a quantity for each shelf.

## Eagle-side source types

The adapter may know Eagle-specific vocabulary. The customer front end must not.

```text
eagle_source
    identity  eagle_source_identity

eagle_store
    source    eagle_source_identity
    identity  eagle_store_identity
    name      Text

eagle_item_identity
    source    eagle_source_identity
    sku       eagle_sku

eagle_item
    identity                  eagle_item_identity
    description               Text
    pricing_unit              Text
    upcs                      list eagle_upc
    primary_upc               maybe eagle_upc
    manufacturer_part         maybe Text
    web_visibility            maybe eagle_web_visibility
    pos_sellable              maybe Bool
    discontinued              maybe Bool
    decimal_quantity_allowed  maybe Bool

eagle_store_inventory
    store             eagle_store_identity
    item              eagle_item_identity
    display_price     money
    available         maybe eagle_quantity
    quantity_on_hand  maybe eagle_quantity
    quantity_on_order maybe eagle_quantity
    locations         list eagle_location

eagle_snapshot
    observed_at   time
    source_time   maybe time
    store         eagle_store
    items         list (eagle_item, eagle_store_inventory)
```

`eagle_quantity` is an exact signed decimal source quantity. It preserves the
source faithfully, including zero, negative values, and decimal values.

It is **not** a customer-facing stock type.

## Identity boundary

Eagle SKU is the source item identity. UPC is an alternate identifier, not the
cart key.

The production Green Grocer `product_identity` is created by the adapter from
an Eagle source identity and SKU:

```text
product_identity from eagle item
    <- eagle_item_identity.source
    <- eagle_item_identity.sku
```

The representation stays opaque outside the adapter. The renderer and cart only
see `product_identity`.

This gives two useful properties:

1. two unrelated Eagle installations may reuse the same SKU without colliding;
2. changing description, price, unit, image, UPC, or stock does not change the
   product's identity.

The selected store is not part of `product_identity`; store-specific price and
stock remain store facts.

## Mapping Eagle into the front end

For every Eagle item that is allowed to appear to customers, the adapter
constructs the existing `product`:

```text
product.identity <- identity from eagle item
product.name     <- eagle_item.description
product.price    <- eagle_store_inventory.display_price
product.unit     <- eagle_item.pricing_unit
product.image    <- image_for product.identity eagle_item.upcs
```

`image_for` is Green Grocer enrichment. Failure to find an image must not make
inventory ingestion fail; the front end may use a placeholder image.

The adapter constructs availability independently:

```text
availability from eagle available

no authoritative available value
    -> unknown

available <= 0
    -> out_of_stock

available > 0
    -> in_stock available
```

The adapter constructs locations by preserving Eagle's location labels:

```text
shelf_locations <- map shelf_location over eagle_store_inventory.locations
```

No per-location quantities are inferred.

## Which Eagle records become customer items

The adapter must not assume that every inventory record belongs in the public
catalog.

The first real store feed should supply its intended web/customer visibility
rule. Where Eagle supplies relevant controls such as Web visibility, POS
sellability, or discontinued state, the adapter should apply the store's
explicit policy before constructing `customer_item`.

Until that policy is known from the actual store/feed, the type boundary keeps
those Eagle fields optional and does not invent a universal formula such as:

```text
visible = quantity_on_hand > 0
```

Out-of-stock products may still legitimately remain visible.

## Snapshot freshness

Eagle itself may update immediately at POS while Green Grocer receives periodic
exports. Therefore a customer catalog is a timestamped observation rather than
an assertion of perfect simultaneity.

```text
customer_catalog.observed_at
```

is the time Green Grocer accepted the snapshot.

```text
customer_catalog.source_time
```

is present only when the Eagle feed supplies a trustworthy generation/change
time.

Freshness policy is derived from these timestamps; it is not baked into each
product. The application can later decide to display `updated 4 minutes ago`,
warn about stale stock, or suppress counts after a threshold without changing
inventory identity or cart semantics.

## Adapter boundary

The minimum semantic interface is:

```text
read eagle inventory
    eagle_feed -> eagle_snapshot

show eagle inventory to customer
    eagle_snapshot -> customer_catalog
```

The actual feed format is intentionally absent. Research found plausible
Compass/report/data-file integrations, but no actual store export has yet been
obtained. CSV columns, FTP paths, report names, API endpoints, and Eagle table
names are therefore not part of this specification.

Once the store supplies an authorized sample feed, the parser can live beneath
`read eagle inventory` without changing the customer types.

## Front-end observations

The customer-facing renderer only needs ordinary observations such as:

```text
products in catalog
availability of product
locations of product
last inventory observation time
```

A product tile/list row can therefore render:

```text
name
price / unit
availability
locations
image
```

For example:

```text
3/8 in. zinc hex nut
$0.29 / each
143 available
Aisle 12 · Bin 4
```

or:

```text
20 lb jasmine rice
$18.99 / bag
out of stock
Aisle 5
```

or, when the feed does not expose Eagle's resolved availability:

```text
availability unavailable
Aisle 5
```

The renderer does not know the words Eagle, SKU, QOH, Compass, UPC, ODBC, or
DataPump unless a future customer feature specifically needs one of them.

## Cart relationship

The existing cart remains:

```text
cart
    amounts  product_identity ⇀ Number
```

for the current integer-quantity mock.

Inventory does not live in the cart. Adding an item to a Green Grocer cart does
not decrement, reserve, or write anything to Eagle in this read-only phase.

Before a future real-store checkout/cart implementation accepts decimal-sale
items, the cart amount type will need to be reconsidered. That is separate from
this inventory-display integration and should be driven by an actual Eagle
fixture containing such an item.

## Required invariants

The adapter must preserve these rules:

1. `product_identity` is stable across price, description, image, UPC, location,
   and stock changes.
2. UPC never substitutes for Eagle SKU as the source item key.
3. customer `in_stock` always carries a strictly positive exact amount.
4. zero or negative Eagle `available` becomes `out_of_stock`.
5. missing authoritative Eagle `available` becomes `unknown`; QOH is not
   silently relabelled as availability.
6. several Eagle locations remain several labels attached to one store-level
   availability value.
7. no negative customer stock is displayed.
8. no binary floating-point quantity is introduced at the semantic boundary.
9. source/feed timestamps are retained so stale external inventory can be
   distinguished from current Eagle state.
10. the adapter is read-only: no Green Grocer action mutates Eagle inventory in
    this phase.

## First acceptance fixture

The first real or scrubbed Eagle fixture should contain enough rows to exercise:

```text
ordinary positive integer stock
zero available
negative QOH
positive decimal available quantity
multiple locations for one SKU
multiple UPCs for one SKU
out-of-stock but visible product
hidden/non-customer product
missing authoritative Available field
price change without identity change
stock change without identity change
```

The acceptance proof is that the same customer front end can render those rows
through `customer_catalog` without containing Eagle-specific logic.

## Non-goals

This specification does not introduce:

- inventory editing;
- reservations;
- checkout or orders;
- payment;
- tax;
- customer accounts;
- purchasing or receiving;
- supplier/vendor types;
- per-location stock quantities that Eagle did not supply;
- an invented public Eagle API;
- direct Eagle database access;
- a new generic commerce ontology.

The whole purpose of the adapter is to keep those concerns out of the present
customer-facing Green Grocer application.
