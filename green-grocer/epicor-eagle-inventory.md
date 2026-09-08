# Epicor Eagle inventory integration research

Research date: 2026-09-08

This note is about **read-only customer-facing inventory** for Green Grocer. It is
not a design for store administration, purchasing, receiving, checkout,
payment, accounting, or writing stock changes back to Eagle.

It was written against `Ashtray-Archer/utilities-android-phone-user`, branch
`green-grocer`, starting at commit
`955fffc673386254e7d78a70985e48fbedd4d325` (`Settle minimal Green Grocer cart
types`). The existing `cart-types.md` intentionally models a fake local catalog
without SKU or inventory semantics. This note does not overturn that decision;
it defines the first evidence-based boundary between a real inventory source and
that small shopper model.

## Conclusion

Assuming the hardware store is running **Epicor Eagle** is reasonable enough to
design a provisional adapter, but not enough to invent an Eagle wire protocol.

The important facts are:

1. Eagle really does maintain store inventory from POS activity. Epicor's POS
   documentation says inventory data is updated automatically from POS, and the
   Eagle field documentation says quantity on hand decreases when an item is
   sold.
2. Eagle contains the customer-facing data Green Grocer needs: SKU,
   description, pricing, available quantity, quantity on hand, UPCs,
   manufacturer part number, selling unit, store, and physical item locations.
3. An item may have **up to six physical location codes**, but Eagle aggregates
   counts from those locations into **one store-level quantity on hand**. Green
   Grocer must not invent per-shelf quantities.
4. `quantity_on_hand` is not the same concept as shopper availability. Eagle's
   available quantity is affected by committed orders and configuration. Prefer
   Eagle's own `Available` value rather than reimplementing its availability
   rule.
5. Eagle quantity values are not just positive integers: QOH can be zero,
   negative, and decimal, and Eagle can permit items to be sold in decimal
   quantities. The current mock cart's positive-integer `Number` therefore
   cannot be assumed to be the eventual real-store quantity type.
6. UPC is not item identity. Eagle supports multiple UPCs per item and, in
   multistore configurations, the same UPC can be associated with different
   SKUs. Use Eagle SKU, namespaced by the Eagle source, as the provisional item
   identity; keep UPCs as alternate identifiers.
7. Public Epicor material shows customer-facing Eagle e-commerce and export
   mechanisms, but this research did **not** find a documented general-purpose
   public Eagle REST/OpenAPI contract on which Green Grocer should depend.
8. Existing commercial Eagle integrations provide strong evidence for a
   supported **Compass/report/data-file -> FTP -> middleware** model. That is a
   better first integration hypothesis than scraping Eagle's database or
   inventing HTTP endpoints.
9. Epicor's published terms for Transoft ODBC/DataPump explicitly restrict
   providing Eagle-derived data to others, including Internet catalog/retail
   applications, unless specifically authorized by Epicor. ODBC is therefore
   not the default Green Grocer route even though it is technically capable of
   extracting data.

The provisional architecture should be:

```text
Epicor Eagle
    -> Epicor-provisioned customer/catalog inventory feed
    -> Green Grocer Eagle adapter
    -> normalized catalog snapshot
    -> shopper application
```

The shopper application should never need to know whether the source behind the
normalized snapshot is Eagle, Propello, another POS, or a fixture file.

## What Eagle actually stores that matters to us

Epicor's current Eagle Inventory Maintenance help enumerates fields including
`SKU`, `Store`, `Desc`, `UPC`, `Mfg #`, `Qty On Hand`, `Qty On Order`, `Retail`,
`Promotion`, selling/pricing units, `Discontinued`, `POS Sellable`, `Decimal
quantity allowed`, and `Web?`.

Eagle Mobile's Price Check view is especially useful as a clue to the correct
customer boundary. Epicor says that screen can be shown to a customer because
it contains customer-friendly information: retail/promotion/list/loyalty
pricing, `Available` quantity, QOO, location, and manufacturer part number.
That is much closer to Green Grocer's needs than Eagle's complete inventory
record, which also contains costs, purchasing, vendor, history, and internal
management data.

### SKU and UPC are different things

