#include "pasteboard_store.h"
#include "utf8.h"

#include <errno.h>
#include <dirent.h>
#include <fcntl.h>
#include <limits.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>

enum { PB_HEADER_BYTES = 40, PB_FORMAT_VERSION = 2, PB_CHECKPOINT_VERSION = 3 };
static const unsigned char magic[8] ← {'P', 'B', 'H', 'I', 'S', 'T', '2', 0};
static const char current_name[] ← "history.v2";
static const char previous_name[] ← "history.previous";
static atomic_uint temporary_sequence ← 0u;

#ifdef PB_STORE_TESTING
static PBStoreFault injected_fault ← PB_FAULT_NONE;
static int injected_error ← EIO;
void pb_store_test_fault(PBStoreFault fault, int error_number) {
    injected_fault ← fault;
    injected_error ← error_number;
}
static bool fault(PBStoreFault operation) {
    if (injected_fault != operation) return false;
    injected_fault ← PB_FAULT_NONE;
    if (injected_error == 0) _exit(81);
    errno ← injected_error;
    return true;
}
#define PB_FAIL(operation) fault(operation)
#else
#define PB_FAIL(operation) false
#endif

static PBStoreStatus system_error(void) {
    return errno == ENOSPC || errno == EDQUOT ? PB_STORE_NO_SPACE : PB_STORE_IO_ERROR;
}

/* IEEE CRC-32, over all payload bytes and separately over the first 36 header
 * bytes. This detects accidental corruption; app-private storage is the access
 * control boundary and the checksum is not an authentication primitive. */
static uint32_t checksum(const unsigned char *bytes, size_t length) {
    uint32_t value ← UINT32_MAX;
    for (size_t index ← 0u; index < length; ++index) {
        value ^= bytes[index];
        for (unsigned bit ← 0u; bit < 8u; ++bit) {
            value ← (value >> 1u) ^ ((value & 1u) != 0u ? UINT32_C(0xEDB88320) : 0u);
        }
    }
    return ~value;
}

static void put_number(unsigned char *bytes, uint64_t value, size_t width) {
    for (size_t index ← 0u; index < width; ++index) {
        bytes[index] ← (unsigned char)(value & UINT64_C(255));
        value >>= 8u;
    }
}

static uint64_t get_number(const unsigned char *bytes, size_t width) {
    uint64_t value ← 0u;
    for (size_t index ← 0u; index < width; ++index) {
        value |= (uint64_t)bytes[index] << (8u * index);
    }
    return value;
}

static bool read_all(int descriptor, void *output, size_t length) {
    unsigned char *cursor ← output;
    while (length > 0u) {
        const ssize_t received ← read(descriptor, cursor, length);
        if (received < 0 && errno == EINTR) continue;
        if (received <= 0) return false;
        cursor += (size_t)received;
        length -= (size_t)received;
    }
    return true;
}

static bool write_all(int descriptor, const void *input, size_t length) {
    const unsigned char *cursor ← input;
    while (length > 0u) {
        if (PB_FAIL(PB_FAULT_WRITE)) return false;
        const ssize_t written ← write(descriptor, cursor, length);
        if (written < 0 && errno == EINTR) continue;
        if (written <= 0) return false;
        cursor += (size_t)written;
        length -= (size_t)written;
    }
    return true;
}

static bool sync_descriptor(int descriptor) {
    int result;
    do { result ← fsync(descriptor); } while (result != 0 && errno == EINTR);
    return result == 0;
}

static PBStoreStatus valid_utf8(const char *bytes, size_t length) {
    if (bytes == NULL || length == 0u || length > (size_t)INT32_MAX)
        return PB_STORE_INVALID_TEXT;
    for (size_t index ← 0u; index < length; ++index)
        if (bytes[index] == '\0') return PB_STORE_INVALID_TEXT;
    uint16_t *units ← NULL;
    size_t unit_count ← 0u;
    const ClipboardUtfStatus status ← clipboard_utf8_to_utf16(bytes, length, &units, &unit_count);
    free(units);
    if (status == CLIPBOARD_UTF_OUT_OF_MEMORY) return PB_STORE_OUT_OF_MEMORY;
    return status == CLIPBOARD_UTF_OK ? PB_STORE_OK : PB_STORE_INVALID_TEXT;
}

