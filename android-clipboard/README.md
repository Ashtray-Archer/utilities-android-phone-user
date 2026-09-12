# Android clipboard JNI bridge

Small C/JNI boundary for Android's real `ClipboardManager`. Application editing, terminal, and message semantics stay native; this directory owns only Android framework lookup, text conversion, clipboard calls, and explicit failure reporting.

## Native surface

```c
ClipboardStatus clipboard_bridge_init(JNIEnv *environment, jobject context);
ClipboardStatus clipboard_bridge_shutdown(void);
ClipboardStatus clipboard_has_text(int *out_has_text);
ClipboardStatus clipboard_get_utf8(char **out_bytes, size_t *out_len);
ClipboardStatus clipboard_set_utf8(const char *bytes, size_t len, int sensitive);
ClipboardStatus clipboard_clear(void);
void clipboard_free(void *allocation);
```

`clipboard_bridge_init` stores the `JavaVM` and one global `ClipboardManager` reference. It does not retain the Activity/Context. Native worker threads are attached to the VM for the duration of an operation and detached only when this bridge attached them.

Initialization and shutdown are lifecycle operations and must not race clipboard calls.

## Text contract

The native ABI is standard UTF-8 with an explicit byte length. It does **not** use JNI modified UTF-8 for application text. That means embedded NUL is preserved, supplementary Unicode characters round-trip through UTF-16 surrogate pairs, overlong/invalid UTF-8 is rejected, and an unpaired UTF-16 surrogate received from Java is represented as U+FFFD in returned UTF-8.

On a successful `clipboard_get_utf8`, the returned buffer is allocated with `malloc`, has a convenience trailing NUL, and may contain earlier embedded NUL bytes. `out_len` is authoritative. Release it with `clipboard_free`.

Empty clipboard text is a successful zero-length value. `CLIPBOARD_NO_VISIBLE_CLIP` instead means Android returned no readable primary clip. Android uses the same observable state when there is no clip and when clipboard access is hidden because the app is not the default IME and lacks input focus; the bridge does not invent a distinction the framework does not expose.

The first `ClipData.Item` must directly contain text. This first slice does not resolve clipboard URIs, start intents, or persist clipboard data.

## Copy, clear, and sensitive data

Copy uses `ClipData.newPlainText` and `ClipboardManager.setPrimaryClip`.

When `sensitive != 0`, API 24+ attaches the boolean description extra with key `android.content.extra.IS_SENSITIVE`. Android 13 / API 33+ recognizes that key as the system sensitive-clipboard rendering hint. On older Android releases the text is still copied; there is no system sensitive-preview behavior to request.

Clear uses `ClipboardManager.clearPrimaryClip()` on API 28+. Older Android releases receive an empty plain-text primary clip because the real clear method does not exist there.

## Paste boundary

A paste request only returns bytes. This bridge never injects key events, appends to a terminal, submits a form, sends a message, or executes returned text. The caller must decide where a user-requested paste is inserted.

There is no listener, polling loop, clipboard history, network path, or persistence in this directory.

## NativeActivity initialization

A native-only Android app already has the required objects through `ANativeActivity` / `android_app`. Attach the thread to `activity->vm` if needed and pass the resulting `JNIEnv *` plus `activity->clazz` to `clipboard_bridge_init`. No Java/Kotlin application class is required.

## Checks

```sh
JAVA_HOME=/path/to/jdk ./run-host-tests.sh
```

The host check exercises standard UTF-8/UTF-16 conversion including embedded NUL, boundary code points, malformed UTF-8, surrogate pairs, and unpaired surrogates. It also compiles the Android-framework JNI bridge against the JDK JNI interface with warnings treated as errors.

### Native-only Android smoke receipt

`device-smoke/` is a disposable `android.app.NativeActivity` test APK. It has no DEX, Gradle, Java, or Kotlin layer and automatically finishes after the receipt.

With an Android SDK/NDK and one ADB-visible device or emulator:

```sh
ANDROID_ABI=armeabi-v7a sh device-smoke/run-device-smoke.sh
```

`ANDROID_ABI` may also be `arm64-v8a` or `x86_64`. The default minimum API is 24 and may be changed with `ANDROID_API`.

A passing receipt proves on the Android framework itself that the bridge initializes from `ANativeActivity`, copies and reads a length-delimited UTF-8 sample containing embedded NUL and non-ASCII characters, reports text presence, executes the sensitive-copy path, clears the clipboard with the documented API split, shuts down, and never injects or executes the pasted bytes.

The smoke receipt does **not** prove background/input-focus withholding, the Android 12 clipboard-read notification, the Android 13+ sensitive preview UI, or deliberately injected framework exceptions. Those remain separate device acceptance boundaries rather than being inferred from the host check.