Eagle permits several UPC records on an item and optionally marks one as the
primary UPC. It also documents multistore cases in which the same UPC is linked
to different SKUs for different co-op inventories.

Consequences for Green Grocer:

```text
Eagle SKU      -> source item identity
UPC            -> alternate scan/search identity
manufacturer # -> alternate human/search identity
```

Do not use UPC as the cart key. Do not assume a UPC is globally one-to-one with
an Eagle SKU.

A bare SKU also should not be treated as globally unique across unrelated Eagle
installations. The durable provisional key is conceptually:

```text
eagle_item_identity = eagle_source × eagle_sku
```

The selected store is *not* part of that item identity. Stock and pricing are
store-specific facts about the item.

### QOH is store-specific and can be strange

Eagle defines quantity on hand as the amount physically in the store. It is
store-specific and decreases when an item is sold at POS. The same official
field help explicitly permits a negative QOH, for example when stock was not
received correctly or more units were sold than Eagle believed were on hand.
Another Eagle help page documents decimal QOH values and Eagle has an item flag
controlling whether decimal sale quantities are allowed.

Therefore this is wrong:

```text
quantity_on_hand : Number
```

under the current Idriç decision that `Number = 1, 2, 3, ...`.

The Eagle adapter needs an **exact signed decimal source quantity**. This note
calls that semantic type `eagle_quantity` without deciding its eventual Idriç
representation. Binary floating point is not appropriate for an inventory
quantity that comes from a decimal business system.

Negative source values should be preserved for diagnostics but never rendered
to a shopper as `-3 in stock`.

### QOH is not availability

Eagle tracks committed quantities, and option 8121 controls whether all
committed quantity or only current committed quantity participates in Eagle's
quantity-available calculation. That means Green Grocer should not casually
implement:

```text
available = quantity_on_hand - committed
```

from whatever fields happen to appear in an export.

Preferred rule:

> If an authorized Eagle feed can provide Eagle's resolved `Available` value,
> ingest that value and treat it as authoritative for shopper availability.

If a feed gives only QOH, call the field `quantity_on_hand`; do not silently
rename it to `available`. A store may later deliberately choose a simpler
shopper rule, but that would be store policy rather than an inferred Eagle
semantic.

For presentation, normalize the resolved available value as:

```text
available <= 0  -> out_of_stock
available > 0   -> in_stock available
```

The raw source value remains available to the adapter for diagnosis.

### A physical location is not a stock bucket

Eagle added support for up to six location codes per item. Its physical
inventory documentation is explicit that counts entered for several locations
are added together and update one QOH figure.

So Green Grocer may legitimately display something like:

```text
7 available
Aisle 4; Back wall; Endcap 2
```

but it must not manufacture:

```text
Aisle 4: 4
Back wall: 2
Endcap 2: 1
```

unless some future source actually supplies those allocations.

The type should consequently be:

```text
locations : list eagle_location
```

not:

```text
locations : eagle_location -> quantity
```

### Item visibility is not simply `QOH > 0`

Eagle has item controls including `Web?`, `POS Sellable`, and `Discontinued`.
Its iNet documentation also describes Web flags used to decide which items are
shown or sold online.

The first real feed should therefore include the store's intended online/web
visibility signal if possible. Green Grocer should not publish every internal
inventory record merely because it exists in Eagle.

## Integration surfaces found

### 1. Eagle iNet / Eagle eCommerce

Epicor currently sells an Eagle eCommerce/iNet product whose inventory is
synchronized with Eagle and whose customers can see product information.
Epicor describes the online transaction as being created in Eagle so sales and
inventory remain in one system.

This proves that customer-facing Eagle inventory is a supported use case. Older
Epicor iNet help also describes a web-server boundary: Internet customers talk
to the web server rather than connecting directly to Eagle, and Eagle supplies
the requested information to that server.

What the public documentation did **not** establish is a supported public API
that an unrelated Green Grocer client can simply call. If this particular store
already licenses iNet/eCommerce, its supported integration surface is worth
checking before building anything else.

### 2. Compass/report/data files delivered to a file drop

This is the most concrete external integration mechanism found.

Modern Retail documents its deployed Eagle integration as:

```text
Epicor -> product/data files -> dedicated FTP site
       -> middleware parses files
       -> Shopify/BigCommerce/WooCommerce/Magento
```

Inventory and pricing changes are sent through the same middleware. Their
published schedule checks product/inventory work several times per hour; a
separate Eagle/Orgill integration example documents inventory-and-price change
files on a 15-minute cadence with slower full snapshots.

Those timings are **examples of a commercial integration, not an Eagle service
level guarantee**. Their significance is architectural: an Eagle store can
have immediately updated internal POS inventory while an external catalog sees
a periodically refreshed snapshot.

That makes a snapshot timestamp part of the Green Grocer boundary.

This is the preferred first path to investigate: ask for an Epicor-supported,
customer-facing Compass/report/data feed, ideally with both a full baseline and
incremental inventory/price changes. Do not freeze a parser until an actual
sample file from the store/Epicor partner has been obtained.

### 3. Eagle report/export files

Epicor's current inventory page says Eagle inventory can be exported in
spreadsheet, text, HTML, and PDF formats. Even if the store cannot immediately
provision an automated feed, a text/spreadsheet export is useful for the first
schema fixture and mapping test.

This is not enough for a useful production stock display unless refreshed
automatically.

### 4. Transoft ODBC / DataPump

Epicor's current third-party terms name both `TRANSOFT DATAPUMP GATEWAY` and
`TRANSOFT ODBC`. They are real Eagle data-extraction products.

However, the same terms say that, unless Epicor specifically authorizes it,
the licensee may not use them to provide Eagle-derived data for use by others,
explicitly including Internet-based catalog/retail/wholesale/distribution
applications.

Green Grocer is exactly the kind of customer-facing application for which that
restriction matters. Do not choose this path without explicit authorization
covering the intended use.

### 5. Direct database scraping or an imagined REST API

No.

This research did not find a public Eagle developer contract establishing a
stable REST/OpenAPI endpoint for these records. Direct database coupling would
also bind Green Grocer to undocumented Eagle internals and could run into the
same product/licensing boundary as extraction tools.

Do not write code around guessed table names, guessed HTTP endpoints, or
screen-scraping Eagle.

## Proposed source types

The following are semantic pseudotypes, not proposed final Idriç syntax.
They deliberately separate Eagle's source ontology from Green Grocer's simple
shopper ontology.

```text
eagle_source
    identity        source_identity

eagle_store
    source          source_identity
    identity        eagle_store_identity

eagle_item_identity
    source          source_identity
    sku             eagle_sku

eagle_item
    identity        eagle_item_identity
    name            Text
    pricing_unit    Text
    upcs            list eagle_upc
    primary_upc     maybe eagle_upc
    manufacturer_part maybe Text
    web_flag        maybe Text
    pos_sellable    maybe Bool
    discontinued    maybe Bool
    decimal_quantity_allowed maybe Bool

eagle_store_stock
    store           eagle_store_identity
    item            eagle_item_identity
    display_price   money
    available       eagle_quantity
    quantity_on_hand maybe eagle_quantity
    quantity_on_order maybe eagle_quantity
    locations       list eagle_location

inventory_snapshot
    observed_at     time
    source_time     maybe time
    items           list (eagle_item, eagle_store_stock)
```

Why the source flags are `maybe`: the normalized adapter should be able to work
from the smallest authorized feed the store can actually provide. We should not
pretend a particular flat-file export contains every Inventory Maintenance
field until we have a real sample.

`display_price` means the one customer-facing price the feed/store has resolved
for the selected storefront. Eagle itself has several price concepts (retail,
promotion, list, loyalty, and others). Green Grocer should not reimplement
Eagle pricing policy from raw fields unless that eventually becomes an explicit
requirement.

`observed_at` is when Green Grocer ingested the snapshot. `source_time` exists
only when the feed itself provides a trustworthy generation timestamp. The UI
may later use this to suppress or label stale inventory.

## Boundary presented to the shopper application

The customer application needs much less Eagle vocabulary:

```text
shopper_stock
    product         product_identity
    store           store_identity
    availability    stock_status
    locations       list shelf_location
    observed_at     time

stock_status
    out_of_stock
    in_stock exact_positive_amount
```

