# Green Grocer references

## POS and inventory integration

- [Epicor Eagle inventory integration research](./epicor-eagle-inventory.md)
  - Research-first boundary for read-only customer inventory.
  - Covers supported/in-use Eagle extraction paths, SKU/UPC identity, store stock,
    availability, physical locations, decimal quantities, freshness, licensing,
    and the proposed adapter types.

## Catalog and fixture sources

Grocery-store fixture and seed-data sources worth mining before inventing our own catalog.

- [Open-Science-Online-Grocery/online-grocery](https://github.com/Open-Science-Online-Grocery/online-grocery)
  - Rails simulated grocery store with a large seeded product catalog.
  - Product seed: <https://github.com/Open-Science-Online-Grocery/online-grocery/blob/master/db/seeds/base/products.csv>
  - Best first place to inspect for realistic mock grocery inventory.

- [esther-ng/groceries](https://github.com/esther-ng/groceries)
  - Rails grocery application with seed data derived from QFC and Safeway.
  - Useful for fixtures modeled after ordinary U.S. supermarket inventory.

- [Glovo/Hackathon-AI-Summit-2025](https://github.com/Glovo/Hackathon-AI-Summit-2025)
  - Mock grocery/retail ordering API with checked-in product/store/order data and data-generation code.
  - Useful for small synthetic SKU-style fixture sets and API examples.

- [EventideSystems/brocade.io](https://github.com/EventideSystems/brocade.io)
  - Rails/Postgres barcode-product project with seed data derived from Datakick.
  - Useful for UPC/GTIN/barcode fixtures. Repository is archived, so treat it as a data/reference source rather than an active dependency.

## Identifier distinction

Keep store SKU and barcode identity separate in Green Grocer fixtures:

- **SKU**: store-local/internal identifier; Green Grocer can define these itself.
- **UPC/EAN/GTIN**: standardized product/barcode identity; use real-format fixture data where useful.

A likely fixture strategy is to take a realistic grocery catalog from the Rails grocery sources, assign Green Grocer SKUs, and attach UPC/GTIN values only where barcode-oriented fixtures are needed.
