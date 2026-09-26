# DEX + JNI plan

The format logic should have one semantics and two Android-facing compilation surfaces. Do not implement one mbox parser in DEX and another in native code.

## Boundary

**DEX side owns Android framework objects and UI:**

- Activity/lifecycle;
- system document picker;
- `content://` URI and permission handling;
- `ContentResolver` / `ParcelFileDescriptor` calls;
- list/detail presentation;
- cancellation and user-visible errors.

**Native side owns byte-oriented archive work:**

- mbox dialect detection/framing;
- checked byte offsets;
- raw-message index;
- header scanning;
- MIME decoding when added;
- search/index operations that do not require Android framework objects.

JNI is only the bridge. Keep Android object lookup and conversion there; keep mailbox semantics out of JNI glue.

This matches the existing repository direction in `android-clipboard`: narrow framework boundary, explicit ownership, ordinary byte buffers on the native side.

## Opening a file

Use Android's Storage Access Framework rather than pathname assumptions:

1. DEX launches `ACTION_OPEN_DOCUMENT` for a readable document.
2. Android returns a `content://` URI.
3. DEX opens it through `ContentResolver.openFileDescriptor(uri, "r")`.
4. Transfer an OS file descriptor through JNI with an explicit ownership rule.
5. Native code checks whether the descriptor is seekable.

Android document providers are allowed to back a read-only descriptor with a pipe/socket rather than a seekable regular file. Therefore the mbox core needs both:

- a sequential scanner that never requires seeking; and
- an optional seekable fast path for ordinary local files.

For a non-seekable provider, scan while copying to an app-private cache file if random-access message browsing is required after the initial pass. That copy is a runtime cache, not a new archive format.

Framework references:

- https://developer.android.com/guide/topics/providers/document-provider
- https://developer.android.com/reference/android/content/Intent#ACTION_OPEN_DOCUMENT

## Native archive representation

Keep the index compact and offset-based. One message record should initially need only fixed-size framing data plus references into the source:

```text
message_record
    raw_start
    raw_end
    separator_start
    separator_end
    header_start
    header_end
    body_start
    dialect
    flags
```

Do not copy every message body during the scan. Store byte spans and read/decode on demand. For streamed/cached input, offsets refer to the cache copy after the copy is complete.

Metadata such as subject/from/date can be indexed separately after boundaries are known.

## JNI surface

Keep calls coarse enough that a mailbox with thousands of messages does not produce thousands of tiny JNI transitions.

A useful first ABI shape is:

```text
open_archive(fd, options) -> archive_handle
scan_archive(archive_handle) -> scan_result
message_count(archive_handle) -> count
read_summary_page(archive_handle, first, count, output_buffer) -> rows_written
read_message(archive_handle, index, output_buffer/options) -> result
close_archive(archive_handle)
```

The concrete generated JNI names are backend details. The semantic contract should be specified independently of JNI symbol spelling.

Prefer packed/flat result buffers or page-sized transfers over building large Java object graphs in native code. Android's JNI guidance recommends minimizing both marshalling and the frequency of crossings.

JNI reference:

- https://developer.android.com/ndk/guides/jni-tips

## Registration and ownership

Prefer explicit native registration from `JNI_OnLoad` rather than relying on long exported `Java_package_Class_method` symbol names. Fail library initialization early if the generated DEX declarations and native table disagree.

Rules to make explicit in tests/docs:

- who owns and closes the passed file descriptor;
- archive handle lifetime;
- whether a call may block;
- cancellation behavior;
- buffer ownership;
- UTF-8/UTF-16 conversion policy;
- error-code stability across DEX/native builds.

Do not pass a `JNIEnv *` across threads. Store `JavaVM *` only if the native side must later attach a thread; preferably keep UI/asynchronous coordination on the DEX side and make native calls blocking at the bridge.

## Text boundary

mbox parsing begins as bytes. JNI should not turn the whole archive into `jstring`.

For displayed decoded text:

1. container parser finds raw byte spans;
2. message/MIME layer determines transfer encoding and charset;
3. decoded Unicode is converted at the UI boundary;
4. malformed source bytes produce an explicit replacement/diagnostic policy without altering the preserved raw archive.

The JNI boundary should use explicit byte lengths. Never depend on C NUL termination for message data.

## DEX backend obligations

The DEX lowering only needs a deliberately small Android surface for the first slice:

- generated Activity class;
- document-picker Intent construction;
- result URI extraction;
- `ContentResolver.openFileDescriptor`;
- native-library load;
- native method declarations/registration agreement;
- simple list/detail UI calls;
- lifecycle cleanup.

No handwritten Java/Kotlin parser is needed. The generated DEX should be inspectable against Android's published DEX format/instruction references:

- https://source.android.com/docs/core/runtime/dex-format
- https://source.android.com/docs/core/runtime/instruction-formats

## Native/JNI backend obligations

First native slice:

1. take ownership of a readable descriptor;
2. detect seekability;
3. scan one RFC-4155/mboxrd mailbox;
4. return exact message count and byte spans;
5. expose one page of header summaries;
6. close cleanly with no leaked descriptor/global refs.

Then add explicit `mboxo`, `mboxcl`, and `mboxcl2` behavior using the same fixture corpus.

## Acceptance ladder

### Host parser

- fixtures from `FORMAT.md`;
- exact raw offsets;
- no Android dependencies;
- malformed lengths and integer overflow rejected safely.

### JNI host compile

- generated/native declarations compile against `jni.h`;
- registration table signatures match;
- bridge has explicit ownership tests where possible.

### DEX structural

- generated `.dex` passes format/verifier checks;
- expected Activity and native declarations are present;
- no Java/Kotlin compiler is required to produce the DEX backend output.

### Android emulator/device

1. OPEN — picker opens a real mbox document;
2. SCAN — expected message count is shown;
3. LIST — first/last subjects/senders agree with a host oracle;
4. BODY — selected text message agrees with the host oracle;
5. LARGE — mailbox is scanned without loading the whole file into managed memory;
6. PIPE — a non-seekable provider path is handled by sequential scan/cache rather than failing because `lseek` is unavailable;
7. CLOSE — descriptor/native archive state is released after Activity teardown.

The host parser is the oracle. DEX/JNI acceptance proves Android transport and lowering, not a second definition of mbox semantics.
