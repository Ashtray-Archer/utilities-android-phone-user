# Saved recipe files on Android

`RecipeFiles.idric` connects the existing checked codec and save/restore
transitions to real Android files. Callers supply an absolute, app-owned path
for one recipe file. A file does not replace the entire saved-recipe collection:
loading it uses `restore_recipe`, adding or replacing its identity and revoking
review only after successful decoding. The interface and private-directory
filename mapping remain application integration work.

`save_recipe_file` validates the recipe before touching storage. It commits
through Android's framework AtomicFile, flushes the UTF-8 writer, synchronizes
the file descriptor and reads the replacement back before returning the normal
save transition. Failure retains the caller's recipes and review. Error details
go to the process-scoped `GreenGrocerRecipeFiles` log; the shopper-facing notice
reports that the previous recipes were retained.

`RecipeFilePlatform.idric` owns only SDK stream, UTF-8 and replacement mechanics.
Fresh UTF-8 encoders and decoders use their reporting policy. Invalid Unicode
is refused before replacement; malformed UTF-8 is never silently repaired into
a different recipe. NUL, newlines, whitespace and supplementary characters are
data. The adapter does not use JNI modified UTF-8, Java/Kotlin source, Gradle,
SQLite or a second recipe implementation.

The resource boundary refuses files above 1 MiB and text above 262144 UTF-16
units. This bounds file work on the phone, independently of the domain's exact
quantity values. Recipe quantities and identifiers are not narrowed. Callers
must serialize operations on a given file; AtomicFile is not a locking API.
This is not a filesystem-wide transaction or power-loss qualification.

Platform contracts:

- [AtomicFile](https://developer.android.com/reference/android/util/AtomicFile)
  owns stream completion, failure rollback and unfinished-write recovery.
- [CharsetDecoder](https://developer.android.com/reference/java/nio/charset/CharsetDecoder)
  and [CharsetEncoder](https://developer.android.com/reference/java/nio/charset/CharsetEncoder)
  define strict decoded/encoded character handling.

## Verification boundary

The recipe workflow prepares the actual file operations as direct DEX and an
independent Smali caller. API29/API34 x86_64 jobs exercise:

- Saving exact source links/notes, embedded NUL and Unicode, ingredient identity,
  dimensions, substitutions and a rational numerator above uint64.
- Independent byte comparison against ordinary UTF-8 SDK encoding.
- Reading the file from a new ART process, rather than reusing in-memory state.
- Invalid UTF-8, corrupt schemas, missing files, invalid recipes, relative paths
  and failed writes with the existing recipes and review retained.
- A real unfinished AtomicFile write created by an independent SDK caller;
  another process then invokes the production loader and must recover the
  preceding good recipe. This qualifies loader recovery, not an installed
  Activity's lifecycle or an injected interruption within the production writer.
- Byte/text budgets, an invalid Unicode write, and an exact round trip at the
  maximum accepted 262144 UTF-16 units, including whitespace, NUL and Unicode.
- A compiled mutation using a replacement-decoding reader. It must execute and
  fail the original invalid-UTF-8 state-preservation assertion; a compiler,
  verifier or process-loading failure cannot count as its rejection.

The previous 52 recipe cases, 24 codec cases, 14 changing-input observations and
nine compiled mutants remain required. The file mutant brings the total to ten.
Every stage retains exits, stdout/stderr, PID-tagged calls, Android logcat,
source/DEX digests and the exact application revision.

The file adapter requires the checked constructors, IORef and exception surface
from backend `dfcf2cb22c0ebcc4ce999189cc2f066593bf3689`, exposed by
[isomorphisms/android-NDK PR #47, “Support checked Activity state, callbacks and native pixel output”](https://github.com/isomorphisms/android-NDK/pull/47).
That is an explicit dependency candidate, separate from canonical main
`6ab4818219efa7ea5eda137ca6b76976eb2a4d42` used for the preceding acceptance.
Frontend remains `2bce7677f513d711875e978bd6f5ad28b9e54d58`. Existing recipe gates
are rerun with the changed backend rather than carrying old results forward.

Current exact-head results are recorded in
[Ashtray-Archer/utilities-android-phone-user PR #88, “Implement Idriç recipe planning and exact saved recipe files”](https://github.com/Ashtray-Archer/utilities-android-phone-user/pull/88).
Source presence and local DEX emission are not Android file execution.

The recipe editor, native renderer/input connection, app-private directory
integration, installed-APK reopening, stable signed recipe APK and physical
A1/C67 acceptance remain unfinished. The existing sample-cart APK is not this
recipe implementation. No retailer account, cart or order is touched.
