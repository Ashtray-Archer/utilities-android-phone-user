# Agent instructions

Apply the shared evidence, script-delivery, and acceptance guardrails in
`isomorphisms/ai-ci/AGENTS.md`.

Before writing or reviewing Idriç in this repository, read:

1. [`STYLE.md`](STYLE.md)
2. the canonical [`isomorphisms/Idric/STYLE.md`](https://github.com/isomorphisms/Idric/blob/Idri%C3%A7/STYLE.md)
3. the canonical [railway](https://github.com/isomorphisms/Idric/tree/Idri%C3%A7/examples/intent/railway) and [HTTP server](https://github.com/isomorphisms/Idric/tree/Idri%C3%A7/examples/intent/http_server) intent examples
4. [`README.md`](README.md) and the README for the utility being changed

Do not copy the canonical style guide into this file. `STYLE.md` records local
constraints; the Idriç repository remains the source of truth for language-wide
style.

## Android APK update identity

All installable Android utilities in this repository must use a persistent test
signer and nondecreasing version code. Never generate a fresh signing key in a
temporary build directory. CI and release/install paths must preserve the same
package name and test signer so a new APK can replace the installed one without
an uninstall. Treat any package-name or signer change as an explicit migration,
not installation cleanup.

## SMS transport evidence boundary

`sms-transport` is outbound-only in its first slice. A source check, APK build,
signature check, installation, launch, or successful `SmsManager` call is not
proof that another physical handset received the text. A physical receipt must
name the exact APK/source revision and separately record the Android send result
and observation of the exact message on the destination handset.

Do not add inbound `RECEIVE_SMS`, default-SMS-role ownership, command parsing,
scheduling, principal resolution, or authorization semantics to the outbound
transport merely to make the first carrier test convenient. Those meanings stay
with Idric-Net and Grease.

The first test ingress to `SendActivity` is deliberately restricted to callers
holding `android.permission.DUMP`, which includes the adb shell on ordinary
debug/test-device workflows and excludes normal third-party apps. Do not weaken
that component to an unguarded exported SMS sender. A future same-device Grease
bridge needs an explicit IPC/security design rather than broadening this test
ingress.

Work on a branch. Preserve each utility's user-facing purpose above Android,
JNI, C, shell, build, or packaging details. Keep boundary adapters narrow and do
not duplicate Idriç-owned application logic into them. Run the utility's
available build or acceptance checks before proposing a merge.

Whenever giving the human a script or command block, assume `$PWD` is arbitrary.
Resolve repository and file paths from the script's own location, an explicit
project location, or a discovered repository root, and perform any required
`cd` inside the script. Never require the human to `cd` first or rely on relative
paths against their current working directory.

When a phone-side script, executable, APK, or CI artifact is ready for the human
to exercise, make GitHub the delivery path and provide a self-contained script
or command that fetches it from the repository, release, or Actions artifact.
Do not rely on chat attachment downloads unless the human explicitly asks for
one.
