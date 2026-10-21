# Email Large Files — first transport experiment

**Status:** local executable experiment, **not** the finished service, not accepted
for Internet exposure or Android release. No registration, payment, email
integration, relay, SSH/SFTP path, or cross-device installation is implemented.

## Product intent

Send 100 GB through multi-terabyte files to people found by an email address.
Email carries a small authenticated invitation; the file bytes are transferred
directly between machines when possible, with an optional rendezvous/relay path
when NAT or firewalls make direct reachability impossible. The recipient should
ideally need no account or installation. Avoid a permanent cloud copy; make
one-time delivery affordable (postage-stamp-sized price) or approximately $5/year
for repeated sends. An expired or revoked offer must not keep working.

## What actually executes here

A temporary HTTP/1.1 bearer-token download endpoint **bound only to
127.0.0.1**, paired with a receiving script. The endpoint uses bounded 64 KiB
reads (`pread`) and 64-bit file offsets; it implements single-byte-range GET,
HEAD, unsatisfiable-range responses, one-hour default offer lifetime and
source-file change rejection between requests. It does not buffer a file in
memory or copy the file to another storage service. The recipient script
supports restart from a partial file, verifies a caller-supplied SHA-256 before
publishing the destination and refuses destination overwrite.

**Do not email the `127.0.0.1` URL.** It works on the sender's own computer only.
Do not expose this listener on the public Internet or treat a plain HTTP token
as a production public link. Public use needs an audited TLS endpoint,
recipient authorization, safe external reachability (and optional relay),
expiry/revocation, and verified integrity delivery.

This provisional C transport adapter is not an Idriç application model and is
not an accepted ICK/NDK build. The authoritative higher-level offer/identity,
state, receipt and pricing semantics should be Idriç-owned; the native boundary
must remain narrow. Local host compilation demonstrates experiment behavior,
**not** ICK qualification or AICI acceptance.

## Repeat the local experiment (Linux)

Run from any working directory:

```sh
bash /absolute/path/to/email-large-files/test-local.sh
```

The script requires a C compiler, `bash`, `curl`, `sha256sum`, `truncate`, and
ordinary Unix core utilities. It compiles in an isolated temporary directory,
spins up a loopback-only sender and checks full download, HTTP HEAD, ranged
requests, reject cases, mutation rejection, SHA-256, partial resume, and a
six-byte range from a **sparse file larger than 4 GiB**. It does not send 4 GiB
on the wire, prove constant memory via profiling, or exercise two devices.

For a manual local-only sample:

```sh
cc -std=c11 -D_FILE_OFFSET_BITS=64 -O2 -Wall -Wextra -Werror -pedantic \
    /absolute/path/to/email-large-files/serve.c -o /tmp/elf-local-serve
/tmp/elf-local-serve /absolute/path/to/a/regular/file
```

The program prints a localhost URL, the safe display name, size, and TTL.
Compute `sha256sum` of the source independently. The receiver can use:

```sh
bash /absolute/path/to/email-large-files/receive.sh \
     'http://127.0.0.1:PORT/f/TOKEN' '64-lowercase-hex-sha256' /absolute/output/path
```

Do not paste the printed token into public logs. A token is a temporary
secret, not evidence that an identity was authenticated.

## Still required for an actual delivery product

1. Typed offer envelope: sender identity, verified recipient address, file name,
   expected length, content digest, time limit, permissions, and revocation.
2. User-facing email invitation and one-click recipient access, without placing
   payload bytes in SMTP or requiring the recipient to create an account.
3. Internet-routable encrypted transport, rendezvous/NAT handling, optional
   ephemeral relay; SSH/SFTP for systems already able to connect directly.
4. Real resumable journal + source snapshot identity, interruption/restart,
   integrity verification and delivery receipt, with no overwrite or duplicates.
5. Exact multi-TB offset, bounded-memory and crash testing; real different-host
   and physical A1/C67 runs; maintained ICK/NDK build and independent AI-CI gate.
6. Metering, pricing, abuse prevention and cleanup that retain no permanent
   copy of transferred content by default.

This is a transport experiment, not a substitute for these open requirements.
