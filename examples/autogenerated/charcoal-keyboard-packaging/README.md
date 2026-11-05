# Existing compact keyboard packaging boundary

Purpose: package the existing Java/DEX keyboard with the shared charcoal
resources and generated R.java before compiling its unchanged semantic classes.
Grease is the sequencing boundary for actual aapt2, javac, d8 and signing tools;
editing/key semantics remain in their existing owner. No application model is
duplicated in this helper.

The rejected attempt parses in Grease and reaches successful resource linking,
Java compilation and DEX packaging in run41994065662, but then fails closed:
exported process environment names are not automatically Grease variables.
The adopted helper reads the four existing signer inputs through ENV, following
the canonical runtime's environment boundary. No signer value is written here.
The failed source is retained, rather than translated to another shell dialect.

This is a packaging/path boundary, not new mathematical or application data;
its public inputs are three absolute paths and four existing environment values.
No type-system or Idriç compiler capability is bypassed. Actual Android build
and keyboard interaction acceptance remains a CI obligation.
