# SMS transport

This utility is the first real Android outbound transport for the existing
Idriç/Grease SMS foundation.

Its job is intentionally small:

```text
authorized due outbound event
  -> destination + exact message body
  -> Android SmsManager
  -> cellular SMS transport
```

It does **not** parse `REMIND`, schedule events, resolve principals, decide
authorization, handle `STOP`, replace the permanent Grease fake outbox, receive
SMS, become the default SMS application, or implement MMS/RCS.

The permanent fake outbox remains the deterministic provider-independent receipt.
This APK is an additional physical transport adapter for the same outbound
destination/body.

## Build

There is no Gradle or Kotlin build.

`build-apk.sh` uses the Android SDK's `aapt2`, `d8`, `zipalign`, and
`apksigner` directly. The package identity is:

```text
com.ashtrayarcher.smstransport
```

The APK deliberately contains DEX because `SmsManager` is an Android framework
API. Application SMS semantics remain outside that DEX boundary.

Like the other installable utilities in this repository, the build refuses to
invent a temporary signer. `ANDROID_KEYSTORE` must point at the persistent
public test keystore established by the stable-signing work.

## Permission boundary

Android declares `SEND_SMS` as a dangerous, hard-restricted permission. The
installer must allowlist the restricted permission before the app can hold it.

For the physical test path, adb's package installer is appropriate. Android's
package-manager shell grants requested runtime permissions with `-g` and,
unless `--restrict-permissions` is supplied, retains the install-time
allowlisting of restricted permissions.

The launchable `MainActivity` only reports/request the permission. It never
sends a text.

The actual `SendActivity` is exported only for the first device-test bridge and
requires the caller to hold `android.permission.DUMP`. That makes adb shell a
usable ingress without turning the APK into an unguarded exported SMS sender.
This is **not** yet the final same-device Grease IPC boundary.

## Physical test

`test-physical.sh` takes an exact built APK and an explicitly supplied
destination number. It:

1. replacement-installs the APK with `adb install -r -g`;
2. verifies that `SEND_SMS` is actually granted;
3. invokes the protected sender through adb shell with exact body `hey`;
4. records the adapter's request-submitted and Android sent-result log lines.

A successful Android sent result is still not a destination-handset receipt.
End-to-end physical acceptance additionally requires observing the exact `hey`
message on the destination phone and recording that observation against the
same source/APK revision.

If a carrier, SIM, device policy, or vendor build blocks sending, record that as
a physical transport result. Do not relabel a source/APK build as carrier
acceptance.

## Next boundary

After the first real carrier send is observed, the next slice is a secure
same-device bridge from Grease to this adapter. It should consume the same
destination/body produced by the existing outbound event while preserving the
fake outbox. Do not make `SendActivity` generally callable as a shortcut.
