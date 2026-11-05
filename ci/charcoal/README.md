# Charcoal utility interfaces

This change covers the standalone Programmer's Unicode Picker, the existing
Compact Math Keyboard activity and input method, and the clipboard NativeActivity
test window. Hardware CLI probes inherit terminal colors; text-pad contains
reference images rather than an executable application.

The C header and four Android resource files are exact copies of android-NDK
`e8b67dfc67776c2b8da919785d15f38d68c91db0`. The shared canonical Grease guard
checks those bytes. Picker background, buffer, keys, borders, title and text
use the corresponding neutral roles. Page data, UTF-8, cursor, selection,
undo, touch geometry, copying and clipboard tests are unchanged.

Both native packagers now compile and link the resource table, preserving their
existing package, API floor, version and persistent public test signer. The
clipboard smoke app still finishes after its real framework receipt; its
otherwise empty application window now has an explicit dark theme.

The compact IME already uses Java and direct javac/d8 packaging. No new Java
file or framework is introduced. Its existing class selects a resource theme
in its constructor, before InputMethodService.onCreate, and both existing
root layouts receive the charcoal background resource. The theme derives from
the platform InputMethod theme so input-method window semantics are retained.
Android documents this timing in
[InputMethodService.setTheme](https://developer.android.com/reference/android/inputmethodservice/InputMethodService#setTheme(int)).
The packager generates the real R.java before compiling those existing classes.

New packaging orchestration is in the two Grease helpers, rather than another
shell dialect or a second application model. Their boundary is explicit Android
packaging paths/SDK/version arguments; they add no editing or keyboard logic.
The prior keyboard packaging sequence is preserved, including DEX, zip alignment,
package/version and finished-signer verification. No fresh key is generated.

Local canonical resource/header checks, Grease parsing and whitespace checks
pass. Actual compiler, APK and existing functional emulator workflows must run
on this source. API29/API34 both-night-mode visual/lifecycle acceptance and all
physical C67 checks remain NOT_RUN. No build or source check establishes them.

This branch starts at main34b2391 and leaves the active Pasteboard and sensor
instrument implementation branches intact. It does not overwrite their changes.
No merge, release, deployment or installed-app replacement is performed.
