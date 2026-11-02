# Clipboard observer executable boundary

The candidate attempts automatic capture through a user-authorized Shizuku
API-13 shell process. Source compilation and DEX emission do not establish
listener delivery or access to clipboard content. Exact Android receipts must
separately establish each boundary below. Physical C67 and TAB_P10 execution
remains unverified without an authenticated endpoint.

| Boundary | Required identity and executable behavior |
| --- | --- |
| Acquisition | Actual Linux UID 2000; Context package `com.android.shell`; public `ClipboardManager.addPrimaryClipChangedListener` and immediate public clipboard read in its callback |
| System permissions | Shell holds `READ_CLIPBOARD_IN_BACKGROUND`, `ACCESS_CONTENT_PROVIDERS_EXTERNALLY` and `INTERACT_ACROSS_USERS_FULL`; successful method lookup alone does not establish permission |
| Private writer | App UID owns its private files and invokes the sole checked Idriç reducer; no shell access to app data files |
| Delivery authorization | Android provider permission, actual Binder caller UID 2000, private 256-bit credential, checked active automatic mode and fresh Shizuku authorization |
| Acknowledgement | Checked transition and synced atomic history commit complete before accepting a delivery; retry identities prevent duplicate insertion |

Shizuku's [pinned developer guide](https://github.com/RikkaApps/Shizuku-API/blob/a27f6e4151ba7b39965ca47edb2bf0aeed7102e5/README.md#userservice)
states that its user service is not a valid Android application process and
that Context content resolution does not work normally there. The companion
therefore uses the external-provider acquisition path in Android's own shell
`content` command. This path is confined to UID 2000, API 29, 34 or 35 and the
primary Android user. Other versions and user profiles expose unavailable
automatic capture; the explicit foreground mode remains available. This is an
implementation/qualification boundary, not evidence that another Android
version makes the feature impossible.

The adapter invokes framework methods through explicit JNI class/member
signatures. It never guesses Binder transaction numbers or changes hidden-API
policy in the app process. It acquires a fresh external reference for each
delivery, uses the app UID writer, then releases
the reference even when a call throws. Provider acquisition can start that
writer after ordinary process death.

| API | Framework call shape |
| --- | --- |
| 29 and 34 | Build a fresh shell context from `ContextImpl.mPackageInfo` using `ActivityThread.currentActivityThread()` and `ContextImpl.createAppContext(ActivityThread, LoadedApk)`; verify its ApplicationInfo UID 2000, package and operation package |
| 29 and 34 | `ActivityManager.getService()`; `IActivityManager.getContentProviderExternal(String authority, int userId, IBinder lifetime, String tag)`; `ContentProviderHolder.provider` |
| 29 | `IContentProvider.call(String callingPackage, String authority, String method, String credential, Bundle extras)` |
| 34 | `IContentProvider.call(AttributionSource source, String authority, String method, String credential, Bundle extras)`; source UID is the actual shell UID 2000 and package is `com.android.shell` |
| 29 and 34 | `IActivityManager.removeContentProviderExternalAsUser(String authority, IBinder lifetime, int userId)` in cleanup |

Primary source definitions are pinned to Android release tags:

API 35 uses the same context factory, external acquisition/release and
AttributionSource call shapes as the API 34 rows above. Its attribution
constructor retains the default device ID. This source agreement permits a
candidate implementation; only fresh API 35 execution can qualify it.

