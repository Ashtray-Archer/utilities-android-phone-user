# Exact recipe file boundary

`RecipeStorage.idric` implements a versioned length-delimited text format for a
single saved recipe. It retains identities, names, serving counts, dimensions,
exact rational quantities, preparation text, acceptable substitution identities,
source links and source notes. The field lengths count decoded characters.
UTF-8 bytes and atomic file replacement belong to the platform adapter.

Twenty-four executed host checks pass. The minimal valid file is an independent
literal, and decoded ingredient/source fields are checked against independent
values. The tests cover significant whitespace, newlines, delimiters, Unicode,
leading-zero identities, values above uint64, invalid numeric fields, zero
servings/denominators, unsupported schemas, truncation and trailing data.
Restoring invalid data preserves existing recipes and review. Restoring a valid
recipe follows the existing save transition and revokes review.

Three changed codec implementations compile and execute, then fail their
intended assertion: dropped source notes, lost rational denominators, and
accepted trailing fields. A first mutation used an unsupported expression type
ascription and was repaired before its result was counted. The initial source
imported nonexistent `Data.Char`; removing that import uses the actual Prelude
`ord` primitive. Neither source error is an Android compiler limitation.

The canonical driver built from android-NDK
`6ab4818219efa7ea5eda137ca6b76976eb2a4d42`, using Idriç frontend
`2bce7677f513d711875e978bd6f5ad28b9e54d58`, emits the complete storage test command
as direct DEX. Independent baksmali disassembly succeeds. These facts are
emission/decoding evidence only. The receipt hashes bind source and output.

Host reproduction uses `green-grocer/_/check-recipe-storage.grease` with absolute
application, pinned frontend and fresh output paths. The maintained recipe ART
workflow also builds the storage command and its three mutants, then requires
the same actual assertion results on API 29/34. Consult the current run for
runtime status; workflow text is not a PASS receipt.

Actual filesystem I/O/reopening, the Android recipe UI, installable recipe APKs,
retailer operations and physical A1/C67 acceptance remain outstanding. This
codec does not persist product observations, review approvals or credentials.
