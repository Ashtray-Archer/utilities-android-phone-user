#include "pasteboard_history.h"
#include "pasteboard_behavior_loader.h"
#include "utf8.h"
#include <fcntl.h>
#include <errno.h>
#include <limits.h>
#include <pthread.h>
#include <stdint.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static pthread_mutex_t initialization_mutex ← PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t notification_mutex ← PTHREAD_MUTEX_INITIALIZER;
static jclass checked_behavior ← NULL;
static jmethodID checked_transition ← NULL;
static char *private_directory ← NULL;
static void (*change_handler)(void *) ← NULL;
static void *change_owner ← NULL;
/* API 26 Bionic does not expose GNU __tls_get_addr. Use the public pthread
 * boundary for per-thread diagnostics, with destructor-owned allocations. */
static pthread_once_t error_once ← PTHREAD_ONCE_INIT;
static pthread_key_t error_key;
static int error_key_ready ← 0;
static atomic_bool capture_enabled ← false;
static atomic_bool automatic_requested ← false;

static void create_error_key(void) { error_key_ready ← pthread_key_create(&error_key, free) == 0; }

static char *error_buffer(void) {
    (void)pthread_once(&error_once, create_error_key);
    if (!error_key_ready) return NULL;
    char *buffer ← pthread_getspecific(error_key);
    if (buffer == NULL) {
        buffer ← calloc(160u, 1u);
        if (buffer && pthread_setspecific(error_key, buffer) != 0) { free(buffer); buffer ← NULL; }
    }
    return buffer;
}

static bool error(const char *message) {
    char *buffer ← error_buffer();
    if (buffer) (void)snprintf(buffer, 160u, "%s", message);
    return false;
}

const char *pb_history_last_error(void) {
    const char *buffer ← error_buffer();
    return buffer ? buffer : "Not enough memory for history status";
}
const char *pb_history_private_directory(void) { return private_directory; }
bool pb_history_capture_enabled(void) { return atomic_load(&capture_enabled); }
bool pb_history_automatic_requested(void) { return atomic_load(&automatic_requested); }

void pb_history_set_change_handler(void (*handler)(void *), void *owner) {
    (void)pthread_mutex_lock(&notification_mutex);
    change_handler ← handler;
    change_owner ← owner;
    (void)pthread_mutex_unlock(&notification_mutex);
}

static void notify_changed(void) {
    (void)pthread_mutex_lock(&notification_mutex);
    if (change_handler != NULL) change_handler(change_owner);
    (void)pthread_mutex_unlock(&notification_mutex);
}

jstring pb_history_java_text(JNIEnv *environment, const char *bytes, size_t length) {
    uint16_t *units ← NULL;
    size_t count ← 0u;
    if (clipboard_utf8_to_utf16(bytes, length, &units, &count) != CLIPBOARD_UTF_OK ||
        count > (size_t)INT32_MAX) { free(units); return NULL; }
    jstring result ← (*environment)->NewString(environment, (const jchar *)units, (jsize)count);
    free(units);
    return result;
}

bool pb_history_native_text(JNIEnv *environment, jstring string, PBText *text) {
    text->bytes ← NULL;
    text->length ← 0u;
    if (string == NULL || (*environment)->ExceptionCheck(environment)) return false;
    const jsize count ← (*environment)->GetStringLength(environment, string);
    const jchar *units ← (*environment)->GetStringChars(environment, string, NULL);
    if (units == NULL) return false;
    const ClipboardUtfStatus status ← clipboard_utf16_to_utf8((const uint16_t *)units,
        (size_t)count, &text->bytes, &text->length);
    (*environment)->ReleaseStringChars(environment, string, units);
    return status == CLIPBOARD_UTF_OK && !(*environment)->ExceptionCheck(environment);
}