| Android 15 source (`android-15.0.0_r1`) | Blob |
| --- | --- |
| [IContentProvider](https://github.com/aosp-mirror/platform_frameworks_base/blob/android-15.0.0_r1/core/java/android/content/IContentProvider.java) | `ef8d0f9e18c81ebdd5b533c6b2dc35d5c4e01a55` |
| [IActivityManager](https://github.com/aosp-mirror/platform_frameworks_base/blob/android-15.0.0_r1/core/java/android/app/IActivityManager.aidl) | `ffb920b907abc178cf165ef70a7415fb963018e9` |
| [ContextImpl](https://github.com/aosp-mirror/platform_frameworks_base/blob/android-15.0.0_r1/core/java/android/app/ContextImpl.java) | `3b9a5d3b2cf2310a89e1e2bcbf0567171ec9423a` |
| [AttributionSource](https://github.com/aosp-mirror/platform_frameworks_base/blob/android-15.0.0_r1/core/java/android/content/AttributionSource.java) | `37f419d717c2f4dd1ee01391e68b756831a231f8` |
| [Shell permissions](https://github.com/aosp-mirror/platform_frameworks_base/blob/android-15.0.0_r1/packages/Shell/AndroidManifest.xml) | `04d30ed6f00108bd159e19f64bef79f3565e431b` |

| Source | API 29 blob | API 34 blob |
| --- | --- | --- |
| [IContentProvider, Android 10](https://github.com/aosp-mirror/platform_frameworks_base/blob/android-10.0.0_r47/core/java/android/content/IContentProvider.java) / [Android 14](https://github.com/aosp-mirror/platform_frameworks_base/blob/android-14.0.0_r1/core/java/android/content/IContentProvider.java) | `0427c2f52415c4c41b1f5b0e80b929555ba4a4a0` | `ef8d0f9e18c81ebdd5b533c6b2dc35d5c4e01a55` |
| [IActivityManager, Android 10](https://github.com/aosp-mirror/platform_frameworks_base/blob/android-10.0.0_r47/core/java/android/app/IActivityManager.aidl) / [Android 14](https://github.com/aosp-mirror/platform_frameworks_base/blob/android-14.0.0_r1/core/java/android/app/IActivityManager.aidl) | `c0c63555b10a95e3452173c648bf2728821bccc4` | `46260ea5e658692a7c7487d7194a572b32675fcc` |
| [Shell permissions, Android 10](https://github.com/aosp-mirror/platform_frameworks_base/blob/android-10.0.0_r47/packages/Shell/AndroidManifest.xml) / [Android 14](https://github.com/aosp-mirror/platform_frameworks_base/blob/android-14.0.0_r1/packages/Shell/AndroidManifest.xml) | `19eac7ccdb027a79221bb7bc0199f34f84d01a3b` | `ee9883b0b0afb6e19f74bb1a6b4c80aa890ce1b2` |

The [Android 14 content command](https://github.com/aosp-mirror/platform_frameworks_base/blob/android-14.0.0_r1/cmds/content/src/com/android/commands/content/Content.java),
blob `f0ac53014bd37640cd8d80d8a9c1c989346437c6`, supplies the acquisition,
attribution and cleanup pattern. Clipboard payloads never enter a command
argument, manager RPC, network request or log. Diagnostic traces contain only
numeric stages, flags and exception class names; exception messages are excluded.

[ContextImpl on API 29](https://github.com/aosp-mirror/platform_frameworks_base/blob/android-10.0.0_r47/core/java/android/app/ContextImpl.java),
blob `41a4fba0434cd2322e15d5bf92aa1a5cd67e290f`, and
[API 34](https://github.com/aosp-mirror/platform_frameworks_base/blob/android-14.0.0_r1/core/java/android/app/ContextImpl.java),
blob `82f4315d033e4486624baae02f17e9d900e30ad1`, define the same factory and
LoadedApk field. A package context created from a container retains that
container's operation package. The fresh factory has no container and derives
its identity from the actual shell LoadedApk. This does not alter package
ownership, UID, permissions or app-op settings.

The user-service control protocol retains tag `pasteboard-observer-v1` and
descriptor `com.isomorphisms.pasteboard.ClipboardObserver.v3`. Configuration
requires the app owner's actual Binder UID, a bounded Parcel with no file
descriptors, exactly one credential String and one positive capture-since Long.
The capture-since value is the synced private credential file's creation/write
time, retained across ordinary app restart and renewed before explicit Resume.
An identical credential/cutoff configuration retains a healthy registered
listener and its callback epoch. Configuration calls are serialized outside the
callback/queue mutex, so retrying a connection cannot invalidate a current read.
An acknowledged authenticated event restores connection state as well as its
heartbeat time. Changed credentials, explicit stops and actual listener failures
still require a new registration. These candidate-15 repairs require fresh
Android receipts; the earlier two-delivery failure remains negative evidence.
Shizuku's normal version-code
replacement restarts an old companion after an application upgrade. ARM32 APKs
request a 32-bit app_process; ARM64 and x86-64 APKs request a 64-bit process.

Pausing or selecting foreground capture rejects observer writes immediately in
the checked model. Explicitly resuming automatic capture saves a fresh private
credential before enabling writes; older queued credentials are discarded.
Ordinary app process restart keeps the credential and acknowledged history.
Callbacks older than the renewed session boundary are excluded. Public SDK
`ClipDescription.getTimestamp()` identifies the original copy time: Android's
later classification callbacks retain that same timestamp. The delivery ID
combines the companion session, this timestamp and a SHA-256 digest of the
length-delimited exact UTF-8 text and advisory origin. Neither the digest nor
the delivery ID is logged. The checked Idriç replay gate therefore accepts one
delivery for repeated metadata callbacks, while distinct texts copied in the
same millisecond remain distinct. Missing SDK timestamps fail closed.
Shizuku revocation/server death stops or disconnects capture. Pending companion
memory can be lost on a companion crash, and copies made during downtime cannot
be reconstructed. Shizuku may require user restart after a device reboot.

Independent acceptance must use real cross-application A/B/C copies and inspect
exact contents before Pasteboard gains focus. It must also reject actual wrong
caller/credential/permission controls and source-compiled mutants, exercise
pause, revocation, process death, reconnection and restart, and bind receipts to
the exact source, compiler, APK digest and environment. Until those receipts
pass, the corresponding controls remain NOT VERIFIED. At source
`2da8967cde5ee5433577f00f65f470958ed15d58`, run
[41359814484](https://github.com/Ashtray-Archer/utilities-android-phone-user/actions/runs/41359814484)
proves exact real A/B/C capture before refocus, permission/UID mutant controls,
pause, revocation, companion/server reconnection, foreground/manual fallback
and restart on API 29, 34 and 35. API 34/35 also pass actual framework expiration
and the complete visible/archive workflow. API 29's post-reboot UI setup fails
because its display override reset, so its spatial qualification remains failed
at that source. The next qualifier restores geometry and adds a real-shell
wrong-credential control with a separate compiled comparison-bypass mutant.
Those changed controls require fresh receipts. Neither physical target has
authenticated execution evidence.