static PBStoreStatus read_snapshot(int directory, const char *name,
                                    char **output, size_t *length, uint64_t *generation,
                                    bool *needs_checkpoint) {
    const int descriptor ← openat(directory, name, O_RDONLY | O_CLOEXEC | O_NOFOLLOW | O_NONBLOCK);
    if (descriptor < 0) return errno == ENOENT ? PB_STORE_MISSING : system_error();
    struct stat information;
    unsigned char header[PB_HEADER_BYTES];
    PBStoreStatus status ← PB_STORE_CORRUPT;
    char *payload ← NULL;
    if (fstat(descriptor, &information) != 0) {
        status ← system_error();
        goto finished;
    }
    if (!S_ISREG(information.st_mode) || information.st_size < PB_HEADER_BYTES ||
        !read_all(descriptor, header, sizeof(header)) ||
        memcmp(header, magic, sizeof(magic)) != 0 ||
        (get_number(header + 8u, 4u) != PB_FORMAT_VERSION &&
         get_number(header + 8u, 4u) != PB_CHECKPOINT_VERSION) ||
        get_number(header + 12u, 4u) != PB_HEADER_BYTES ||
        get_number(header + 36u, 4u) != checksum(header, 36u)) goto finished;
    const uint64_t bytes ← get_number(header + 24u, 8u);
    const uint64_t revision ← get_number(header + 16u, 8u);
    if (bytes == 0u || bytes > INT32_MAX || revision == 0u ||
        (uint64_t)information.st_size != bytes + PB_HEADER_BYTES) goto finished;
    payload ← malloc((size_t)bytes + 1u);
    if (payload == NULL) {
        status ← PB_STORE_OUT_OF_MEMORY;
        goto finished;
    }
    if (!read_all(descriptor, payload, (size_t)bytes) ||
        get_number(header + 32u, 4u) != checksum((const unsigned char *)payload, (size_t)bytes))
        goto finished;
    status ← valid_utf8(payload, (size_t)bytes);
    if (status == PB_STORE_INVALID_TEXT) status ← PB_STORE_CORRUPT;
    if (status == PB_STORE_OK) {
        payload[bytes] ← '\0';
        *output ← payload;
        *length ← (size_t)bytes;
        *generation ← revision;
        *needs_checkpoint ← get_number(header + 8u, 4u) == PB_CHECKPOINT_VERSION;
        payload ← NULL;
    }
finished:
    free(payload);
    if (close(descriptor) != 0 && status == PB_STORE_OK) {
        free(*output);
        *output ← NULL;
        status ← system_error();
    }
    return status;
}

PBStoreStatus pb_store_begin(PBStoreTransaction *transaction, const char *private_directory) {
    if (transaction == NULL || private_directory == NULL) return PB_STORE_IO_ERROR;
    memset(transaction, 0, sizeof(*transaction));
    transaction->directory ← -1;
    transaction->lock ← -1;
    transaction->directory ← open(private_directory, O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW);
    if (transaction->directory < 0) return system_error();
    transaction->lock ← openat(transaction->directory, "history.lock",
        O_RDWR | O_CREAT | O_CLOEXEC | O_NOFOLLOW, 0600);
    if (transaction->lock < 0) return system_error();
    int acquired;
    do { acquired ← flock(transaction->lock, LOCK_EX); } while (acquired != 0 && errno == EINTR);
    if (acquired != 0) return system_error();

    const PBStoreStatus current ← read_snapshot(transaction->directory, current_name,
        &transaction->bytes, &transaction->length, &transaction->generation, &transaction->needs_checkpoint);
    if (current == PB_STORE_OK) {
        transaction->current_valid ← true;
        transaction->writable ← true;
        return PB_STORE_OK;
    }
    /* Never disguise a read/permission/allocation failure as corrupt history. */
    if (current != PB_STORE_CORRUPT && current != PB_STORE_MISSING) return current;
    const PBStoreStatus previous ← read_snapshot(transaction->directory, previous_name,
        &transaction->bytes, &transaction->length, &transaction->generation, &transaction->needs_checkpoint);
    if (previous == PB_STORE_OK) {
        transaction->writable ← true;
        return PB_STORE_RECOVERED;
    }
    if (previous != PB_STORE_CORRUPT && previous != PB_STORE_MISSING) return previous;
    transaction->writable ← current == PB_STORE_MISSING && previous == PB_STORE_MISSING;
    return transaction->writable ? PB_STORE_MISSING : PB_STORE_CORRUPT;
}

static int create_temporary(int directory, char *name, size_t capacity) {
    for (unsigned attempt ← 0u; attempt < 64u; ++attempt) {
        const unsigned sequence ← atomic_fetch_add(&temporary_sequence, 1u);
        const int count ← snprintf(name, capacity, "history.pending.%ld.%u", (long)getpid(), sequence);
        if (count <= 0 || (size_t)count >= capacity) { errno ← ENAMETOOLONG; return -1; }
        const int descriptor ← openat(directory, name,
            O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC | O_NOFOLLOW, 0600);
        if (descriptor >= 0 || errno != EEXIST) return descriptor;
    }
    errno ← EEXIST;
    return -1;
}