bool pb_history_open(JNIEnv *environment, jobject context) {
    if (environment == NULL || context == NULL) return error("History context unavailable");
    (void)pthread_mutex_lock(&initialization_mutex);
    if (checked_behavior != NULL && private_directory != NULL) {
        (void)pthread_mutex_unlock(&initialization_mutex);
        return true;
    }
    bool success ← false;
    if ((*environment)->PushLocalFrame(environment, 32) != JNI_OK) goto opened;
    jclass context_class ← (*environment)->GetObjectClass(environment, context);
    jmethodID get_loader ← context_class ? (*environment)->GetMethodID(environment,
        context_class, "getClassLoader", "()Ljava/lang/ClassLoader;") : NULL;
    jobject loader ← get_loader ? (*environment)->CallObjectMethod(environment, context, get_loader) : NULL;
    jclass behavior ← loader ? pb_load_checked_behavior(environment, context, loader) : NULL;
    jmethodID transition ← behavior && !(*environment)->ExceptionCheck(environment)
        ? (*environment)->GetStaticMethodID(environment, behavior, "pasteboard_transition",
            "(Ljava/lang/String;)Ljava/lang/String;") : NULL;
    if (transition == NULL || (*environment)->ExceptionCheck(environment)) goto frame_finished;
    jmethodID files ← (*environment)->GetMethodID(environment, context_class,
        "getFilesDir", "()Ljava/io/File;");
    jobject folder ← files ? (*environment)->CallObjectMethod(environment, context, files) : NULL;
    jclass file_class ← folder ? (*environment)->GetObjectClass(environment, folder) : NULL;
    jmethodID absolute ← file_class ? (*environment)->GetMethodID(environment, file_class,
        "getAbsolutePath", "()Ljava/lang/String;") : NULL;
    jstring path ← absolute ? (*environment)->CallObjectMethod(environment, folder, absolute) : NULL;
    PBText directory ← {0};
    if (!pb_history_native_text(environment, path, &directory) || directory.length == 0u ||
        strlen(directory.bytes) != directory.length || directory.bytes[0] != '/') {
        free(directory.bytes);
        goto frame_finished;
    }
    jclass retained ← (*environment)->NewGlobalRef(environment, behavior);
    if (retained == NULL) { free(directory.bytes); goto frame_finished; }
    checked_behavior ← retained;
    checked_transition ← transition;
    private_directory ← directory.bytes;
    success ← true;
frame_finished:
    if ((*environment)->ExceptionCheck(environment)) (*environment)->ExceptionClear(environment);
    (*environment)->PopLocalFrame(environment, NULL);
opened:
    if ((*environment)->ExceptionCheck(environment)) (*environment)->ExceptionClear(environment);
    (void)pthread_mutex_unlock(&initialization_mutex);
    return success || error("Checked Pasteboard behavior could not be loaded");
}

static PBText ascii(const char *text) { return (PBText){(char *)text, strlen(text)}; }

/* One actual checked export call. Native code frames transport and returns the
 * checked result; it does not interpret or reproduce the transition rules. */
static bool reduce(JNIEnv *environment, const char *action, PBText serialized,
                   const PBText *arguments, size_t argument_count, PBFields *result) {
    if (argument_count > SIZE_MAX ÷ sizeof(PBText) - 2u) return error("History request too large");
    PBText *fields ← malloc((argument_count + 2u) * sizeof(PBText));
    if (fields == NULL) return error("Not enough memory for history request");
    fields[0] ← ascii(action);
    fields[1] ← serialized;
    if (argument_count != 0u) memcpy(fields + 2u, arguments, argument_count * sizeof(PBText));
    PBText encoded ← {0};
    const size_t field_count ← strcmp(action, "initialize") == 0 ? 1u : argument_count + 2u;
    const bool framed ← pb_wire_encode(fields, field_count, &encoded);
    free(fields);
    if (!framed) return error("History request is not valid UTF-8 or is too large");
    if ((*environment)->PushLocalFrame(environment, 8) != JNI_OK) { free(encoded.bytes); return false; }
    jstring input ← pb_history_java_text(environment, encoded.bytes, encoded.length);
    free(encoded.bytes);
    jstring output ← input ? (*environment)->CallStaticObjectMethod(environment,
        checked_behavior, checked_transition, input) : NULL;
    PBText returned ← {0};
    bool success ← pb_history_native_text(environment, output, &returned) &&
        pb_wire_decode(returned.bytes, returned.length, result);
    free(returned.bytes);
    if ((*environment)->ExceptionCheck(environment)) {
        (*environment)->ExceptionClear(environment);
        success ← false;
    }
    (*environment)->PopLocalFrame(environment, NULL);
    if (!success) return error("Checked Pasteboard transition failed");
    if (result->count != 2u || pb_text_equal(result->values[0], "invalid_history") ||
        pb_text_equal(result->values[0], "invalid_request")) {
        pb_wire_free(result);
        return error("Saved history is incompatible or damaged; files preserved");
    }
    return true;
}

