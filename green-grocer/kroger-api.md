# Kroger public API notes

Status: initial documentation pass, 2026-09-26.

This file records the API surface relevant to Green Grocer. Keep facts that are
confirmed by Kroger's developer material separate from things we still need to
exercise against a real developer application.

## Immediate conclusion

The self-service Public APIs are enough for a useful first Kroger integration:

1. locate a Kroger store;
2. search its catalog;
3. inspect a product by Kroger product ID or UPC;
4. obtain store-specific price, availability, and aisle/shelf information when
   a location is supplied;
5. authenticate a Kroger customer;
6. add one or more UPCs to that customer's real Kroger cart.

The public Cart API is deliberately narrow: it documents adding items to the
authenticated customer's cart. Kroger's separate Partner Carts API exposes the
larger create/read/update/delete cart surface. Treat checkout, payment, pickup
slot selection, and final order submission as **not available through the
self-service public API unless we later find explicit Kroger documentation
saying otherwise**.

That still gives us the useful path:

```text
Green Grocer UI
    -> choose store
    -> search Kroger products
    -> choose product / quantity
    -> add UPC to Kroger cart
    -> hand off to Kroger for checkout
```

Do not design around scraping the Kroger Android app unless the public API
proves inadequate.

## Canonical service roots

Production API:

```text
https://api.kroger.com/v1/
```

Developer portal:

```text
https://developer.kroger.com/
```

Kroger also publishes an official public Postman workspace. It is useful because
the developer portal is heavily client-rendered and some reference pages are
hard to read without a browser:

```text
https://www.postman.com/kroger/the-kroger-co-s-public-workspace/
```

Kroger public collection:

```text
https://www.postman.com/kroger/the-kroger-co-s-public-workspace/collection/ki6utqb/kroger-public-apis
```

## Registration and authentication

Using Public APIs requires:

1. create a Kroger developer account;
2. register an application;
3. receive OAuth 2 client credentials.

Kroger's official public collection documents two relevant OAuth uses:

- application/client authentication for public catalog/location work;
- OAuth 2 Authorization Code authentication when a request acts on a shopper's
  account.

The public `POST /cart/add` endpoint explicitly requires an authenticated
customer using the OAuth 2 Authorization Code grant.

Before implementing auth, verify and record:

- authorization endpoint;
- token endpoint;
- redirect URI rules;
- exact public scope strings;
- access-token lifetime;
- refresh-token behavior;
- whether Android custom URI schemes are accepted or HTTPS app links are
  required.

Do not guess those values into code.

## Locations API

Purpose: find Kroger-owned stores, chains, and departments.

Confirmed public rate limit from Kroger's official Postman collection:

```text
1,600 requests/day per endpoint
```

### Find stores near a ZIP code

```http
GET /v1/locations?filter.zipCode.near=45202&filter.limit=200
Authorization: Bearer <token>
```

The response includes location records containing fields such as the store name
and street address. Capture the returned `locationId`; product requests use it
to obtain store-specific data.

### Filter by departments

Kroger's knowledge-check example:

```http
GET /v1/departments
GET /v1/locations?filter.zipCode.near=45202&filter.department=10,09&filter.limit=200
```

This lets us find stores with required departments without hard-coding
department names or IDs.

Other documented location-family operations include:

```text
GET  /v1/locations/{locationId}
HEAD /v1/locations/{locationId}

GET  /v1/chains
HEAD /v1/chains/{name}

GET  /v1/departments
HEAD /v1/departments/{id}
```

We do not need most of these for the first app slice.

## Products API

### Search products

Confirmed useful filters from Kroger's public examples include:

```text
filter.term
filter.locationId
filter.brand
filter.fulfillment
filter.limit
```

Example:

```http
GET /v1/products?filter.term=milk&filter.locationId=01400751&filter.brand=Kroger&filter.fulfillment=sth
Authorization: Bearer <token>
```

For our app, `filter.locationId` should normally be present. Kroger documents
that location-scoped product requests are what provide store-specific price,
availability, and aisle information.

Fulfillment values need their own verified table before code depends on them.
The official Kroger example uses:

```text
sth = ship to home
```

Do not infer the spelling of pickup or delivery modalities from that one value.

### Product details

```http
GET /v1/products/{productId-or-UPC}?filter.locationId={locationId}
Authorization: Bearer <token>
```

Kroger explicitly documents that the path identifier may be either its
`productId` or a UPC.

With a location supplied, the response can include aisle-location information.
The documented aisle structure includes fields such as:

```text
number
shelfNumber
bayNumber
description
sequenceNumber
side
numberOfFacings
shelfPositionInBay
```

This is useful beyond ordering: Green Grocer can tell the user where the item is
in the physical store.

### Identifier mapping

Keep the existing Green Grocer distinction between our internal product identity
and standardized/store identities.

For Kroger-backed products, record at least:

```text
Green Grocer product identity
Kroger productId
UPC
Kroger locationId associated with price/availability observation
```

Do not collapse Kroger `productId` and UPC into one type merely because the
details endpoint accepts either.

## Public Cart API

Confirmed public endpoint:

```http
POST /v1/cart/add
Authorization: Bearer <customer authorization-code token>
Content-Type: application/json
```

Example body from Kroger:

```json
{
  "items": [
    {
      "quantity": 1,
      "upc": "0001111040101",
      "modality": "<string>"
    }
  ]
}
```

Successful example response:

```text
204 No Content
```

