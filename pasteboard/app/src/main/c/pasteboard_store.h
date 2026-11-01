#ifndef PASTEBOARD_STORE_H
#define PASTEBOARD_STORE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* The storage adapter accepts an opaque checked-model snapshot. It does not
 * interpret board, duplicate, protection or retention semantics. */
typedef enum {
    PB_STORE_OK,
    PB_STORE_MISSING,
    PB_STORE_RECOVERED,
    PB_STORE_CORRUPT,
    PB_STORE_IO_ERROR,
    PB_STORE_NO_SPACE,
    PB_STORE_OUT_OF_MEMORY,
    PB_STORE_INVALID_TEXT,
    PB_STORE_COMMIT_UNCERTAIN
} PBStoreStatus;

typedef struct {
    int directory;
    int lock;
    char *bytes;
    size_t length;
    uint64_t generation;
    bool current_valid;
    bool writable;
    bool committed;
} PBStoreTransaction;

/* Each transaction opens its own lock description, so flock serializes both
 * independent processes and threads. Hold it through read/reduce/commit. */
PBStoreStatus pb_store_begin(PBStoreTransaction *transaction,
                             const char *private_directory);
PBStoreStatus pb_store_commit(PBStoreTransaction *transaction,
                              const char *utf8, size_t length);
void pb_store_end(PBStoreTransaction *transaction);
const char *pb_store_status_name(PBStoreStatus status);

#ifdef PB_STORE_TESTING
typedef enum {
    PB_FAULT_NONE,
    PB_FAULT_WRITE,
    PB_FAULT_SYNC_FILE,
    PB_FAULT_BACKUP,
    PB_FAULT_REPLACE,
    PB_FAULT_SYNC_DIRECTORY
} PBStoreFault;
void pb_store_test_fault(PBStoreFault fault, int error_number);
#endif

#endif