void pb_history_view_free(PBHistoryView *view) {
    if (view == NULL) return;
    for (size_t index ← 0u; index < view->count; ++index) {
        free(view->entries[index].identity.bytes);
        free(view->entries[index].text.bytes);
        free(view->entries[index].captured_at.bytes);
    }
    free(view->entries);
    memset(view, 0, sizeof(*view));
}

static int board_number(PBText value) {
    if (pb_text_equal(value, "0")) return 0;
    if (pb_text_equal(value, "1")) return 1;
    if (pb_text_equal(value, "2")) return 2;
    if (pb_text_equal(value, "archive")) return -1;
    return -2;
}

static bool project(PBText serialized, PBHistoryView *view) {
    PBFields root ← {0}, entries ← {0};
    if (!pb_wire_decode(serialized.bytes, serialized.length, &root)) return false;
    bool valid ← root.count == 7u && pb_text_equal(root.values[0], "PB3");
    if (!valid) goto projected;
    view->selected_board ← board_number(root.values[1]);
    view->capture_enabled ← pb_text_equal(root.values[3], "1");
    view->automatic_requested ← pb_text_equal(root.values[4], "1");
    valid ← view->selected_board >= 0 && (view->capture_enabled || pb_text_equal(root.values[3], "0")) &&
        (view->automatic_requested || pb_text_equal(root.values[4], "0")) &&
        pb_wire_decode(root.values[6].bytes, root.values[6].length, &entries);
    if (entries.count > SIZE_MAX ÷ sizeof(PBHistoryEntry)) valid ← false;
    if (!valid) goto projected;
    view->entries ← calloc(entries.count, sizeof(PBHistoryEntry));
    if (entries.count != 0u && view->entries == NULL) { valid ← false; goto projected; }
    view->count ← entries.count;
    for (size_t index ← 0u; index < entries.count; ++index) {
        PBFields entry ← {0};
        valid ← pb_wire_decode(entries.values[index].bytes, entries.values[index].length, &entry) &&
            entry.count == 5u;
        if (valid) {
            PBHistoryEntry *target ← &view->entries[index];
            target->board ← board_number(entry.values[1]);
            target->protected ← pb_text_equal(entry.values[2], "1");
            valid ← target->board >= -1 && (target->protected || pb_text_equal(entry.values[2], "0"));
            if (valid) {
                target->identity ← entry.values[0]; entry.values[0].bytes ← NULL;
                target->captured_at ← entry.values[3]; entry.values[3].bytes ← NULL;
                target->text ← entry.values[4]; entry.values[4].bytes ← NULL;
            }
        }
        pb_wire_free(&entry);
        if (!valid) break;
    }
projected:
    pb_wire_free(&entries);
    pb_wire_free(&root);
    if (!valid) pb_history_view_free(view);
    return valid;
}

typedef struct {
    PBText text;
    uint64_t captured_at;
} LegacyEntry;

