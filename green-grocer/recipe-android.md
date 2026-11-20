# Native recipe candidate

The recipe APK starts at Recipes and connects the exact Idriç model to native
Android drawing and touch. Import a saved recipe, select/unselect it, change
servings, inspect combined requirements, and add/remove pantry quantities.
Details displays ingredients, source links and notes, and saves through the
same strict UTF-8 AtomicFile adapter used by the file acceptance lane.

New recipe and Details → Edit recipe open an isolated draft. It includes name,
positive whole servings, ingredients, exact decimal/fraction quantities in g,
mL or each, preparation, source links and notes. Ingredient identities are
fresh unless the shopper explicitly chooses an existing ingredient to combine
shopping quantities. Existing substitution choices are retained; choosing a
different identity does not silently approve its substitutions.

Save validates the complete draft and updates the saved shopping state only
after durable storage succeeds. Invalid or failed saves retain the draft and
preceding review. Canceling a changed draft asks whether to discard it. The
draft is process-session state and is lost if Android ends the process before
save. Live catalog search, product/store selection controls and retailer cart
submission remain unfinished. Stores says
connections are unavailable. An unresolved plan cannot pass final review. The
candidate makes no retailer requests.

## Ownership and runtime boundary

`RecipeSession.idric` encodes the complete typed shopping state as a versioned,
length-framed opaque Text: saved recipes, selections, exact quantities, pantry,
default and per-ingredient stores, approved matches, observed products, screen,
history, review and notice. It preserves arbitrary-precision values, optional
metadata, Unicode and NUL. It is a process-session transport; only recipe files
are durable, so reopening starts at Recipes without restoring selections.

`RecipeScreen.idric` derives all displayed rows, labels and opaque action
tokens from that state. It admits only controls visible in the current screen,
then calls the existing shopping transitions. `RecipeNative.idric` exposes
checked scalar Text/Bool/IO Text methods for start, validation, projection,
actions, back eligibility, import and restore. No arbitrary object marshal or
numeric narrowing crosses JNI.

`RecipeDraft`, `RecipeText`, `RecipeEditorSession`, `RecipeEditorScreen` and
`RecipeEditor` add the optional draft, raw fields, typed text selection and
composition, checked controls, and a fresh input generation to that shopping
session. Text fields admit at most 262144 UTF-16 units. Numeric recipe values
remain arbitrary precision. Ingredient/source overviews and identity choices
are paginated; typing projects a small caret window while retaining all text.
Recipe files keep their existing schema.

`recipe_engine.c` loads the actual checked DEX asset through Android's
InMemoryDexClassLoader. It retains the opaque state independently of the native
window, serializes pure model calls and commits a candidate only after its complete
projection parses. `recipe_view.c` decodes codepoint-counted fields into exact
UTF-16 spans, including surrogate pairs and embedded NUL. It refuses malformed,
truncated or over-budget projections as a whole, preserving the preceding view.
Transport budgets are 1048576 UTF-16 units, 512 rows and three controls per row.
They do not narrow recipe-domain arithmetic.

`recipes_main.c` and the existing canvas implement drawing, wrapping, scrolling,
hit boxes and native lifecycle/input. Each native window retains one Java Surface
and producer connection across frames; window termination and activity exit
release it explicitly. Frame-local paint/canvas references remain disposable.
They do not calculate recipe quantities,
prices or shopping decisions. The recipe APK links no `model/cart.c` or sample
catalog. Checked Idriç framework plans emit the NativeActivity subclass and
Runnable callback classes; no Java/Kotlin application source, Gradle, framework
form replacement or Smali application producer is used.

`recipe_input.c` adds a transparent 1 × 1 focus View and BaseInputConnection
solely for the SDK input protocol. Idriç emits their checked DEX constructor
and callback plans. The SDK keyboard, clipboard, UTF-16 positions and temporary
Editable read projections cross this adapter; editable text and selection stay
in Idriç. Each connection binds an exact field/generation key. Late commands
after field changes or discard refuse. A separate class loads TextAttribute
overloads only on API33 and later. Plain hardware keys use the SDK key mapping;
hardware dead-key composition is not qualified by this lane.

Keyboard callbacks never perform filesystem I/O. File effects release the
state mutex while running on the native thread, then Idriç merges their result
with newer independent draft text. Unexpected concurrent shopping changes
refuse rather than overwriting the current session.

## Import and private files

Import opens Android's ACTION_OPEN_DOCUMENT picker on the UI thread. The native
result callback starts a worker that opens the returned URI through
ContentResolver, copies at most 1 MiB of raw bytes to a private temporary file,
then wakes the native event loop. Only Idriç decodes, validates and saves it.
Cancellation, provider errors and invalid files preserve the existing model.
The worker keeps no window pointer and performs no provider I/O on the UI thread.

