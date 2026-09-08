# Repository style

Idriç-facing source in this repository follows the canonical Idriç guide in
[`isomorphisms/Idric/STYLE.md`](https://github.com/isomorphisms/Idric/blob/Idri%C3%A7/STYLE.md)
and its canonical intent examples:

- [railway](https://github.com/isomorphisms/Idric/tree/Idri%C3%A7/examples/intent/railway)
- [HTTP server](https://github.com/isomorphisms/Idric/tree/Idri%C3%A7/examples/intent/http_server)

This file records only repository-specific constraints. Do not duplicate the
canonical guide here.

## Keep each phone utility small and independently understandable

Each utility should remain independently buildable where practical. Its top
level should say what the phone user is trying to do before exposing Android,
JNI, native-window, clipboard, terminal, packaging, or other platform machinery.

Use the filesystem as the deep dive: keep user-facing purpose and semantic
operations near the top, and place platform-specific implementations beneath
them.

## Idriç owns Idriç-facing behavior

Where a utility is Idriç-owned, keep application semantics in Idriç rather than
letting a C/JNI or Android-framework boundary become the de facto application
model.

C/JNI, shell, and other host-language pieces should be narrow boundary adapters.
Do not duplicate business/application logic across the Idriç and boundary
layers. If a small mirror exists for integration or public compatibility, keep
its role explicit and prevent it from silently becoming the source of truth.

## Preserve native/mobile constraints explicitly

When a utility has a deliberate constraint such as native APK output or being
DEX-free, keep that constraint visible in the relevant top-level documentation
and acceptance checks rather than burying it inside build machinery.

Keep build and packaging mechanics below the utility's purpose. Do not make the
reader understand the Android toolchain to discover what the utility does.
