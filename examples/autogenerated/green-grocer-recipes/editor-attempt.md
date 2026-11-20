# Native recipe editing

## Intent and types, before implementation

Create and edit a saved recipe directly in the existing native Android app.
`ShoppingState` remains the owner of the saved plan. An `EditorSession` adds an
optional `RecipeDraft` and an input generation; edits do not change the saved
recipe, selections, pantry, product observations, or approved review until a
durable save succeeds. Canceling a dirty draft requires an explicit discard.

`RecipeDraft` retains the immutable recipe identity, raw name/serving fields,
ingredient drafts, source links/notes, the selected editor page, and a dirty
flag. Ingredient drafts retain exact identity, dimension, raw exact quantity,
preparation and existing permitted substitutions. New identities are fresh;
combining with an ingredient already used by another recipe is an explicit
shopper choice. No product or store is selected by the editor.

`RecipeText` owns Unicode text, a UTF-16 selection and optional composition
range. The Android IME specifies UTF-16 offsets; Idriç validates boundaries and
never splits supplementary characters. Commit, composition replacement,
selection, deletion, cursor motion and surrounding-text snapshots belong here.
The scalar SDK positions are bounded transport values, not recipe quantities.

Central pure operations:

- `editor_action` new/edit cases: create an isolated draft or copy every saved
  field, retaining sources and permitted substitutions.
- `change_draft` / `edit_text`: update only the draft; reject stale field keys.
- `validate_draft : RecipeDraft → Either Text Recipe`: require legal exact
  nonnegative numeric spelling, positive servings/ingredient quantities and
  nonempty required fields; decimals and fractions stay rational.
- `editor_view : EditorSession → RecipeView`: expose only current controls.
- `encode_editor` / `decode_editor`: complete length-framed
  opaque session, including selection/composition and input generation.
- `merge_file_effect`: adopt a completed saved-file operation while preserving
  newer independent draft edits; refuse an unexpected concurrent shopping change.

`RecipeNative` owns the save effect through the existing UTF-8 AtomicFile
adapter and filename ownership guard. Success revokes review through the
existing save transition. Validation failure or failed I/O retains the draft
and preceding saved shopping state. Recipe files keep their existing format.

## Android boundary

The existing NativeActivity/Surface renderer remains the visible UI. A tiny
transparent focus View and a BaseInputConnection subclass provide the Android
keyboard protocol only. Their constructors and native declarations must be
emitted through checked Idriç DEX plans, never Java/Kotlin/Gradle or Smali as an
application producer. SDK Editable values are temporary read projections of
Idriç text, not another text or shopping state owner. Native code transports
SDK calls, clipboard text, focus keys and drawing notifications.

Pure IME callbacks use the current opaque session under the state mutex.
File effects run outside that mutex; their results merge with newer draft text
before presentation. Each input connection binds the exact current field key,
so delayed commits after switching fields or discarding a draft are refused.

## Acceptance

Host and direct DEX/ART checks must exercise independent expected quantities,
all draft fields, invalid/zero/decimal/fraction inputs, unchanged saved state
and review during editing, explicit discard, stale input keys, supplementary
characters, NUL, selection, composition and both deletion units. Compiled
mutants must fail their intended assertions.

Installed API29/API34 acceptance must create a recipe, edit its ingredients and
source notes through the actual InputConnection, save it, select it and inspect
the exact demand, then force-stop and reopen it. Preserve the existing import,
file-effect and native UTF-16 gates. A test IME and independent SDK observer
are acceptance code only. Physical A1/C67 and retailer writes remain separate.

## Current compiler boundary

Frontend `2bce7677f513d711875e978bd6f5ad28b9e54d58`, backend
`f4e68eb012a69a7648ff5991cd320ec61feb5eaf`. The generic typed DEX IR supports
instance/constructor plans; the convenience framework writer only supplies
no-argument constructors. The input adapter therefore needs explicit checked
SDK constructor plans. Source presence does not qualify those constructors or
the IME path until independently decoded and run on ART.

The implementation uses those explicit typed SDK constructor plans and keeps
the semantic owner described above. Host, direct ART and installed system-IME
execution are recorded by separate workflow receipts. Source presence alone
does not establish any execution gate. No alternate semantic owner or compiler
fallback is selected.
