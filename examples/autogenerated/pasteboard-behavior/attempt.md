# Fixed Pasteboard executable model

## Intended boundary

Continue the recovered application on `recover-pasteboard-20261101`. The checked
Idriç reducer owns capture, exact-text duplicate handling, ordering, board
selection, movement, pinning, deletion and archive retention. C owns Android
clipboard conversion, rendering, JNI invocation, file locking and atomic byte
storage. It must not implement a second reducer.

Values are decoded `Text`, entry identities, three board positions, protected
entries, a sequence of retained entries, and explicit results. The model keeps
all retained text separately from the sixteen visible positions on each board.
Storage encoding and Android classes are boundary representations.

The central exported operation is a checked `Text → Text` transition. Its input
contains one length-delimited action and a complete length-delimited snapshot;
its result contains an explicit outcome and the next validated snapshot.
Lengths count decoded Unicode scalar values in the reducer protocol. The file
adapter separately frames standard UTF-8 by byte length and validates integrity.
No character count is reused as a byte count.

## Invariants and decisions

- Initially select the middle of three unlabeled boards, each with 16 visible
  positions, two columns and eight rows.
- A capture inserts newest first. Exact duplicate text has one retained entry;
  recopying promotes that entry without discarding its protection or board.
- Tapping an entry marks that clipboard value seen, so the app's own copy does
  not become a new capture.
- Overflow archives the oldest unprotected visible entry. An entirely protected
  board sends a new capture directly to the archive. No overflow deletes text.
- Only adjacent-board movements are allowed. A full protected destination
  refuses a move and leaves the source intact.
- Protection blocks deletion until explicitly removed. Deletion is explicit and
  must be committed atomically before the UI reports success.
- Embedded NUL is refused with a visible unsupported-text result. Empty and
  non-text clipboard items are distinguished from unavailable clipboard access.
- Sensitive clips are excluded from durable capture by default. Clipboard
  contents must never appear in logs or network traffic.

## Required execution

Use the canonical direct DEX backend and checked source frontend. Qualify a
separate caller against emitted DEX under ART; compile semantic source mutants
and require the same observer to reject them. Host reference behavior and DEX
decoding remain separate evidence. First-party Java/Kotlin, generated Java,
`javac`, `d8`, Gradle and RefC are excluded.

Starting inspected heads:

| Repository | Revision |
| --- | --- |
| utilities recovery | `18411b63faf7fa50a935bb04e38ff5669fcf1a47` |
| utilities main | `34b2391a97867ab25ea885cedc7ac2e04b591609` |
| android-NDK main | `6ab4818219efa7ea5eda137ca6b76976eb2a4d42` |
| Idriç | `0e8cfdd1106ebdeffd01b6c519880ea3afc7dc35` |
| ai-ci main | `a66e3a9bb1070a338f8c4a67f9b4b4ac39dfb5ec` |
| Cat Food main | `eafd7fa9e61138a664b205bf8ba82ec6837bbbb9` |

## Execution record

Implementation and qualification are in progress. No DEX/ART, observer, APK,
emulator or physical-device success is claimed by this attempt record.
Compiler/runtime failures and resulting reusable language work will be retained
here when observed.