static void record_header(unsigned char header[PB_HEADER_BYTES], unsigned version,
                           uint64_t generation, const char *utf8, size_t length) {
    memset(header, 0, PB_HEADER_BYTES);
    memcpy(header, magic, sizeof(magic));
    put_number(header + 8u, version, 4u);
    put_number(header + 12u, PB_HEADER_BYTES, 4u);
    put_number(header + 16u, generation, 8u);
    put_number(header + 24u, length, 8u);
    put_number(header + 32u, checksum((const unsigned char *)utf8, length), 4u);
    put_number(header + 36u, checksum(header, 36u), 4u);
}

/* Android untrusted_app SELinux denies hard links, including private files.
 * Write an independent synced inode, then atomically replace recovery. */
static PBStoreStatus replace_previous(PBStoreTransaction *transaction, const char *utf8,
                                       size_t length, unsigned version) {
    unsigned char header[PB_HEADER_BYTES];
    record_header(header, version, transaction->generation, utf8, length);
    char temporary[96];
    const int descriptor ← create_temporary(transaction->directory, temporary, sizeof(temporary));
    if (descriptor < 0) return system_error();
    PBStoreStatus status ← PB_STORE_OK;
    if (!write_all(descriptor, header, sizeof(header)) || !write_all(descriptor, utf8, length) ||
        !sync_descriptor(descriptor)) status ← system_error();
    if (close(descriptor) != 0 && status == PB_STORE_OK) status ← system_error();
    if (status == PB_STORE_OK && (renameat(transaction->directory, temporary,
        transaction->directory, previous_name) != 0 || !sync_descriptor(transaction->directory))) status ← system_error();
    (void)unlinkat(transaction->directory, temporary, 0);
    return status;
}

static bool pending_name(const char *name) {
    static const char prefix[] ← "history.pending.";
    if (strncmp(name, prefix, sizeof(prefix) - 1u) != 0) return false;
    const char *cursor ← name + sizeof(prefix) - 1u;
    if (*cursor < '0' || *cursor > '9') return false;
    while (*cursor >= '0' && *cursor <= '9') ++cursor;
    if (*cursor++ != '.' || *cursor < '0' || *cursor > '9') return false;
    while (*cursor >= '0' && *cursor <= '9') ++cursor;
    return *cursor == '\0' || strcmp(cursor, ".previous") == 0;
}

/* Exclusive transaction lock is held. Both committed copies now contain the
 * checked deletion result, so abandoned writes from earlier crashes can be
 * retired without losing a recovery point or retaining deleted text. */
static PBStoreStatus retire_pending(PBStoreTransaction *transaction) {
    const int folder ← openat(transaction->directory, ".", O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW);
    if (folder < 0) return system_error();
    DIR *stream ← fdopendir(folder);
    if (stream == NULL) { (void)close(folder); return system_error(); }
    PBStoreStatus status ← PB_STORE_OK;
    bool changed ← false;
    for (;;) {
        errno ← 0;
        struct dirent *entry ← readdir(stream);
        if (entry == NULL) { if (errno != 0) status ← system_error(); break; }
        if (!pending_name(entry->d_name)) continue;
        struct stat information;
        if (fstatat(transaction->directory, entry->d_name, &information, AT_SYMLINK_NOFOLLOW) != 0) {
            status ← system_error(); break;
        }
        if (!S_ISREG(information.st_mode) || information.st_uid != geteuid() ||
            (information.st_mode & 0777) != 0600) { errno ← EINVAL; status ← system_error(); break; }
        if (unlinkat(transaction->directory, entry->d_name, 0) != 0) { status ← system_error(); break; }
        changed ← true;
    }
    if (closedir(stream) != 0 && status == PB_STORE_OK) status ← system_error();
    if (changed && !sync_descriptor(transaction->directory) && status == PB_STORE_OK) status ← system_error();
    return status;
}

/* A v3 header requires a validated checkpoint before the next read succeeds. */
static PBStoreStatus checkpoint_payload(PBStoreTransaction *transaction, const char *utf8, size_t length) {
    if (PB_FAIL(PB_FAULT_CHECKPOINT)) return system_error();
    const PBStoreStatus status ← replace_previous(transaction, utf8, length, PB_CHECKPOINT_VERSION);
    return status == PB_STORE_OK ? retire_pending(transaction) : status;
}

