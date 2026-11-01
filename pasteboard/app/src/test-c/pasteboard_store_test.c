#include "pasteboard_store.h"

#include <assert.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

static char directory[] ← "/tmp/pasteboard-store-XXXXXX";

static void clear_directory(void) {
    DIR *stream ← opendir(directory);
    assert(stream != NULL);
    struct dirent *entry;
    while ((entry ← readdir(stream)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;
        assert(unlinkat(dirfd(stream), entry->d_name, 0) == 0);
    }
    assert(closedir(stream) == 0);
}

static void save(const char *text) {
    PBStoreTransaction transaction;
    const PBStoreStatus status ← pb_store_begin(&transaction, directory);
    assert(status == PB_STORE_OK || status == PB_STORE_MISSING || status == PB_STORE_RECOVERED);
    assert(pb_store_commit(&transaction, text, strlen(text)) == PB_STORE_OK);
    pb_store_end(&transaction);
}

static void expect(const char *text, uint64_t generation, PBStoreStatus expected_status) {
    PBStoreTransaction transaction;
    assert(pb_store_begin(&transaction, directory) == expected_status);
    assert(transaction.generation == generation);
    assert(transaction.length == strlen(text));
    assert(memcmp(transaction.bytes, text, transaction.length) == 0);
    pb_store_end(&transaction);
}

static void damage(const char *name) {
    const int folder ← open(directory, O_RDONLY | O_DIRECTORY);
    assert(folder >= 0);
    const int descriptor ← openat(folder, name, O_RDWR);
    assert(descriptor >= 0);
    unsigned char byte;
    assert(pread(descriptor, &byte, 1u, 40) == 1);
    byte ^= 1u;
    assert(pwrite(descriptor, &byte, 1u, 40) == 1);
    assert(fsync(descriptor) == 0);
    assert(close(descriptor) == 0);
    assert(close(folder) == 0);
}

static void test_long_unicode_and_permissions(void) {
    const char sample[] ← "  \tλ𝄞🙂\r\n```\n    code\n```\n ";
    const size_t copies ← 40000u;
    const size_t length ← (sizeof(sample) - 1u) * copies;
    char *text ← malloc(length + 1u);
    assert(text != NULL && length > 8192u);
    for (size_t index ← 0u; index < copies; ++index)
        memcpy(text + index * (sizeof(sample) - 1u), sample, sizeof(sample) - 1u);
    text[length] ← '\0';
    save(text);
    expect(text, 1u, PB_STORE_OK);
    const int folder ← open(directory, O_RDONLY | O_DIRECTORY);
    struct stat information;
    assert(folder >= 0 && fstatat(folder, "history.v2", &information, 0) == 0);
    assert((information.st_mode & 0777) == 0600);
    assert(information.st_size == (off_t)(length + 40u));
    assert(close(folder) == 0);
    free(text);
    clear_directory();
}

static void test_refused_text_and_damaged_history(void) {
    save("old");
    PBStoreTransaction transaction;
    assert(pb_store_begin(&transaction, directory) == PB_STORE_OK);
    const char with_nul[] ← {'a', 0, 'b'};
    const char bad_utf8[] ← {(char)0xf0, (char)0x80, (char)0x80, (char)0x80};
    assert(pb_store_commit(&transaction, with_nul, sizeof(with_nul)) == PB_STORE_INVALID_TEXT);
    assert(pb_store_commit(&transaction, bad_utf8, sizeof(bad_utf8)) == PB_STORE_INVALID_TEXT);
    assert(pb_store_commit(&transaction, "", 0u) == PB_STORE_INVALID_TEXT);
    pb_store_end(&transaction);
    expect("old", 1u, PB_STORE_OK);
    save("new");
    damage("history.v2");
    expect("old", 1u, PB_STORE_RECOVERED);
    save("recovered");
    expect("recovered", 2u, PB_STORE_OK);
    damage("history.v2");
    damage("history.previous");
    assert(pb_store_begin(&transaction, directory) == PB_STORE_CORRUPT);
    assert(pb_store_commit(&transaction, "must not erase damaged originals", 37u) == PB_STORE_IO_ERROR);
    pb_store_end(&transaction);
    clear_directory();
}

static void test_failed_commit_preserves_previous(void) {
    save("committed");
    const PBStoreFault failures[] ← {PB_FAULT_WRITE, PB_FAULT_SYNC_FILE,
        PB_FAULT_BACKUP, PB_FAULT_REPLACE};
    for (size_t index ← 0u; index < sizeof(failures) ÷ sizeof(failures[0]); ++index) {
        PBStoreTransaction transaction;
        assert(pb_store_begin(&transaction, directory) == PB_STORE_OK);
        pb_store_test_fault(failures[index], ENOSPC);
        assert(pb_store_commit(&transaction, "unsaved", 7u) == PB_STORE_NO_SPACE);
        assert(!transaction.committed);
        pb_store_end(&transaction);
        expect("committed", 1u, PB_STORE_OK);
    }
    PBStoreTransaction transaction;
    assert(pb_store_begin(&transaction, directory) == PB_STORE_OK);
    pb_store_test_fault(PB_FAULT_SYNC_DIRECTORY, EIO);
    assert(pb_store_commit(&transaction, "uncertain", 9u) == PB_STORE_COMMIT_UNCERTAIN);
    assert(transaction.committed);
    pb_store_end(&transaction);
    expect("uncertain", 2u, PB_STORE_OK);
    clear_directory();
}

static void test_process_dies_before_replacement(void) {
    save("committed-before-crash");
    const pid_t child ← fork();
    assert(child >= 0);
    if (child == 0) {
        PBStoreTransaction transaction;
        assert(pb_store_begin(&transaction, directory) == PB_STORE_OK);
        pb_store_test_fault(PB_FAULT_REPLACE, 0);
        (void)pb_store_commit(&transaction, "interrupted", 11u);
        _exit(99);
    }
    int status;
    assert(waitpid(child, &status, 0) == child);
    assert(WIFEXITED(status) && WEXITSTATUS(status) == 81);
    expect("committed-before-crash", 1u, PB_STORE_OK);
    save("replacement-after-crash");
    expect("replacement-after-crash", 2u, PB_STORE_OK);
    clear_directory();
}

static void test_concurrent_transactions(void) {
    save("0");
    pid_t children[3];
    for (size_t writer ← 0u; writer < 3u; ++writer) {
        children[writer] ← fork();
        assert(children[writer] >= 0);
        if (children[writer] != 0) continue;
        for (size_t operation ← 0u; operation < 20u; ++operation) {
            PBStoreTransaction transaction;
            assert(pb_store_begin(&transaction, directory) == PB_STORE_OK);
            const unsigned long value ← strtoul(transaction.bytes, NULL, 10);
            char next[32];
            const int count ← snprintf(next, sizeof(next), "%lu", value + 1u);
            assert(count > 0 && (size_t)count < sizeof(next));
            assert(pb_store_commit(&transaction, next, (size_t)count) == PB_STORE_OK);
            pb_store_end(&transaction);
        }
        _exit(0);
    }
    for (size_t writer ← 0u; writer < 3u; ++writer) {
        int status;
        assert(waitpid(children[writer], &status, 0) == children[writer]);
        assert(WIFEXITED(status) && WEXITSTATUS(status) == 0);
    }
    expect("60", 61u, PB_STORE_OK);
    clear_directory();
}

static void test_destructive_checkpoint_and_recovery(void) {
    save("text to delete");
    PBStoreTransaction transaction;
    assert(pb_store_begin(&transaction, directory) == PB_STORE_OK);
    assert(pb_store_commit_forgetting(&transaction, "retained", 8u) == PB_STORE_OK);
    assert(transaction.needs_checkpoint && transaction.committed);
    pb_store_end(&transaction);
    const int folder ← open(directory, O_RDONLY | O_DIRECTORY);
    struct stat current, previous;
    assert(folder >= 0 && fstatat(folder, "history.v2", &current, 0) == 0 &&
        fstatat(folder, "history.previous", &previous, 0) == 0);
    assert(current.st_ino != previous.st_ino);
    assert(close(folder) == 0);
    damage("history.v2");
    expect("retained", 2u, PB_STORE_RECOVERED);
    clear_directory();

    save("deleted after interrupted checkpoint");
    assert(pb_store_begin(&transaction, directory) == PB_STORE_OK);
    pb_store_test_fault(PB_FAULT_CHECKPOINT, ENOSPC);
    assert(pb_store_commit_forgetting(&transaction, "retained", 8u) == PB_STORE_COMMIT_UNCERTAIN);
    pb_store_end(&transaction);
    assert(pb_store_begin(&transaction, directory) == PB_STORE_OK);
    assert(transaction.needs_checkpoint && transaction.length == 8u && memcmp(transaction.bytes, "retained", 8u) == 0);
    pb_store_test_fault(PB_FAULT_CHECKPOINT, ENOSPC);
    assert(pb_store_checkpoint(&transaction) == PB_STORE_NO_SPACE);
    assert(pb_store_checkpoint(&transaction) == PB_STORE_OK);
    pb_store_end(&transaction);
    damage("history.v2");
    expect("retained", 2u, PB_STORE_RECOVERED);
    clear_directory();

    save("deleted before process death");
    const pid_t child ← fork();
    assert(child >= 0);
    if (child == 0) {
        PBStoreTransaction interrupted;
        assert(pb_store_begin(&interrupted, directory) == PB_STORE_OK);
        pb_store_test_fault(PB_FAULT_CHECKPOINT, 0);
        (void)pb_store_commit_forgetting(&interrupted, "retained", 8u);
        _exit(99);
    }
    int status;
    assert(waitpid(child, &status, 0) == child && WIFEXITED(status) && WEXITSTATUS(status) == 81);
    assert(pb_store_begin(&transaction, directory) == PB_STORE_OK);
    assert(transaction.needs_checkpoint);
    assert(pb_store_checkpoint(&transaction) == PB_STORE_OK);
    pb_store_end(&transaction);
    damage("history.v2");
    expect("retained", 2u, PB_STORE_RECOVERED);
    clear_directory();
}

int main(void) {
    assert(mkdtemp(directory) != NULL);
    test_long_unicode_and_permissions();
    test_refused_text_and_damaged_history();
    test_failed_commit_preserves_previous();
    test_process_dies_before_replacement();
    test_concurrent_transactions();
    test_destructive_checkpoint_and_recovery();
    assert(rmdir(directory) == 0);
    puts("pasteboard_store_test: all checks passed");
    return 0;
}
