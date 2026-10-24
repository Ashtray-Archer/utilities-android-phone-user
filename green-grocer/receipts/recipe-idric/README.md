# Exact-source recipe evidence

Application parent: ac1398cc25a1ed56c8221cfb08da73770975441f.
Frontend: 740bbf2b1d9e811fd00c719c3473eede17ceea3b.
Backend parent: de29100e9b24b5bbc85aca7f77cd3b08c0934917, with the parser-only
Placement.idr binding renames recorded by the canonical backend continuation.
Source hashes bind these results independently of the eventual documentation
commit. Executed compiler hashes include the actual Chez images, not only
their launchers. Host was this disposable Linux x86_64 container.

`host-execution.txt`: 37 positive checks. `mutant-*.txt`: compiled implementation
mutations rejected by their intended assertions. `runtime-host.txt`: actual
probe outputs. `dex-compile.txt` and `dex-result.tsv`: full recipe runtime fails
at Data.Nat.div', exit 1, no recipes.dex. `saved-recipe.dex` and its checked ANF
and independent baksmali disassembly: isolated save-transition emission only.

Host verification invocation:

```text
grease green-grocer/_/check-recipes.grease ABS_REPOSITORY ABS_IDRIC ABS_NEW_OUTPUT
grease green-grocer/_/check-recipe-dex.grease ABS_REPOSITORY ABS_IDRIC ABS_CURRENT_DEX_DRIVER ABS_NEW_OUTPUT
```

The paths above are argument schemas, not pasteable commands. The scripts
validate absolute paths and print the same interfaces without arguments.
No Java source, generated C, alternative cart implementation, Android runtime,
current C-oracle execution, live retailer access, APK or physical-device result
is supplied by this evidence. The previous sample APKs remain historical.