static bool legacy_number(FILE *file, size_t width, uint64_t *value) {
    *value ← 0u;
    for (size_t index ← 0u; index < width; ++index) {
        const int byte ← fgetc(file);
        if (byte == EOF) return false;
        *value |= (uint64_t)(unsigned char)byte << (8u * index);
    }
    return true;
}

static bool advance_migration(JNIEnv *environment, PBFields *current,
                              const char *action, PBText *arguments, size_t count,
                              const char *expected) {
    PBFields next ← {0};
    if (!reduce(environment, action, current->values[1], arguments, count, &next)) return false;
    const bool accepted ← pb_text_equal(next.values[0], expected) ||
        (strcmp(action, "capture") == 0 && pb_text_equal(next.values[0], "duplicate"));
    if (!accepted) {
        pb_wire_free(&next);
        return error("Legacy history contains unsupported text; original file preserved");
    }
    pb_wire_free(current);
    *current ← next;
    return true;
}

/* Version 1 wrote explicit little-endian integer fields on all maintained
 * targets. Decode those bytes only; every placement and duplicate decision is
 * performed by the same checked reducer used for new captures. Never remove or
 * rewrite pasteboard.bin, including after a successful migration. */
static bool migrate_legacy(JNIEnv *environment, int directory, PBFields *current) {
    const int descriptor ← openat(directory, "pasteboard.bin", O_RDONLY | O_CLOEXEC | O_NOFOLLOW | O_NONBLOCK);
    if (descriptor < 0) return errno == ENOENT || error("Legacy history could not be opened; files preserved");
    struct stat information;
    const bool regular ← fstat(descriptor, &information) == 0 && S_ISREG(information.st_mode);
    FILE *file ← regular ? fdopen(descriptor, "rb") : NULL;
    if (file == NULL) { (void)close(descriptor); return error("Legacy history is not a readable file"); }
    LegacyEntry entries[3][16] ← {0};
    size_t counts[3] ← {0};
    static const unsigned char expected_magic[8] ← {'P', 'B', 'O', 'A', 'R', 'D', '1', 0};
    bool valid ← true;
    for (size_t index ← 0u; index < sizeof(expected_magic); ++index)
        if (fgetc(file) != expected_magic[index]) valid ← false;
    uint64_t version ← 0u, selected ← 0u, old_hash ← 0u;
    valid ← valid && legacy_number(file, 4u, &version) && version == 1u &&
        legacy_number(file, 4u, &selected) && selected < 3u && legacy_number(file, 8u, &old_hash);
    for (size_t board ← 0u; valid && board < 3u; ++board) {
        uint64_t count ← 0u;
        valid ← legacy_number(file, 4u, &count) && count <= 16u;
        counts[board] ← (size_t)count;
        for (size_t index ← 0u; valid && index < counts[board]; ++index) {
            LegacyEntry *entry ← &entries[board][index];
            uint64_t length ← 0u;
            valid ← legacy_number(file, 8u, &entry->captured_at) &&
                legacy_number(file, 4u, &length) && length > 0u && length < 8192u;
            if (!valid) break;
            entry->text.bytes ← malloc((size_t)length + 1u);
            valid ← entry->text.bytes != NULL;
            if (!valid) break;
            entry->text.length ← (size_t)length;
            for (size_t byte ← 0u; valid && byte < entry->text.length; ++byte) {
                const int value ← fgetc(file);
                if (value == EOF) valid ← false;
                else entry->text.bytes[byte] ← (char)(unsigned char)value;
            }
            entry->text.bytes[entry->text.length] ← '\0';
        }
    }
    valid ← valid && fgetc(file) == EOF && ferror(file) == 0;
    if (fclose(file) != 0) valid ← false;
    for (size_t board ← 0u; valid && board < 3u; ++board) {
        char board_name[4];
        (void)snprintf(board_name, sizeof(board_name), "%zu", board);
        PBText selection ← ascii(board_name);
        valid ← advance_migration(environment, current, "select", &selection, 1u, "selected");
        for (size_t reverse_index ← counts[board]; valid && reverse_index > 0u; --reverse_index) {
            const size_t index ← reverse_index - 1u;
            char identity[48], timestamp[32];
            (void)snprintf(identity, sizeof(identity), "legacy-%zu-%zu", board, index);
            (void)snprintf(timestamp, sizeof(timestamp), "%llu", (unsigned long long)entries[board][index].captured_at);
            PBText arguments[3] ← {ascii(identity), ascii(timestamp), entries[board][index].text};
            valid ← advance_migration(environment, current, "capture", arguments, 3u, "captured");
        }
    }
    if (valid) {
        char board_name[4];
        (void)snprintf(board_name, sizeof(board_name), "%u", (unsigned)selected);
        PBText selection ← ascii(board_name);
        valid ← advance_migration(environment, current, "select", &selection, 1u, "selected");
        PBText empty[3] ← {ascii("legacy-end"), ascii(""), ascii("")};
        valid ← valid && advance_migration(environment, current, "capture", empty, 3u, "empty");
    }
    for (size_t board ← 0u; board < 3u; ++board)
        for (size_t index ← 0u; index < 16u; ++index) free(entries[board][index].text.bytes);
    return valid || error("Legacy history is damaged or incompatible; original file preserved");
}

