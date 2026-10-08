# STAR G0 — Green Grocer decision

Reviewed 2026-10-07 (America/Detroit). **Recommendation: build a small offline
native cart specimen with a pure C semantic core, one coherent screen, exact
cents, and paired A1/C67 builds.** Keep an independent Idriç experiment. Do not
make inventory, Eagle, a framework, or an application generator prerequisites.

This is a decision and handoff, not implementation or execution evidence.

## 1. What Green Grocer was

### Verified source state

The remote `green-grocer` head still matched
`039961365c9a2ebe3af8d5668d90106141cf186a` at review. Its Green Grocer subtree
contains exactly the five assigned Markdown files and no application. Current
`main` was `ec022aea6fe6836ea78f479b498e85b3452d88b7`; the branches share
`1516f23e4cf975b3f33a10c8237ddbaccb9c9f3f`. The seven Green Grocer commits have
not become main's application. No merge or wholesale rebase is needed for this
review.

The history distinguishes successive decisions:

| Evidence | What it establishes |
| --- | --- |
| [Fixture references](https://github.com/Ashtray-Archer/utilities-android-phone-user/blob/a75bbbb26b81c677ed965aea42bd2759f97218a7/green-grocer/references.md) | Initial interest in realistic grocery examples and identifier distinctions. |
| [Framework archaeology](framework-archaeology.md), introduced at `66652dc1f8a67081ca4b639ee095fb62b114f245` | Explicitly provisional research into shopper operations and state ownership, with no chosen language, framework, server, or deployment. |
| [Cart types](cart-types.md), `955fffc673386254e7d78a70985e48fbedd4d325` | Deliberately settles one displayed/cartable product and a sparse cart; declines mature-commerce distinctions. |
| [Eagle research](epicor-eagle-inventory.md) and [customer inventory](customer-inventory-types.md), ending at the reviewed head | Later read-only inventory extension; explicitly preserves the fixture cart and lacks an authorized source feed. |
| [Carpenter Brothers draft](https://github.com/Ashtray-Archer/utilities-android-phone-user/tree/a2885c2ee032dfe4cc23bcb987a5ff22942c6efd/green-grocer/carpenter-brothers-hardware-store) | A real HTML/JavaScript specimen exists on a descendant branch: nine synthetic hardware products, search, locations, availability, and cart controls. Source inspected; not run in this review. |
| [Kroger notes](https://github.com/Ashtray-Archer/utilities-android-phone-user/blob/a15f215e86a23b46dcfb8849425c2c191f7d06e4/green-grocer/kroger-api.md) | A later direction adds factual retailer data, user-owned recurrence, and an explicit external-cart handoff, while rejecting merchandising-driven shopping. Documentation, not a demonstrated integration. |

[Ashtray-Archer/utilities-android-phone-user PR #41, “Carpenter Brothers hardware store”](https://github.com/Ashtray-Archer/utilities-android-phone-user/pull/41)
was closed and draft at inspection, targeting `green-grocer`. Its branch still
exists. Preserve it as evidence; do not reopen or merge it as part of G0.
`corner-store` and `neighborhood-hardware-store` still point to the shared
ancestor, not additional implementations.

### Motives, separated

| Motive | Historical role; decision now |
| --- | --- |
| Useful grocery/store application | Genuine direction, strengthened by the hardware and Kroger branches. A fixture app alone does not satisfy real shopping needs. |
| Deliberately small buyer/cart application | Central and the best immediate purpose: small enough to understand completely. |
| Framework study | Central preparatory work, now sufficiently answered. Stop the comparison exercise. |
| Idriç architecture and typed transitions | The notes use Idriç vocabulary but contain no Green Grocer Idriç implementation. Preserve as a precise companion experiment, not a claimed historical executable. |
| Native Android rendering/input | Cart notes call this a rendering benchmark; neighboring utilities supply native precedents. Make it an explicit goal now, rather than treating a language choice as already settled. |
| Inventory/POS and Eagle | Later extension, motivated by customer stock/location questions. Useful boundary research, not the first cart's foundation. |
| Eventual real-store front end | Concrete in Carpenter Brothers; store inventory remains synthetic and the source system unconfirmed in the research. Keep this possible destination separate. |
| Reusable application generation | A plausible later use of a small specimen. No inspected Green Grocer artifact establishes a generator contract or implemented factory. Do not invent an original commitment. |

## 2. What remains valuable

Keep the sparse cart, stable local product identities, exact-money requirement,
and derived totals. One product is enough when the displayed package is exactly
what the shopper selects. No behavior currently requires a separate descriptive
product, offer, variant, or transaction line.

Keep four lessons from the archaeology: operations have meaning independently
of input handlers; cart lifetime differs from window lifetime; displayed values
derive from authoritative state; incomplete editor text must not become a
quantity. The proposed button-only first slice avoids that last problem rather
than building a form system to demonstrate it.

Retain all five research files as historical evidence. Keep the inventory
boundary's distinction between unknown and zero availability, its separation
from cart state, and its refusal to invent shelf-level counts. These are useful
for a later store project. Keep the Kroger principle that retailer data should
serve the shopper rather than introduce advertising and engagement machinery.

The hardware draft proves that a single catalog/cart surface can express the
interaction in source. It does **not** establish the stronger semantic contract:
it converts price strings with JavaScript `Number`, uses floating-point totals,
silently skips unresolved cart identities, and gates adding on availability in
the view while cart increments do not enforce a corresponding stock limit.
These are reasons to retain it as a sketch rather than adopt it as the oracle.

## 3. What should be discarded or deferred

Discard these requirements for the next slice, without deleting their history:

- The archaeology's “research more commerce systems before settling types”
  sequence. The later cart decision already superseded much of that agenda.
- A prescribed browse/detail/cart/navigation screen hierarchy. Product details
  can be visible in the row; returning to browse need not be a route transition.
- The claim that naming an exact `money` type settles its representation.
  The next implementation uses integer cents and checked arithmetic.
- The assumption that Idriç `Number` alone guarantees positivity. The current
  compiler checkpoint says nonnegative; the September note says positive.
  State the invariant independently and verify it in the companion experiment.
- Bare `lb` as a fixture unit implying arbitrary weighed produce. Use a named
  prepacked `1 lb bag` if quantities count bags. Fractional quantities belong
  to a later, deliberately selected interaction.

**Eagle's role is a later adapter exercise.** Its immediate research value has
been extracted. No further Eagle investigation, parser, or provisioning work
belongs on the path to this executable. Reactivate only for an actual requested
store deployment with an authorized sample and agreed customer policy. The old
technical/licensing research is dated evidence, not fresh vendor advice.

Defer `customer_item`, `customer_catalog`, `product_availability`, and
`shelf_location` from the executable. They are coherent for “does this store
have it, and where?”, but that is a different question from a local cart.
Even the adapter's `(source, SKU)` identity rule remains conditional on the
actual feed's uniqueness scope; the old document does not prove it universally.

Exclude checkout, payment, orders, tax, delivery, fulfillment, reservations,
inventory writeback, seller UI, accounts/authentication, real-time inventory,
backend service, database, and cloud sync because this specimen neither makes a
purchase nor shares durable state. Exclude recommendations/promotions because
they add no evidence about the chosen interaction. Defer Kroger integration and
recurring-item planning as a separate useful-product direction. Exclude an app
factory or general commerce framework until another application demonstrates
a concrete shared requirement.

## 4. Current project thesis

Green Grocer should now exist in order to demonstrate an ordinary, understandable
interactive application whose entire meaning can be inspected independently of
its screen: choose a few products, change their amounts, remove them, and see
the exact consequences. Its value is the complete path from a finger action to
a semantic transition to an updated display on the user's phones.

This first executable is an honest offline specimen with sample prices, not a
pretend store service. It should provide a small reference program for later
Idriç and application-generation work without requiring either to mature first.
The useful-store direction remains available; it does not get to enlarge this
first acceptance boundary by implication.

## 5. Recommended first executable slice

One scrollable list of eight bundled products. Each row shows the full name,
package/unit, price, a small local image or placeholder, current amount, `+`,
`−`, and a remove control when selected. No separate detail screen: the fixture
must fit its meaningful information in the row, wrapping rather than truncating.

A persistent footer shows the number of distinct selected products and the
sample merchandise total, including `$0.00` when empty. A single “Selected
only” toggle filters the same list. Turning it off returns to all products
without changing the cart. No search box is needed for eight products.

The user can add from zero, add repeatedly, decrement one to absence, remove
an amount greater than one, review only selected rows, and resume browsing.
Scrolling must never activate a quantity button. An empty filtered list must
still expose the toggle. Label the catalog as sample data and the cart as
session-only; prices remain fixed for the session. Fresh process launch starts
empty. Window/surface recreation and background/foreground within a surviving
process preserve the cart; process-death restoration is deferred.

Internally, the same semantic operations serve the graphical app and an
independent test driver. Neither renderer nor Android code computes a competing
cart total or owns a second quantity store. Destroying/recreating presentation
resources must leave the process-owned cart intact. This is the interesting
experiment; four screens would not strengthen it.

## 6. Recommended architecture

Size estimates below compare owned subsystems, not promised line counts.

| Architecture | Question answered; size and dependencies | Reuse, device fit, and risk |
| --- | --- | --- |
| **A. Pure C core + small native Android surface — choose** | Can a complete local cart stay visibly independent of platform code? One core, one fixture, one screen, existing APK boundary. | Useful reference for later language/backend work. ARMv7 A1 and AArch64 C67 fit the existing native lanes. No new language runtime required; main risk is letting drawing code grow into a widget framework. |
| B. Idriç owns the executable core + thin native renderer | Can checked cart invariants survive the compiler/backend boundary? Same app plus collection, numeric, and foreign-boundary qualification. | Highest immediate language value, but inspected backend contracts do not establish the full cart path. Native ARM branch fixtures still reject needed control flow; direct DEX lacks the general collection/object/wide-value path. High risk of turning the app into compiler work. |
| C. Fixture-driven browser specimen, then app-factory input | Can one declared catalog/interaction generate or drive an understandable application? Small if keeping the HTML sketch; much larger once generation is included. | Browser use on both phones is plausible but does not test the selected native boundary. No Idriç runtime dependency. Existing source saves UI work, but a factory adds an unearned second language/representation. |
| D. Inventory-aware customer catalog | Can a shopper find price, stock, and shelf location from a source-neutral snapshot? Cart/UI plus availability/freshness/policy and eventually an adapter. | Strongest real-store direction, suitable for either phone, no inherent Idriç requirement. More useful only with real data and a real store need; feed/policy uncertainty dominates. |

Choose A deliberately, not as an Idriç implementation in disguise. C owns this
specimen's executable semantics; the separate Idriç source is a checked model
and language experiment, never evidence that Idriç generated the APK. Replacing
the owner later requires exact conformance evidence and removal of duplicated
production semantics.

Use `NativeActivity`, direct native-window drawing, and the repository's
existing input/layout precedents. Keep the core under `green-grocer/model/`,
presentation separate, and build/platform machinery below the user-facing
purpose. No UI framework, generic event bus, service container, repository
pattern, or mandatory presentation-object hierarchy. The core can expose
read-only observations directly.

## 7. Semantic model

Example: one banana bunch at 129 cents, two prepacked apple bags at 499 cents,
and three cereal boxes at 379 cents give
`129 + 2 × 499 + 3 × 379 = 2264` cents, displayed as `$22.64`.
An unselected rice bag occupies no cart coordinate.

These are semantic descriptions, not claims of accepted Idriç syntax:

| Value | Contract |
| --- | --- |
| `product_identity` | Unique stable fixture-local token. Not a list position, visible name, SKU, UUID service, or Android resource ID. An enum or compact integer representation is sufficient. |
| `product` | Identity, name, exact price, package/unit text, logical image reference. One displayed package is one selectable unit. |
| `catalog` | Immutable finite collection of products, unique by identity. A simple list plus lookup suffices; no new catalog service. |
| `money` | Nonnegative integer USD cents, represented by unsigned 64-bit storage with checked addition/multiplication. No floats, currency conversion, decimal parser, or rounding. |
| Cart amount | Positive whole package count, represented within `1…2³²−1`. Zero means absence. The bound is a machine limit, not a stock assertion. |
| `cart` | Finite partial map from catalog identities to positive amounts, initially empty. A compact unique-entry array suffices; no hash-map framework is necessary. |
| Presentation state | `selected_only`, scroll offset, and temporary pointer/gesture state. No page stack. None belongs in cart state. |

Minimum operations:

- `add(identity)`: absent becomes one; otherwise increase by one.
- `set_amount(identity, count)`: positive replaces the coordinate; zero removes.
- `remove(identity)`: delete; removing an already absent valid identity is a no-op.
- Decrement is an input action using the current observation and `set_amount`;
  disabled at zero. All mutations pass through the semantic boundary.

Reject unknown identities, unrepresentable counts, and arithmetic overflow
without changing the cart. Validate the bundled catalog once, including unique
identities and valid asset references or a defined placeholder. Unresolved cart
keys are an invariant failure, never silently omitted from totals. Keep failures
small and explicit; no commerce error hierarchy.

Cart rows, line totals, overall total, distinct-product count, ordering, and
filtered rows are derived. Row order follows the catalog's declared order,
independently of insertion order. Price and product data are immutable session
inputs; no quote/snapshot/repricing machinery is needed.

### Exact mathematical structure

Let `product_set` be the finite set of identities. A cart stores a positive
integer on its selected subset; extending it by zero gives a function
`product_set → {0, 1, 2, …}`. Abstract unbounded carts form the free commutative
monoid on `product_set` under coordinatewise addition, with empty cart as zero.
This describes addition of quantities, not a vector space or a UI command to
merge carts. The bounded machine representation is not closed under that
addition; checked transitions can reject overflow.

With a fixed price per product,
`total(cart) = Σ price(identity) × amount(identity)` over selected identities.
It is an additive weighted sum into nonnegative cents. Each accepted input
determines one next state. The screen is an observation of catalog, cart, and
presentation state; it is not an invertible encoding of the cart.

The useful Idriç experiment is specific: express catalog membership of cart
keys and positivity of stored amounts, then show that add/set/remove preserve
them and that totals agree with exact fixtures. Renaming identities by a
bijection, while transporting prices and commands, must commute with transitions
and preserve totals. This is a useful representation-independence law; no
category framework is needed. Compile/typecheck claims, proof claims, and target
execution claims must remain separate.

## 8. Dependencies

| Dependency | Reuse now; boundary |
| --- | --- |
| ICK + Android NDK | ICK compiles the cart core and bundled catalog to target objects; NDK compiles the Android/test boundary and links those objects against the target runtime. Record these as separate stages. Qualify the exact core on both A1 and C67; if ICK fails or lacks a required capability, report the exact source, pin, command and diagnostic to the user. Do not silently substitute NDK compilation for the core or start compiler repairs here. |
| Cat Food | Authoritative A1/C67 target profiles and established artifact delivery/identity. Not a runtime dependency of cart operations. |
| Flexible Pipes | Reuse the paired-build orchestration when its current callable path is verified. Never assume a checked-in job prompt proves dispatch/build support. A missing path is an orchestration gap, not a reason to build only C67. |
| `android-NDK` / nearby utilities | Reuse the canonical NativeActivity/APK boundary, stable signer policy, and narrow useful rendering/input precedents. Do not copy unrelated sensor, clipboard, Shizuku, or keyboard behavior. |
| Idriç | Independent G1 below; no production dependency for architecture A. No generated-C/RefC fallback presented as Idriç acceptance. |
| IDK | No required feature of this cart calls for a second language/runtime. Its live repository was not resolved in this review; no readiness claim is made. |
| ai-ci / Grease | Reuse applicable build/evidence contracts and Grease for newly authored orchestration. No generic enforcement fork in Green Grocer. |

Current evidence behind these decisions:

- [Idriç checkpoint](https://github.com/isomorphisms/Idric/blob/ff4d852862a3942592f8ade9afde8d409d9803be/_/EDRIC.md)
  and [style](https://github.com/isomorphisms/Idric/blob/ff4d852862a3942592f8ade9afde8d409d9803be/STYLE.md): distinguish intended language semantics, bootstrap representation, and backend evidence.
- [Native ARM branching boundary](https://github.com/fuego-ironworks/idric-arm-thumb/blob/0ccef59e21415585c265b79360164f7351baa1c5/tests/branching/README.md)
  and [direct DEX boundary](https://github.com/fuego-ironworks/idric-arm-thumb/blob/044877107e179df2ab3c8420867d71faff254f9f/DEX-README.md): neither is evidence of a compiled whole-cart runtime.
- [ICK gate](https://github.com/dilapidated-shed/ick/blob/64fab64ff47acc627ce1154c3118f0232390cad5/docs/android-release-gate.md): focused four-ABI object/link evidence, with broader Android runtime/application qualification still distinguished.
- [A1](https://github.com/isomorphisms/catfood/blob/609a9628d5a52860f956bf62e0914a0cd03292ae/android/devices/miro-a1.md)
  and [C67](https://github.com/isomorphisms/catfood/blob/609a9628d5a52860f956bf62e0914a0cd03292ae/android/devices/miro-c67.md): A1 uses `armeabi-v7a`; C67's native lane is `arm64-v8a`. C67's ARM32 compatibility does not replace the requested native companion build.
- [Flexible Pipes delivery boundary](https://github.com/isomorphisms/flexible-pipes/blob/c8cb7ac069a798eaf6cc228de9a2c23b2b351587/docs/job-delivery.md)
  and its [A1/C67 assignment fixture](https://github.com/isomorphisms/flexible-pipes/blob/c8cb7ac069a798eaf6cc228de9a2c23b2b351587/regressions/job-delivery/a1-c67.txt): the policy exists, but that fixture alone does not prove an operating companion-build service.

Build both devices from the same source and fixture revision, on a build host.
Prioritize A1 if work is blocked partway; report the C67 obligation as pending.
Keep package identity stable, use a persistent signer, and record distinct
artifact, install, launch, interaction, and replacement-install evidence.
Neither device is a compilation host.

## 9. Unknowns

**Unknown but irrelevant to this first slice:** Eagle deployment/version/feed,
permissions and store policy; Kroger credentials/API behavior; decimal sale and
fulfillment rules; real catalog/image sources; production repricing/tax;
recurrence design; IDK readiness; a general generator's interface.

**Unknowns that block particular acceptance stages, not the model decision:**

- The selected ICK-core + NDK-boundary/link path needs exact compiler, NDK and
  qualified native harness pins before G3 builds. Repository-level precedent is not
  Green Grocer compiler or package acceptance.
- The callable Cat Food/Flexible Pipes paired-build/delivery path must be
  resolved before claiming shared-workflow completion. If absent, record the
  exact owner/interface gap; do not create a second orchestrator in this app.
- A1/C67 touch, text readability, scrolling, lifecycle behavior, and installation
  of these new artifacts remain untested. Physical acceptance needs access to
  the actual devices; absent access remains `NOT_RUN`.

No unresolved commerce or architecture question blocks G2. G1 may discover a
language gap; it blocks the Idriç experiment only. No additional Star is needed.

## 10. Downstream jobs

These are assignments for later execution, not authorization to run them during
G0. G1 and G2 can proceed independently; G3 consumes G2's exact verified head.
Each job must refresh live branches, read applicable instructions, inspect
existing work before branching, and preserve this decision's boundaries. No
job authorizes merging, closing existing work, publishing a release, or changing
another project's semantics. Return contradictory evidence to Sun instead of
quietly redesigning the task.

### Sun G1 — Check the Idriç cart experiment

**Objective:** determine how much of the actual sparse-cart invariant can be
expressed and checked today, and identify the first real backend obstruction.

**Inputs:** `Ashtray-Archer/utilities-android-phone-user`, `green-grocer`, this
`green-grocer/star-review.md` (sections 7–8); language/backend evidence pins in
section 8. Resolve and record current compiler/backend heads before using them.

**Exact boundary:** a small generated attempt under
`examples/autogenerated/green-grocer-cart/` in this repository. Write `attempt.md`
with the semantic types first. Then use actual documented Idriç syntax for a
three-product cart and add/set/remove observations. Keep the source independent
of Android. Resolve the `Number` discrepancy from compiler evidence, without
changing the language. Try only the smallest meaningful supported target handoff.

**Acceptance:** retain source and exact commands/revisions/results; demonstrate
or honestly mark blocked positivity/membership preservation, empty/remove
behavior, exact 2264-cent total, and identity-renaming invariance. Include a
zero-entry/foreign-key counterexample that must be rejected at the claimed
boundary. Identify the first unsupported operation if target lowering fails;
typechecking is not target execution. Stop with a bounded Earth handoff only
if a concrete, independently worthwhile language fix emerges.

**Exclusions:** renderer, APK, C production changes, RefC/generated-C fallback,
general category library, broad compiler/runtime repair, and making G2 wait.
Sun stops when evidence and the first language boundary are established.

### Earth G2 — Implement and verify the independent cart core

**Objective:** implement exactly section 7 as an independently inspectable C
core and bundled eight-product fixture.

**Inputs:** `Ashtray-Archer/utilities-android-phone-user`, current `green-grocer`,
`green-grocer/star-review.md`; historical baseline
`039961365c9a2ebe3af8d5668d90106141cf186a`. G1 is not a prerequisite.

**Exact boundary:** `green-grocer/model/`, fixture data/assets, and focused
semantic verification/build wiring. Expose add/set/remove and read-only
observations; import no Android or rendering headers. Compile/link through the
selected ICK-core + NDK-boundary/link policy. If a host execution lane cannot
satisfy that policy, report it or use a qualified target runner; do not invoke
an undeclared compiler.

**Acceptance:** verify empty cart; add twice; set positive; set zero; remove
present/absent; distinct identities with identical labels; catalog reordering;
the 2264-cent example; invalid identity; amount and arithmetic limits with
unchanged state on rejection. Check exact cents with independently calculated
expectations and targeted bad cases for zero retention and missing-key omission.
Totals/rows are derived and the core has no mutable shadow totals. Record exact
source/toolchain and the execution boundary. Supply the verified head and the
small API to G3.

**Exclusions:** Android UI, networking, persistence, inventory, general container
library, generated application framework, Idriç ownership claims, and editing
the historical hardware prototype to make it appear conformant.

### Earth G3 — One native screen, paired builds, honest device acceptance

**Objective:** make section 5 usable on A1 and C67 using G2's exact core.

**Inputs:** `Ashtray-Archer/utilities-android-phone-user`,
`green-grocer/star-review.md`, G2's recorded source head and API; current Cat
Food device/build profiles, Flexible Pipes interface, canonical `android-NDK`
packaging, and repository native input/rendering precedents. Refuse a missing
or materially changed G2 prerequisite.

**Exact boundary:** Green Grocer presentation, Android adapter, paired build
and existing delivery wiring. Keep the eight-product fixture, core operations,
and prices unchanged. Use the shared A1/C67 policy, prioritizing A1; resolve
package identity and persistent signer once. Render on change/input, with no
idle animation loop. Record exact ICK core compilation and NDK boundary/link
stages. An ICK failure blocks the affected stage and must be reported to the
user with its reproducer; changing the compiler path requires a new decision.

**Acceptance:** produce both ABI artifacts from the same source; inspect ABI,
signer, package identity, and version code. Exercise add/change/remove, exact
total, selected-only/return, empty filter, and drag-versus-tap on the real input
path. Inspect readable wrapped text and usable controls at each device's actual
window size. Confirm surface recreation preserves the cart; fresh process
starts empty as labeled. Record physical install, launch, interaction, and
replacement without uninstall separately for A1 and C67. If physical access or
shared orchestration is unavailable, finish the authorized local work and name
that boundary `NOT_RUN`/`BLOCKED`; do not report the executable slice accepted.

**Exclusions:** framework migration, inventory/Eagle/Kroger, accounts,
checkout, persistence, device-side compilation, compiler repair, and a new
cross-project build orchestrator. Stop and return a concrete Sun question only
if implementation disproves this selected boundary.

**Allocation:** Star decision complete. One independent Sun experiment; two
Earth jobs on the executable path. No mechanical rollout currently warrants
Moon, and no further Star review is required.
