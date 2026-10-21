# Secure-delivery continuation: reviewed failures and execution boundary

Assignment: [Flexible Pipes #242](https://github.com/isomorphisms/flexible-pipes/issues/242).
Continue the existing `email-large-attachment` branch and
[utilities-android-phone-user PR #82, “Start Email Large Files with resumable local transfer experiment”](https://github.com/Ashtray-Archer/utilities-android-phone-user/pull/82).

Reviewed implementation revision: `512ce6a477c55ff193717b7f1afa4bc418f12b82`.
This record is source-review evidence only. No finding below has yet been
reproduced through the maintained build/FP/independent AI-CI route. No new
transfer implementation, deployment, invitation send, acceptance, or release
is claimed by this change.

## Concrete source findings

| Finding | Source evidence | Required regression |
| --- | --- | --- |
| Expiry does not govern an accepted connection | `serve.c` checks its deadline only in the outer accept loop. `handle_request` and `write_fully` do not receive or check that deadline. The socket timeout limits one blocked operation, not total request/transfer lifetime. | Accept a connection before expiry, complete its headers after expiry, and attempt a throttled transfer across expiry. Require the declared expiry policy to stop authorization and continuing payload transmission, with a documented bounded cancellation interval. Test wall-clock rollback as well as ordinary expiry. |
| One slow client occupies the entire sender | Requests run serially. A client can send header bytes within successive five-second receive timeouts. | A slow unauthenticated client must hit an absolute header deadline; a second legitimate recipient must remain serviceable within a declared limit. Bound connections, memory, file descriptors, and worker count. |
| Publication is not crash-durable | `receive.sh` hashes, hard-links and removes the partial. It never synchronizes file data or the parent directory, and stores no durable offer binding or completion record. | Inject process termination after each checkpoint and publication step; additionally test the claimed storage-crash model. Restart must recognize a verified published destination, retain the same offer identity, and neither replace unrelated content nor report unsynchronized data as durable. Process-kill tests alone cannot prove power-loss durability. |
| Partial-file checks race later path opens | `receive.sh` checks for a symlink before separate `curl`, hash and link operations. No descriptor lock binds those operations to one inode. | In a directory writable by an adversary, exchange the partial path after inspection. Require safe refusal without writing or publishing another file. Require exclusive ownership of a receiver journal during simultaneous resumes. |
| Source identity is checked only before streaming | `serve.c` compares file metadata once per request, then reads a mutable open source without further identity checks. | Modify the source during transmission and between resumed requests. Reject mismatched content and prevent a verified completion receipt. A metadata tuple is not an immutable content snapshot. |
| Final integrity is local and depends on a supplied digest | `receive.sh` compares a user-supplied SHA-256. No authenticated offer binds that value to sender, recipient, length or lifetime; the browser recipient has no equivalent verification path. | Alter each authenticated offer field and payload independently. Require verification of exact length and digest before publication/completion; browser download completion alone must not produce an integrity PASS. |

The receiver's existing hash check can detect corrupted combined bytes. The
findings above do not claim that such bytes pass its SHA-256 check. The missing
properties are authenticated expected identity, safe storage operations,
durability, lifecycle enforcement and a verified browser delivery path.

## Security and delivery acceptance that must remain separate

1. Bind offer ID, authenticated sender, intended recipient, safe display name,
   exact 64-bit byte length, content digest, expiry, permissions and revocation
   generation. A random bearer token establishes capability possession, not
   the identity of its holder. Do not invent an authenticated-email claim from
   the recipient string supplied by the sender.
2. Invitations contain metadata and an HTTPS link only. Reject header
   injection. Email-scanner GET/HEAD requests must not accept an offer,
   consume a one-time credential, or mark delivery complete. Actual email
   delivery and link opening require separate evidence from message creation.
3. Use maintained TLS with hostname/certificate verification. Keep payload
   and credentials out of logs, referrers, third-party assets and caches.
   Do not expose the experimental HTTP listener while implementing this.
4. Exercise separate sender and recipient machines. Test direct reachability,
   blocked inbound ports, address change and double NAT. An outbound sender
   tunnel to an authenticated ephemeral relay is a candidate traversal path,
   not an implemented feature. A relay that terminates TLS can see plaintext;
   describe that trust boundary honestly or add separately verified end-to-end
   payload encryption. Do not label ordinary forwarding as end-to-end encryption.
5. Preserve SSH/SFTP interoperability through existing cryptographic libraries
   or verified executables, including host-key verification. The recipient's
   browser-only path must not silently acquire an SSH installation requirement.
6. Persist revocation and expiry across restart. Check cancellation during
   streaming as well as at request admission. Revocation cannot retract bytes
   already received; declare and measure its maximum enforcement delay.
7. Bind resumable offsets to the authenticated offer and immutable source
   identity. Synchronize payload/checkpoint ordering, verify partial state on
   restart, use exclusive journals and publish without overwriting. Cover full
   disk, truncated journals, duplicate requests, server restart, receiver
   restart, replay and interrupted completion acknowledgement.
8. Measure an actual large payload over the wire, elapsed time, sender and
   receiver peak RSS, descriptors, disk usage, and application/relay byte
   counts. Keep sparse multi-TB offset probes as a separate test category.
   Include sizes around 2^31, 2^32 and 2^40 without narrowing arithmetic.
9. Record direct and relayed byte counts separately, retry overhead, relay
   storage byte-hours, provider billing units and the dated tariff used for
   estimates. No payment or postage-stamp/$5-year viability claim is supported
   by the current experiment.

AI-CI must independently exercise the actual producer executable and reject
deliberately broken expiry, identity, integrity and resumption cases. A leaf
repository assertion that its own fixtures passed is insufficient. FP must
retain exact producer, toolchain and verifier revisions plus actual outputs.

## Observed execution boundary in this continuation

The source checkout is available on a disposable Ubuntu 24.04 x86_64 host.
No installed `gh`, `grease`, `idric`, `ithon`, `ike`, `ick`, `catfood`, `fp`
or `flexible-pipes` entrypoint was found in the current executable search path.
No SSH credentials or second accessible execution machine were established.
This describes this host only; it is not evidence that those programs or
machines are unavailable elsewhere.

The existing FP hosted `run-pipeline.yml` can select checked-in pipelines via
workflow dispatch or a request committed to `pipeline-requests`. No Email Large
Files pipeline was found in the inspected current pipeline listing. Its
workflow alone is not evidence of an ELF dispatch or independent acceptance.
Do not execute the repository's generic host-compiler experiment as an
undeclared substitute for the required maintained build route.

Next executable boundary: establish the existing pinned FP/Kitchen/Ike build
environment and independent AI-CI runner, then implement and reproduce the
cases above on this same feature branch. External delivery additionally needs
an available test endpoint and second machine; no public deployment or paid
infrastructure commitment has been made.

**Overall state: BLOCKED; secure product implementation INCOMPLETE.**