bool pb_history_apply(JNIEnv *environment, const char *action,
                       const PBText *arguments, size_t argument_count, PBHistoryView *view) {
    char *diagnostic ← error_buffer();
    if (diagnostic) diagnostic[0] ← '\0';
    if (environment == NULL || checked_behavior == NULL || private_directory == NULL ||
        action == NULL || view == NULL) return error("History is not initialized");
    PBStoreTransaction transaction;
    const PBStoreStatus storage ← pb_store_begin(&transaction, private_directory);
    if (storage != PB_STORE_OK && storage != PB_STORE_RECOVERED && storage != PB_STORE_MISSING) {
        pb_store_end(&transaction);
        return error(pb_store_status_name(storage));
    }
    PBFields initialized ← {0}, transitioned ← {0};
    PBHistoryView projected ← {0};
    bool success ← false;
    PBText previous ← {transaction.bytes, transaction.length};
    if (storage == PB_STORE_MISSING) {
        if (!reduce(environment, "initialize", (PBText){0}, NULL, 0u, &initialized)) goto finished;
        if (!migrate_legacy(environment, transaction.directory, &initialized)) goto finished;
        previous ← initialized.values[1];
    }
    if (!reduce(environment, action, previous, arguments, argument_count, &transitioned)) goto finished;
    const PBText next ← transitioned.values[1];
    if (!project(next, &projected)) { (void)error("Checked history projection failed"); goto finished; }
    const bool changed ← storage == PB_STORE_MISSING || storage == PB_STORE_RECOVERED ||
        previous.length != next.length || memcmp(previous.bytes, next.bytes, next.length) != 0;
    PBStoreStatus committed ← PB_STORE_OK;
    if (changed) committed ← pb_store_commit(&transaction, next.bytes, next.length);
    if (committed != PB_STORE_OK) {
        (void)error(pb_store_status_name(committed));
        goto finished;
    }
    projected.generation ← transaction.generation;
    projected.storage_status ← storage;
    (void)snprintf(projected.outcome, sizeof(projected.outcome), "%s", transitioned.values[0].bytes);
    atomic_store(&capture_enabled, projected.capture_enabled);
    atomic_store(&automatic_requested, projected.automatic_requested);
    pb_history_view_free(view);
    *view ← projected;
    memset(&projected, 0, sizeof(projected));
    success ← true;
finished:
    pb_history_view_free(&projected);
    pb_wire_free(&transitioned);
    pb_wire_free(&initialized);
    const bool changed_disk ← transaction.committed;
    pb_store_end(&transaction);
    if (changed_disk) notify_changed();
    return success;
}