The exact representation of `exact_positive_amount` remains open because Eagle
supports decimal quantities while the present mock uses integer `Number`.
That is a real type-system boundary, not a reason to push Eagle's raw numeric
representation into the rest of Green Grocer.

For a single selected store, the current mock `product` can be projected from
Eagle data approximately as:

```text
product.identity <- eagle_item_identity
product.name     <- eagle_item.name
product.price    <- eagle_store_stock.display_price
product.unit     <- eagle_item.pricing_unit
product.image    <- Green Grocer image enrichment
```

and availability remains adjacent data:

```text
shopper_stock.product      <- product.identity
shopper_stock.store        <- selected store
shopper_stock.availability <- normalize Eagle Available
shopper_stock.locations    <- Eagle location codes
```

Images should not block the inventory adapter. Green Grocer can continue to
resolve images from its own assets or later enrichment keyed by SKU/UPC.

The cart can remain conceptually separate from inventory. Adding something to a
cart does not make Green Grocer the inventory system of record, and this first
integration does not reserve or decrement Eagle inventory.

## Consequences for the existing `cart-types.md`

Do **not** rewrite the existing mock cart types merely to anticipate Eagle.
They were intentionally minimal for the local rendering benchmark.

The first real Eagle sample will, however, give evidence for two later changes:

1. `product_identity` can acquire a production implementation backed by
   `(source, SKU)` rather than a mock-local token.
2. If the real store has decimal-sale SKUs that Green Grocer intends to cart,
   cart amounts will need an exact positive quantity type broader than integer
   `Number`.

Until then, those are integration findings rather than reasons to complicate
the existing mock.

## First implementation target

The next engineering pass should remain read-only and small:

1. Obtain one **authorized sample Eagle product/inventory feed** from the store
   or its Epicor representative/integration provider. A Compass/data-feed text
   or CSV-like export is preferable; an ordinary text/spreadsheet export is
   enough for the first frozen fixture.
2. Record the exact Eagle release and which integration modules the store
   already has (especially Compass and/or iNet/eCommerce).
3. Confirm that the feed is licensed for a public/customer-facing catalog.
4. Ask for these fields where available:
   - store identity;
   - SKU;
   - description;
   - intended Web/online visibility;
   - POS sellable/discontinued state;
   - resolved customer/display price;
   - Eagle `Available` quantity;
   - QOH for diagnosis;
   - pricing/selling unit;
   - UPCs/primary UPC;
   - manufacturer part number;
   - locations 1-6;
   - source/change timestamp if the feed provides one.
5. Check in a scrubbed/frozen sample and a field-map document before writing a
   production parser.
6. Parse it into `inventory_snapshot` and prove the normalization rules with
   fixtures for:
   - positive stock;
   - zero stock;
   - negative QOH;
   - multiple locations with one aggregate store amount;
   - several UPCs for one SKU;
   - a decimal quantity;
   - a hidden/non-Web or non-sellable item;
   - stale snapshot handling.
7. Feed the resulting shopper projection into the existing Green Grocer product
   list. No Eagle writes, checkout, reservations, owner UI, or customer account
   system belong in this pass.

## Facts still unknown about the actual store

Research cannot determine these from public documentation:

- whether the store is definitely on Eagle rather than Propello or another
  Epicor product;
- the Eagle release;
- whether it uses single-store or multistore Eagle;
- whether Compass is installed/configured;
- whether iNet/eCommerce is licensed;
- what product/inventory report or feed the store already sends elsewhere;
- the actual location-code conventions (`A12`, `PLUMBING`, etc.);
- which Eagle price should be the Green Grocer display price;
- whether the store wants exact counts exposed publicly or only in/out-of-stock;
- whether its inventory includes decimal-sale items that Green Grocer should
  support;
- the authorized external-update cadence.

Those questions should be answered by one store/Epicor sample and provisioning
conversation, not by adding speculative types.

## Sources

### Epicor