Owned recipes live in `files/recipes`. Filenames are decimal 128-bit polynomial
identity fingerprints, not cryptographic hashes. Full recipe identities remain
inside each file. Before replacement the app checks the existing base/AtomicFile
backup identity and refuses a collision or unreadable ownership. Identity text
never becomes a path component. Startup enumerates owned files and restores
them through the existing model transition, including AtomicFile backup recovery.
Operations are serialized within the app process.

## Build and evidence

The maintained workflow is
[green-grocer-recipes.yml](../.github/workflows/green-grocer-recipes.yml). It pins
the Idriç frontend to `2bce7677f513d711875e978bd6f5ad28b9e54d58` and the backend
to `f4e68eb012a69a7648ff5991cd320ec61feb5eaf`. That candidate includes
[isomorphisms/android-NDK PR #47, “Support checked Activity state, callbacks and native pixel output”](https://github.com/isomorphisms/android-NDK/pull/47)
and the packaging repair in
[isomorphisms/android-NDK PR #48, “Store and page-align native libraries for direct APK loading”](https://github.com/isomorphisms/android-NDK/pull/48).
Canonical backend main remains a separate source identity.

NDK 29.0.14206865 compiles the native SDK adapter; the checked Idriç compiler
emits application semantics and callback DEX. Android platform/build-tools 36
package A1/armeabi-v7a, C67/arm64-v8a and x86_64 candidates. Package
`org.isomorphisms.greengrocer` remains stable, version code is 2, and the existing
public test certificate is
`de9b1d47c5a65e6d46a204b79dd9ee566b9d3c9832ba81ebc4213d3392e92ff9`.
Native libraries must be stored, 16 KiB ZIP-aligned and byte-identical after
signing; receipt verification is independent of installation.

Invoke the Grease helpers with absolute paths; each prints its full interface
without arguments:

- `prepare-recipe-sdk.grease`: declared SDK/NDK inputs and existing public signer.
- `prepare-recipe-ui.grease`: checked host/DEX screens, callback generation,
  deterministic DEX repetition, native checks and three signed candidates.
- `build-recipe-native.grease` and `package-recipes.grease`: native adapter and
  maintained NativeActivity producer with receipt/input validation.
- `package-recipe-observer.grease`: independent SDK-only acceptance APK and host
  expectations; it never produces application semantics or callback DEX.
- `observe-recipe-ui.grease`: separate emulator execution, installed-app touches,
  private bytes, screenshots and cold-process restore evidence.
- `observe-recipe-editor.grease`: checked editor ART/mutation controls, installed
  creation/editing with a system-bound test IME, failed-save retry and cold reopening.

| Gate | Required observation |
| --- | --- |
| Existing planner/codec | 52 + 24 cases, 14 changing runtime inputs and 11 compiled mutants, including strict file effects, API29/API34 ART |
| Screen/session | 16 host and ART cases; independent full-state semantic observations, stale-control refusals and three source mutants rejected at their named assertions |
| Editor/text | 37 host and ART cases, including the full text limit; stale field and decimal mutants rejected at their named assertions |
| Native text boundary | Unicode/NUL, every truncated prefix, schema, surrogate and budget checks; compiled surrogate mutant exits at the expected assertion |
| Candidate packaging | Actual checked DEX and callback bytes, stable signer/version, three ABI builds, independent finished-APK receipt verification |
| Installed x86_64 app | Own app UID, exact installed APK, 12 expected screen states, real touch input, saved UTF-8 bytes and distinct-PID cold reopening, API29/API34 |
| Installed editor | 36 host-projected views, real InputMethodService connection, composition, supplementary characters/NUL, selection, clipboard, stale-connection refusal, invalid servings, failed save/retry, exact demand and edited-file cold reopening, API29/API34 |
| Import callback | Real picker opening followed by an **injected SDK file-URI result**, then actual native worker and Idriç save; not provider-grant or human selection qualification |
| Physical A1/C67 and independent producer promotion | NOT_RUN |

Only the x86_64 acceptance candidate enables model-view hashes and native hit-box
logs and is debuggable. The independent Smali SDK observer compares full-view
String hashes to checked host projections, taps actual native bounds, requires
visible native background/button pixels in every screenshot (up to four RGB
levels per channel for observed capture quantization), and verifies private
file bytes. Failed pixel checks retain the captured image. It does not replace
app logic.
The test keyboard is in the acceptance APK, runs in its own process and calls
the real served InputConnection through Android. The editor observer uses
touches and SDK text queries, never the app's semantic exports. Its broadcast
command receiver exists only in that test APK. The emulator's previous keyboard
is restored after the test.
Emulator results do not establish physical phone acceptance, a live provider
grant, retailer access or independent producer registration.

Current source, terminal workflow results and remaining work are recorded in
[Ashtray-Archer/utilities-android-phone-user PR #88, “Connect Idriç recipes to native Android screens and saved files”](https://github.com/Ashtray-Archer/utilities-android-phone-user/pull/88).