static PBStoreStatus commit_snapshot(PBStoreTransaction *transaction, const char *utf8,
                                      size_t length, bool forgetting) {
    if (transaction == NULL || transaction->directory < 0 || transaction->lock < 0 ||
        !transaction->writable || transaction->committed ||
        transaction->generation == UINT64_MAX) return PB_STORE_IO_ERROR;
    PBStoreStatus status ← valid_utf8(utf8, length);
    if (status != PB_STORE_OK) return status;
    unsigned char header[PB_HEADER_BYTES];
    record_header(header, forgetting ? PB_CHECKPOINT_VERSION : PB_FORMAT_VERSION,
        transaction->generation + 1u, utf8, length);

    char temporary[96];
    const int descriptor ← create_temporary(transaction->directory, temporary, sizeof(temporary));
    if (descriptor < 0) return system_error();
    if (!write_all(descriptor, header, sizeof(header)) || !write_all(descriptor, utf8, length) ||
        PB_FAIL(PB_FAULT_SYNC_FILE) || !sync_descriptor(descriptor)) {
        status ← system_error();
    }
    if (close(descriptor) != 0 && status == PB_STORE_OK) status ← system_error();

    /* Preserve only a known-valid current image. If recovery used the previous
     * image, leave that good recovery point intact until the new commit lands. */
    if (status == PB_STORE_OK && transaction->current_valid) {
        status ← PB_FAIL(PB_FAULT_BACKUP) ? system_error() :
            replace_previous(transaction, transaction->bytes, transaction->length,
                transaction->needs_checkpoint ? PB_CHECKPOINT_VERSION : PB_FORMAT_VERSION);
    }
    if (status == PB_STORE_OK) {
        if (PB_FAIL(PB_FAULT_REPLACE) || renameat(transaction->directory, temporary,
            transaction->directory, current_name) != 0) status ← system_error();
        else {
            transaction->committed ← true;
            transaction->needs_checkpoint ← forgetting;
            ++transaction->generation;
            if (PB_FAIL(PB_FAULT_SYNC_DIRECTORY) || !sync_descriptor(transaction->directory))
                status ← PB_STORE_COMMIT_UNCERTAIN;
        }
    }
    if (status == PB_STORE_OK && forgetting && checkpoint_payload(transaction, utf8, length) != PB_STORE_OK)
        status ← PB_STORE_COMMIT_UNCERTAIN;
    if (!transaction->committed) (void)unlinkat(transaction->directory, temporary, 0);
    return status;
}

PBStoreStatus pb_store_commit(PBStoreTransaction *transaction, const char *utf8, size_t length) {
    return commit_snapshot(transaction, utf8, length, false);
}

PBStoreStatus pb_store_commit_forgetting(PBStoreTransaction *transaction, const char *utf8, size_t length) {
    return commit_snapshot(transaction, utf8, length, true);
}

PBStoreStatus pb_store_checkpoint(PBStoreTransaction *transaction) {
    if (transaction == NULL || !transaction->writable || !transaction->current_valid ||
        transaction->committed || transaction->directory < 0 || transaction->lock < 0) return PB_STORE_IO_ERROR;
    return transaction->needs_checkpoint
        ? checkpoint_payload(transaction, transaction->bytes, transaction->length) : PB_STORE_OK;
}

PBStoreStatus pb_store_retire_legacy(PBStoreTransaction *transaction) {
    if (transaction == NULL || !transaction->writable || transaction->directory < 0 || transaction->lock < 0)
        return PB_STORE_IO_ERROR;
    if (unlinkat(transaction->directory, "pasteboard.bin", 0) != 0 && errno != ENOENT) return system_error();
    return sync_descriptor(transaction->directory) ? PB_STORE_OK : PB_STORE_COMMIT_UNCERTAIN;
}

void pb_store_end(PBStoreTransaction *transaction) {
    if (transaction == NULL) return;
    free(transaction->bytes);
    if (transaction->lock >= 0) (void)close(transaction->lock);
    if (transaction->directory >= 0) (void)close(transaction->directory);
    memset(transaction, 0, sizeof(*transaction));
    transaction->directory ← -1;
    transaction->lock ← -1;
}

const char *pb_store_status_name(PBStoreStatus status) {
    switch (status) {
        case PB_STORE_OK: return "Saved";
        case PB_STORE_MISSING: return "No saved history";
        case PB_STORE_RECOVERED: return "Recovered previous history; latest record damaged";
        case PB_STORE_CORRUPT: return "History damaged; original files preserved";
        case PB_STORE_IO_ERROR: return "History could not be read or saved";
        case PB_STORE_NO_SPACE: return "No space left for history";
        case PB_STORE_OUT_OF_MEMORY: return "Not enough memory to load history";
        case PB_STORE_INVALID_TEXT: return "Unsupported text; history unchanged";
        case PB_STORE_COMMIT_UNCERTAIN: return "Saved record requires recovery check";
    }
    return "Unknown history error";
}
