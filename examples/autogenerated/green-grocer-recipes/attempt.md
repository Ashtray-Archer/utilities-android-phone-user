# Recipe shopping: Idriç implementation attempt

## Saved recipe file type sketch

`encode_recipe : Recipe → Text` and `decode_recipe : Text → Either Text Recipe`
form a versioned, length-delimited text file boundary for saved recipes. Field
lengths count decoded characters; the platform file adapter must perform UTF-8
encoding/decoding without replacement. Length framing preserves newlines,
colons, source queries, significant whitespace and supplementary characters.
Ingredient identities, exact rational quantities, dimensions, preparation,
permitted substitutions, servings and source notes all remain explicit fields.
No retailer observation, approval, private credential or reviewed-cart flag is
persisted in a recipe file.

`restore_recipe : Text → ShoppingState → ShoppingState` admits only a complete
supported file through the existing save transition. Malformed/truncated data,
invalid numeric fields, zero denominators/servings and trailing fields are
refused without changing saved recipes, the plan or its review. Successful
restore replaces the same recipe identity and revokes review. Parsing is pure;
atomic file replacement and actual Android reopening are separate platform
effects and must not be claimed by codec tests.

Host result: all 24 storage cases pass and three changed codecs compile and fail
their intended assertion (source notes erased, fractional denominators lost,
trailing fields accepted). The complete storage command emits direct DEX and
independent baksmali decoding succeeds. Source and output hashes are retained
under `green-grocer/receipts/recipe-storage/`. Actual ART, file I/O and Android
reopening remain separate until their named observers execute successfully.

The tests must inspect independently expected decoded fields, exercise corrupt
inputs and show state preservation. An encode/decode self-roundtrip alone is
insufficient. No alternate application model or JSON format is introduced.

## Runtime continuation

Run the existing 51 independently expected recipe checks and six altered
implementations through the same canonical direct DEX compiler, then execute
them under Android ART. Export the existing checked test command as an IO Unit
entrypoint; keep every assertion and application operation in Idriç. The
positive command must return its expected lines and exit zero. Each compiled
mutation must fail at its original assertion, rather than at verification or
loading. This extends acceptance and does not supply Android UI evidence.

An external observer will also call the four actual runtime exports with
changing boundary/Unicode/empty inputs. Its expected results are independent
constants. Assembly, compiler output, device fingerprint, bytes transferred,
process status and actual output must remain separately inspectable.

## Continuation type sketch: source links and shopper-selected stores

The recovered October 24 requirements include saved recipes originating in
TikTok/YouTube videos and a single shopping plan split across stores, with the
shopper choosing products and stores. This continuation extends the existing
semantic owner. It does not infer ingredients from a video or perform a retailer
write.

`RecipeSource` retains an exact source link and a shopper-entered note. A recipe
has zero or more sources; an empty source link is rejected without changing the
saved recipe. Editing ingredients or servings must retain the source records.
No network effect follows from storing a link.

`RequirementStore` binds an ingredient/dimension requirement to an explicit
store override. The existing selected store is the default; each override wins
only for its requirement. Returning to the default removes that override.
Changing either choice invalidates review. Each approval and fresh observation
must match the store selected for that requirement. Shopping arithmetic remains
in the existing planner, with no alternate quantity or money model.

`prepare_store_handoffs : ShoppingState → Either Text (List StoreHandoff)`
partitions the existing fully reviewed handoff by exact retailer and location.
An unresolved line in any store blocks the whole proposal. Equal SKU/UPC text at
different stores must never merge. These are pure proposals, not sent orders.

Acceptance will execute independent exact totals, source preservation, store
overrides, reset-to-default, stale approvals, missing/changed observations and
store-separated handoff cases, plus compiled deliberate mutations.

### November 15 result

The checked host suite passes all 51 cases on frontend
`2bce7677f513d711875e978bd6f5ad28b9e54d58`; all six compiled semantic mutations
fail their intended behavioral assertions. The runtime probe also produces its
nine independently expected output lines. The first edit used the reserved
keyword `choice` as a binding; the parser rejected it. Renaming the binding to
`store_choice` repaired that source error, without a language or backend change.

A fresh canonical android-NDK driver at
`6ab4818219efa7ea5eda137ca6b76976eb2a4d42`, built with the same frontend, emits
the actual updated runtime probe via `--directive managed-exports`. No narrowed
arithmetic, generated C, Java or alternate application model was introduced.
The earlier recursive Number-helper emission blocker is therefore superseded
for this probe. ART execution and full renderer/storage integration remain the
next acceptance boundaries. No new missing language capability is established
by this continuation; lowering alone does not prove the complete app works.

