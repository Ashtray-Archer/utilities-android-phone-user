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

## Native boundary

There is no Java, Kotlin, Gradle, or application DEX layer.

The APK uses `android.app.NativeActivity` from the Android framework and a small
C shared library. That library calls `SmsManager.sendTextMessage` through JNI.
The framework remains the native Android SMS boundary; the C code is only the
narrow adapter needed to reach it.

The first test ingress is deliberately restricted to callers holding
`android.permission.DUMP`. On ordinary device-test workflows that includes adb
shell and excludes normal third-party applications. The activity reads only
three intent extras:

```text
destination
body
request_id
```

A future same-device Grease bridge needs an explicit IPC/security design rather
than weakening this exported test ingress.

## Build

`build-apk.sh` uses the Android NDK and SDK tools directly. It builds
`libsms_transport.so` for:

```text
armeabi-v7a
arm64-v8a
x86_64
```

and packages them into an APK with stable package identity:

```text
com.ashtrayarcher.smstransport
```

Like the other installable utilities in this repository, the build refuses to
invent a temporary signer. `ANDROID_KEYSTORE` must point at the persistent
public test keystore established by the stable-signing work.

## Permission boundary

The APK requests only `SEND_SMS` for SMS. It deliberately does not request
`RECEIVE_SMS`.

The physical-test path installs with `adb install -r -g` and verifies from
package-manager state that `SEND_SMS` is actually granted before attempting a
carrier send.

## Physical test

The Actions artifact contains the exact APK, build receipt, this README, and
`test-physical.sh`.

Run the test script from a machine or Android device that already controls the
target phone through adb. The destination is prompted locally so it does not
need to be pasted into chat or stored in shell history.

The script:

1. replacement-installs the exact APK;
2. verifies `SEND_SMS` is actually granted;
3. starts the protected native activity with exact body `hey`;
4. requires the native JNI adapter to report that
   `SmsManager.sendTextMessage` accepted the request;
5. leaves end-to-end acceptance pending until the destination handset is
   observed receiving exact body `hey`.

The JNI call returning without an Android exception is not itself proof of
carrier delivery. The destination-handset observation is the physical receipt.

## Next boundary

After the first real carrier send is observed, the next slice is a secure
same-device bridge from Grease to this adapter. It should consume the same
destination/body produced by the existing outbound event while preserving the
fake outbox.
