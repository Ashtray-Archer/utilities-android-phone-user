# Agent instructions

Before writing or reviewing Idriç in this repository, read:

1. [`STYLE.md`](STYLE.md)
2. the canonical [`isomorphisms/Idric/STYLE.md`](https://github.com/isomorphisms/Idric/blob/Idri%C3%A7/STYLE.md)
3. the canonical [railway](https://github.com/isomorphisms/Idric/tree/Idri%C3%A7/examples/intent/railway) and [HTTP server](https://github.com/isomorphisms/Idric/tree/Idri%C3%A7/examples/intent/http_server) intent examples
4. [`README.md`](README.md) and the README for the utility being changed

Do not copy the canonical style guide into this file. `STYLE.md` records local
constraints; the Idriç repository remains the source of truth for language-wide
style.

Work on a branch. Preserve each utility's user-facing purpose above Android,
JNI, C, shell, build, or packaging details. Keep boundary adapters narrow and do
not duplicate Idriç-owned application logic into them. Run the utility's
available build or acceptance checks before proposing a merge.