The older results below remain historical. Current hashes and outputs are in
`green-grocer/receipts/recipe-stores-2026-11-15/`.

## Type sketch (written before implementation)

The shopper chooses saved recipes and positive serving counts. Ingredients have
their own text identity, rational quantities, dimensions and preparation text.
Mass, volume and count remain distinct; no onion-to-grams assumption is made.
Aggregation precedes pantry subtraction and package rounding. Zero requirements
stay visible as pantry-covered rows. Exact nonnegative rational arithmetic uses
Number and a denominator encoded as one plus a Number, so division by zero is
unrepresentable in a constructed quantity.

Four identities remain separate: ingredient, internal product, retailer/store
product identifier, and optional checkout identifier. All external identifiers
are Text. No identifier is parsed as a number. An approved match retains the
observed store/product/package/price/availability. A fresh observation must
agree before a reviewed cart can be handed off. Unknown, unavailable, missing,
changed and ambiguous products stay explicit. Money is exact USD cents; the
existing C oracle's uint64 money, uint32 package count and 64-line bounds are
checked when projecting to that oracle, rather than redefining recipe meaning.

Central operations:

- scale ingredients by selected servings / recipe servings;
- combine requirements with equal ingredient identity and dimension;
- subtract explicitly supplied pantry quantities, clamped at zero;
- approve a candidate for a requirement, including an explicit substitution;
- round total remaining demand up to whole saleable packages;
- revalidate product observations and derive exact SKU-backed cart lines;
- reduce user actions into screen and shopping state, beginning at Recipes.

Pure transitions return an updated state or a specific visible failure. Effects
(storage, rendering, catalog HTTP, OAuth and retailer writes) belong below this
boundary. Runtime acceptance must call the actual checked Idriç operations with
changing input. Host Chez execution is only a semantic control; Android requires
the canonical android-NDK direct DEX driver and actual ART execution.

## Desired result and known boundary

Extend `green-grocer-executable` at ac1398cc25a1ed56c8221cfb08da73770975441f.
The original C cart and NativeActivity are retained as independent oracle and
renderer. The historical cart specimen does not run in that APK. No Java,
generated C, WebView, alternate cart owner or completed-order claim is allowed.

This implementation requires dynamic records, lists, exact rational arithmetic,
text identity equality, calls and effects. Existing DEX scalar/opaque-reference
exports do not establish support for those values. The runtime probe will expose
the first actual rejection using a legal scalar observation boundary; it must
not erase or relocate the domain values to avoid that rejection.

## Executed results

Frontend 740bbf2b1d9e811fd00c719c3473eede17ceea3b checks the source and builds
the host program through Chez. The maintained `green-grocer/_/check-recipes.grease`
ran 37 cases and three behavioral mutations. All positive cases pass. Each
mutation compiles, executes and fails the intended assertion (pantry ignored,
rounding down, changed-price acceptance). `RecipeRuntime` separately executes
on the host with outputs 0, 1, 2, 2 for 0, 300, 301, 500 grams; saved-name and
empty-name cases exercise actual transition decisions.

The canonical backend at de29100e9b24b5bbc85aca7f77cd3b08c0934917 required two
reserved-binding renames to build. Its actual checked DEX compiler then rejects
`RecipeRuntime` with `Recursive checked helper is not in the DEX specialization
slice: Data.Nat.div'`, exit 1, no recipes.dex. The same failure is reduced in
android-NDK's `examples/autogenerated/green-grocer-package-rounding/`. Do not
replace exact Number operations with narrow machine integers to hide it.

`SavedRecipeRuntime` separately emits DEX from the real record/list save
transition; pinned baksmali independently disassembles it. This does not prove
runtime execution. Exact source and executed compiler hashes, raw host and
mutation results, failing DEX log and generated save-probe bytes live in
`green-grocer/receipts/recipe-idric/`.

The application is incomplete: renderer/editor/storage integration, current
C-oracle differential execution, live retailer adapter, producer qualification,
recipe-first APKs and physical A1/C67 behavior remain unimplemented or NOT_RUN.
No fallback was selected. The next compiler obligation is exact recursive
Number-helper lowering, demonstrated by the failing emission test followed by
the independent ART observer. Later collection/effect boundaries must be probed
in turn; this one rejection is not proof about all of them.