Confirmed public rate limit:

```text
5,000 requests/day
```

### Important semantic difference from our current cart

The current Green Grocer cart is a local sparse map:

```text
product_identity -> positive amount
```

Kroger's public cart write uses:

```text
UPC + quantity + modality
```

Do not replace the local cart model with Kroger's transport object. The adapter
should translate only at the integration boundary.

A reasonable first boundary is:

```text
kroger_cart_item
    upc       Text
    quantity  Number
    modality  kroger_modality
```

where `kroger_modality` remains unimplemented until we verify Kroger's exact
accepted values.

## Public versus Partner cart surface

Public:

```text
POST /v1/cart/add
```

Kroger Partner documentation additionally exposes operations such as:

```text
GET    /v1/carts
POST   /v1/carts
GET    /v1/carts/{id}
POST   /v1/carts/{id}/items
PUT    /v1/carts/{id}/items/{upc}
DELETE /v1/carts/{id}/items/{upc}
```

Partner APIs require additional security review and a contractual relationship
with Kroger; they are not available through self-service application
registration.

Therefore the first Green Grocer Kroger integration should not pretend it owns
or can fully synchronize the Kroger cart. It can maintain its own local cart,
then append selected lines into the shopper's Kroger cart.

## Product boundary: catalog/cart pipe, not Kroger merchandising

Green Grocer should not attempt to reproduce the full Kroger shopping
experience merely because some related data or behavior may exist.

The default Kroger adapter is intentionally narrow:

```text
use Kroger for:
    store identity
    product identity
    factual product data
    store-specific price
    availability
    aisle/shelf location
    fulfillment facts needed to add the chosen item
    adding an explicitly chosen item to the customer's Kroger cart
```

Do **not** make the following part of the core integration:

```text
sponsored placements
advertising
recommendation carousels
cross-sells / upsells
"you may also like"
campaign-driven specials feeds
automatic coupon promotion
loyalty-program nudges
cart-abandonment prompts
checkout/payment
pickup-slot selection
order submission
Kroger-style engagement notifications
```

Some distinctions matter:

- If Kroger returns a sale or loyalty price as a factual property of the product
  the user is already looking at, Green Grocer may display it. That is different
  from building a "Specials" destination whose purpose is merchandising.
- A coupon can be useful to the shopper, but coupon discovery/application is a
  separate optional feature. Do not make it a dependency of ordinary product
  search or cart use.
- A user-defined recurring-item system belongs to Green Grocer. Do not confuse
  it with Kroger recommendations, sponsored reorder prompts, or retailer
  engagement campaigns.
- The Kroger cart is an external handoff target. Until we deliberately decide
  otherwise and have a supported API, Green Grocer does not need to mirror,
  police, optimize, or check out that cart.

This is both a product choice and an integration boundary. Kroger may reserve
some merchandising, loyalty, checkout, or order-management capabilities for
its own applications or Partner APIs; Green Grocer should not depend on them
unless we have both a clear user need and explicit supported access.

## First implementation slice

Keep the first executable slice narrow:

```text
1. developer credentials from local configuration
2. obtain application OAuth token
3. search locations by ZIP
4. choose/store one locationId
5. search products at that location
6. render name + price + availability + UPC/productId
7. authenticate customer with Authorization Code
8. POST selected UPC + quantity to /cart/add
9. verify the same item appears in the real Kroger cart
```

The physical acceptance test is step 9. HTTP 204 alone is not enough.

Do not implement checkout automation in this slice.

## Open questions to verify experimentally

- exact OAuth endpoints and scope strings;
- token and refresh-token lifetimes;
- Android redirect-URI behavior;
- complete Product public rate limits;
- exact fulfillment values;
- exact cart `modality` values;
- whether repeated `/cart/add` calls add to an existing quantity or produce
  some other cart behavior;
- maximum number of `items` accepted in one `/cart/add` request;
- UPC formatting expectations, especially leading zeroes;
- response headers exposing rate-limit state;
- whether product search price fields distinguish regular, promo, and loyalty
  prices;
- whether digital coupons are exposed anywhere in the self-service public API;
- whether pickup-slot data exists in any public API;
- whether any public handoff/deep link can open Kroger directly at checkout.

## Sources

Primary/current Kroger material:

- Kroger Developers: <https://developer.kroger.com/>
- Kroger Public APIs collection:
  <https://www.postman.com/kroger/the-kroger-co-s-public-workspace/collection/ki6utqb/kroger-public-apis>
- Public add-to-cart request:
  <https://www.postman.com/kroger/the-kroger-co-s-public-workspace/request/mwiie4o/add-to-cart>
- Public product-details request:
  <https://www.postman.com/kroger/the-kroger-co-s-public-workspace/request/ayf9yld/product-details>
- Public Locations folder:
  <https://www.postman.com/kroger/the-kroger-co-s-public-workspace/folder/adkttgg/locations>
- Kroger knowledge-check examples:
  <https://www.postman.com/kroger/the-kroger-co-s-public-workspace/documentation/qg005u4/kroger-knowledge-check-answers>
- Partner API documentation, used only to mark the public/partner boundary:
  <https://www.postman.com/kroger/the-kroger-co-s-public-workspace/documentation/nryx3kn/kroger-partner-apis>

Secondary index, useful for enumerating Kroger's developer products but not a
substitute for Kroger's own documentation:

- API Evangelist Kroger index:
  <https://github.com/api-evangelist/kroger>