- [Epicor Eagle](https://www.epicor.com/en-us/products/retail-management-systems-rms/eagle/)
  — current overview; real-time inventory visibility, e-commerce inventory sync,
  reporting/export capability.
- [Eagle Point of Sale](https://www.epicor.com/en-us/products/retail-management-systems-rms/eagle/point-of-sale-pos/)
  — says POS automatically updates inventory data.
- [Eagle Inventory Management](https://www.epicor.com/en-us/products/retail-management-systems-rms/eagle/inventory-management/)
  — real-time inventory, multistore inventory, customer availability/location
  lookup, exports to spreadsheet/text/HTML/PDF.
- [Eagle eCommerce](https://www.epicor.com/en-us/products/retail-management-systems-rms/eagle/ecommerce/)
  — Eagle/iNet storefront, synchronized inventory and product information.
- [Epicor third-party terms](https://www.epicor.com/en/company/legal/third-party-terms/)
  — Transoft DataPump/ODBC extraction terms and restriction on Internet-facing
  use without specific authorization.
- [Qty On Hand](https://help.eaglesoa.com/30/en-w-eagle/Inventory/Reference/Inv_Maint_Field_Help/2Pricing/QOH.htm)
  — store-specific QOH, POS decrement, zero and negative semantics.
- [Inventory Maintenance field help](https://help.eaglesoa.com/34/en-n-eagle/Inventory/Reference/Inv_Maint_Field_Help/Inv_Maintenance_Field_Help.htm)
  — inventory field inventory including SKU/store/UPC/pricing/codes.
- [Entering UPCs](https://help.eaglesoa.com/25/en-w-eagle/Inventory/Loading_Inv/Entering_UPCs.htm)
  — multiple UPCs, optional primary UPC, multistore UPC-to-SKU behavior.
- [Committed Quantity: Current and Future](https://help.eaglesoa.com/30/en-w-eagle/Cust_Ord_Mgmt/Reference/Committed_Quantity__Current_and_Future.htm)
  — configuration-dependent committed quantity used by Eagle's available
  calculation.
- [Eagle Mobile Inventory: Using](https://help.eaglesoa.com/25/en-n-auto/Eagle_Mobile/E_Mobile_Inv/EMobl_Inv_Usg.htm)
  — identifies a customer-safe price-check field set including Available, QOO,
  Location, pricing, and manufacturer part.
- [Adding Items to the Count File Manually](https://help.eaglesoa.com/25/en-n-auto/Inventory/Phys_Inv_Shrinkg/Post_Counts_Shrinkg/Add_2_PIP_Man.htm)
  — up to six locations/counts but one aggregated QOH.
- [Decimal quantity allowed](https://help.eaglesoa.com/27/en-w-eagle/Inventory/Reference/Inv_Maint_Field_Help/3Codes/Decim_qty_allowd.htm)
  — per-item decimal sale support.
- [Eagle release 28 location enhancement](https://help.eaglesoa.com/28/en-w-eagle/Web_Only/Beta_Level_28.htm)
  — six item location codes.

### Deployed third-party Eagle integration evidence

These are not the authority for Eagle semantics, but they are useful evidence of
how a current commercial Eagle-to-web integration is actually provisioned.

- [Modern Retail: Epicor Eagle Website Integration Overview](https://support.modernretail.com/hc/en-us/articles/204796147-Epicor-Eagle-Website-Integration-Overview)
  — Epicor product files -> dedicated FTP -> middleware -> e-commerce platform.
- [Modern Retail: Epicor Compass](https://support.modernretail.com/hc/en-us/articles/360055918054-Epicor-Compass)
  — Compass used for frequent inventory/pricing updates.
- [Modern Retail: Upload & Update Frequency](https://support.modernretail.com/hc/en-us/articles/360057019894-Upload-Update-Frequency)
  — periodic polling rather than an assumed webhook stream.
- [Modern Retail: Eagle and Orgill File Upload Frequency](https://support.modernretail.com/hc/en-us/articles/49029930536595-Epicor-Eagle-and-Orgill-File-Upload-Frequency)
  — example of full files plus more frequent inventory/price change files.
- [Modern Retail: Location-Based Inventory](https://support.modernretail.com/hc/en-us/articles/213599127-Epicor-Eagle-Location-Based-Inventory)
  — example of store-location inventory being sent to a website.
