# Minimal Green Grocer cart types

## Decision

Green Grocer needs one displayed-and-cartable `product` concept and one sparse
`cart`. It does not presently need the commerce distinctions found in mature
store systems.

The smallest coherent shape is:

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

Here `⇀` describes a finite partial mapping, not proposed Idriç syntax. A cart
stores only products whose amount is nonzero. The absent coordinate is zero.

### Numeric vocabulary

The job text calls `Number` nonnegative, but the later Idriç decision is:

\[
Number = 1, 2, 3, \ldots
\]

and `±Number` contains negative values, zero, and positive values. This design
follows that later decision without inventing another numeric hierarchy. A
stored cart amount is a `Number`; zero is represented by absence from the
sparse cart. `±Number` is not needed.

## Proposed semantic types

| Type | Meaning and justification |
|---|---|
| `product` | The fake thing shown to the shopper and the same thing added to the cart. The mock has no requirement that would separate a descriptive product from a sellable offer. |
| `product_identity` | A stable, mock-local identity used as the cart key. It prevents two products with the same visible data from collapsing together and lets name, price, unit, or image change without creating a different cart coordinate. It is not declared to be a SKU, GTIN, PLU, barcode, database key, or Android resource number. |
| `money` | An exact monetary value supporting zero, addition, and multiplication by `Number`. It prevents prices and totals from becoming unlabelled numbers or binary floating-point values. The mock uses one dollar display convention, so `money` carries no currency or market machinery. |
| `product_image` | An opaque logical reference to a fake product image. The renderer resolves it to its own asset representation; paths, decoded pixels, Android resource integers, and image-loading machinery stay below this boundary. |
| `cart` | A finite sparse mapping from `product_identity` to positive amount. It owns only the shopper's current quantities; product facts and all totals remain elsewhere or are derived. |

`name` and `unit` remain `Text`. The name is displayed text. The unit is also
presentation-facing text supplied by the fake seller, such as `bunch`, `lb`,
or `box`; the mock neither converts units nor proves dimensional laws.

`price : money` belongs on `product` for this mock. There is one present fake
price and no price lifecycle. Moving it into an offer, quote, or cart snapshot
would model rules that the mock does not exercise.

## Fundamental values and rendering projections

The fundamental values are:

- the five fields of each fake `product`;
- the positive product amounts in `cart`.

Everything below is calculated when needed:

- a cart row containing a resolved product, its amount, and its line total;
- formatted price text such as `$4.99 / lb`;
- a line total;
- the cart total;
- a cart badge showing the number of occupied product coordinates;
- row order and the currently visible scroll slice;
- decoded or platform-specific image data.

A renderer may name its row projection `cart_row` if that makes its code
clearer. That does not make `cart_row` or `Cart.Line` a fundamental cart type.
There is no line-specific identity, price, instruction, option, or other fact
in the mock: one nonzero product coordinate already contains the complete cart
meaning.

## Required operations

The semantic cart transitions are:

```text
empty_cart
add product to cart
change amount of product in cart to amount
remove product from cart
```

- `add product to cart` creates an amount of one or increases the existing
  amount by one seller-defined unit.
- `change amount of product in cart to amount` replaces a coordinate with a
  positive `Number`.
- `remove product from cart` deletes the coordinate. Decreasing one to zero
  invokes removal; zero is not retained as a cart entry.

The semantic observation needed by presentation is enumeration of the cart's
nonzero `(product_identity, amount)` coordinates. The presentation layer then
resolves identities against `fake_products : List product` and derives:

```text
cart rows from fake_products and cart
line total from product and amount
cart total from fake_products and cart
```

“Visible cart entries” therefore crosses two concerns: enumerating coordinates
is a cart observation, while resolving products, choosing row order, and
selecting the visible scroll slice are rendering work. No routing, event,
command, result, repository, or global-store type is required to express these
pure in-memory transitions.

## Concrete fixture

| identity | name | price | unit | image | cart amount | line total |
|---|---|---:|---|---|---:|---:|
| `bananas` | Bananas | $1.29 | bunch | `bananas_image` | 1 | $1.29 |
| `gala_apples` | Gala apples | $4.99 | lb | `gala_apples_image` | 2 | $9.98 |
| `corn_flakes` | Corn flakes | $3.79 | box | `corn_flakes_image` | 3 | $11.37 |
| `jasmine_rice` | Jasmine rice | $6.49 | bag | `jasmine_rice_image` | — | — |

The three nonzero coordinates give a derived cart total of **$22.64**. Jasmine
rice demonstrates that a product can be displayed without occupying a zero
line in the cart.

These values are fixtures for scrolling, image display, quantity changes,
totals, and rerendering. They are not assertions about real store inventory or
pricing.

## Mathematical interpretation

Let `P` be the finite set of fake product identities and let `S` be the finite
support of a cart. The stored cart is:

\[
C : S \to Number, \qquad S \subseteq P.
\]

Its zero-extension is a function

\[
\bar C : P \to \{0\} \cup Number
\]

whose value is zero exactly when a product is absent from the cart. If
`price(p)` is the present `money` value on the product, then

\[
\operatorname{total}(C)
  = \sum_{p \in S} price(p)\,C(p).
\]

This is a finite weighted sum, not a claim that carts form a vector space.
Cart coordinates have no additive inverses, the seller-defined units can
differ between products, and no scalar-field structure is being proposed.

## Rejected apparent types

- `offer`, `variant`, `sellable`, and `catalog_item`: no mock behavior
  distinguishes any of them from `product`.
- `catalog`: `fake_products : List product` is enough.
- `cart_line` or `line_identity`: the one-coordinate-per-product rule leaves no
  independent line fact to represent.
- `quantity`, `cart_amount`, or a numeric refinement hierarchy: `Number` is
  already the positive amount stored at each sparse coordinate.
- `unit`: `Text` is sufficient until unit-specific operations or laws exist.
- `price`, `displayed_price`, `confirmed_price`, and `price_quote`: the field
  name `price` gives a `money` value its role; the mock has no price lifecycle.
- `currency`, `market`, conversion, or rounding types: the fixture has one
  dollar display convention and performs no such operations.
- SKU, GTIN, UPC, EAN, PLU, or barcode types: none participates in this mock.
  `product_identity` deliberately makes no claim to those meanings.
- `order`, `checkout`, `payment`, and fulfillment types: the mock ends at the
  cart.
- cart event, command, failure, persistence, session, and transport types: the
  local deterministic mock does not need them to change state and rerender.

## Deliberately not modeled

- inventory management, availability, or reservations;
- fulfillment, substitutions, requested-versus-measured weight, or seller
  rounding;
- checkout, orders, payment, tax, or SNAP handling;
- promotions, discounts, seller policies, or seller/admin operations;
- price changes, notifications, stale-price detection, price quotes, or price
  snapshots;
- SKU, GTIN, UPC, EAN, PLU, barcode, product-variant, or catalog-entry
  ontology;
- universal units, dimensional analysis, unit conversion, or fractional-weight
  entry;
- persistence, networking, authentication, synchronization, concurrency, or
  session ownership;
- Android resource identifiers, file paths, bitmap formats, decoding, caching,
  or image-loading policy;
- production validation and failure taxonomies.

## Unresolved questions

No unresolved question prevents this type decision or the rendering benchmark.
An actual store-owner conversation or a concrete mock interaction may later
require a distinction listed above. That would be evidence for adding the
distinction then, not a reason to add it now.
