#define _POSIX_C_SOURCE 200809L
#define _FILE_OFFSET_BITS 64
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <limits.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

/* Local-only prototype transport adapter. No public network or TLS support. */
#define REQUEST_LIMIT 8192
#define COPY_BUFFER (64 * 1024)

struct offer {
    int source;
    struct stat snapshot;
    char token[65];
    char filename[128];
};

static void fail(const char *message) {
    perror(message);
    exit(1);
}

static bool parse_number(const char *text, uint64_t *result) {
    if (!text || *text < '0' || *text > '9') return false;
    uint64_t value = 0;
    do {
        unsigned digit = (unsigned)(*text - '0');
        if (value > (UINT64_MAX - digit) / 10) return false;
        value = value * 10 + digit;
        text++;
    } while (*text >= '0' && *text <= '9');
    if (*text) return false;
    *result = value;
    return true;
}

static void create_token(char output[65]) {
    unsigned char bytes[32];
    int source = open("/dev/urandom", O_RDONLY | O_CLOEXEC);
    if (source < 0) fail("Open system randomness");
    for (size_t done = 0; done < sizeof(bytes);) {
        ssize_t n = read(source, bytes + done, sizeof(bytes) - done);
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) fail("Read system randomness");
        done += (size_t)n;
    }
    close(source);
    static const char hex[] = "0123456789abcdef";
    for (size_t i = 0; i < sizeof(bytes); ++i) {
        output[2 * i] = hex[bytes[i] >> 4];
        output[2 * i + 1] = hex[bytes[i] & 15];
    }
    output[64] = '\0';
}

/* Exact length and value, without a value-dependent early exit. */
static bool matches_token(const char *path, const char token[65]) {
    if (strncmp(path, "/f/", 3) != 0 || strlen(path) != 67) return false;
    unsigned diff = 0;
    for (size_t i = 0; i < 64; ++i) diff |= (unsigned)(unsigned char)(path[i + 3] ^ token[i]);
    return diff == 0;
}

static void clean_name(const char *path, char output[128]) {
    const char *name = strrchr(path, '/');
    name = name ? name + 1 : path;
    if (!*name) name = "download";
    size_t j = 0;
    for (; *name && j < 127; name++) {
        const unsigned char c = (unsigned char)*name;
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') || c == '.' || c == '_' || c == '-')
            output[j++] = (char)c;
        else output[j++] = '_';
    }
    output[j] = 0;
}

static bool unchanged(const struct stat *original, const struct stat *current) {
    return original->st_dev == current->st_dev &&
           original->st_ino == current->st_ino &&
           original->st_size == current->st_size &&
           original->st_mtim.tv_sec == current->st_mtim.tv_sec &&
           original->st_mtim.tv_nsec == current->st_mtim.tv_nsec &&
           original->st_ctim.tv_sec == current->st_ctim.tv_sec &&
           original->st_ctim.tv_nsec == current->st_ctim.tv_nsec;
}

static bool write_fully(int destination, const void *data, size_t size) {
    const char *bytes = data;
    size_t done = 0;
    while (done < size) {
        ssize_t n = write(destination, bytes + done, size - done);
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) return false;
        done += (size_t)n;
    }
    return true;
}

static void simple_reply(int socket, int status, const char *reason, const char *extra) {
    char message[512];
    int n = snprintf(message, sizeof(message),
         "HTTP/1.1 %d %s\r\nContent-Length: 0\r\nCache-Control: no-store\r\n"
         "X-Content-Type-Options: nosniff\r\nConnection: close\r\n%s\r\n",
         status, reason, extra ? extra : "");
    if (n > 0 && (size_t)n < sizeof(message)) (void)write_fully(socket, message, (size_t)n);
}

enum range_result { RANGE_ABSENT, RANGE_VALID, RANGE_INVALID };

static enum range_result parse_range(char *header, off_t size, off_t *start, off_t *end) {
    if (!header) return RANGE_ABSENT;
    if (strncmp(header, "bytes=", 6) != 0 || strchr(header, ',') || size == 0)
        return RANGE_INVALID;
    char *spec = header + 6;
    char *hyphen = strchr(spec, '-');
    if (!hyphen || strchr(hyphen + 1, '-')) return RANGE_INVALID;
    *hyphen = 0;
    uint64_t first = 0, last = 0;
    if (*spec) {
        if (!parse_number(spec, &first) || first >= (uint64_t)size) return RANGE_INVALID;
        if (hyphen[1]) {
            if (!parse_number(hyphen + 1, &last) || last < first) return RANGE_INVALID;
            if (last >= (uint64_t)size) last = (uint64_t)size - 1;
        } else last = (uint64_t)size - 1;
        *start = (off_t)first;
        *end = (off_t)last;
    } else {
        if (!parse_number(hyphen + 1, &last) || last == 0) return RANGE_INVALID;
        if (last > (uint64_t)size) last = (uint64_t)size;
        *start = size - (off_t)last;
        *end = size - 1;
    }
    return RANGE_VALID;
}

static void handle_request(int connection, const struct offer *offer) {
    char request[REQUEST_LIMIT + 1];
    size_t total = 0;
    bool complete = false;
    while (total < REQUEST_LIMIT) {
        ssize_t n = read(connection, request + total, REQUEST_LIMIT - total);
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) break;
        total += (size_t)n;
        request[total] = '\0';
        if (strstr(request, "\r\n\r\n")) { complete = true; break; }
    }
    if (!complete) { simple_reply(connection, 431, "Request Header Fields Too Large", NULL); return; }
    char *endline = strstr(request, "\r\n");
    if (!endline) { simple_reply(connection, 400, "Bad Request", NULL); return; }
    *endline = 0;
    char method[8], path[256], version[16], extra;
    if (sscanf(request, "%7s %255s %15s %c", method, path, version, &extra) != 3 ||
        (strcmp(version, "HTTP/1.1") && strcmp(version, "HTTP/1.0"))) {
        simple_reply(connection, 400, "Bad Request", NULL); return;
    }
    if (!matches_token(path, offer->token)) {
        simple_reply(connection, 404, "Not Found", NULL); return;
    }
    bool is_head = strcmp(method, "HEAD") == 0;
    if (!is_head && strcmp(method, "GET")) {
        simple_reply(connection, 405, "Method Not Allowed", "Allow: GET, HEAD\r\n"); return;
    }
    char *range = NULL;
    for (char *line = endline + 2; line[0] && line[1];) {
        char *line_end = strstr(line, "\r\n");
        if (!line_end) { simple_reply(connection, 400, "Bad Request", NULL); return; }
        *line_end = 0;
        if (!*line) break;
        if (strncasecmp(line, "Range:", 6) == 0) {
            if (range) { simple_reply(connection, 400, "Bad Request", NULL); return; }
            range = line + 6;
            while (*range == ' ' || *range == '\t') range++;
        }
        line = line_end + 2;
    }
    struct stat current;
    if (fstat(offer->source, &current) || !unchanged(&offer->snapshot, &current)) {
        simple_reply(connection, 409, "Conflict", NULL); return;
    }
    off_t size = offer->snapshot.st_size;
    off_t first = 0, last = size ? size - 1 : 0;
    enum range_result rr = is_head ? RANGE_ABSENT : parse_range(range, size, &first, &last);
    if (rr == RANGE_INVALID) {
        char extra_header[128];
        snprintf(extra_header, sizeof(extra_header), "Content-Range: bytes */%jd\r\n", (intmax_t)size);
        simple_reply(connection, 416, "Range Not Satisfiable", extra_header);
        return;
    }
    off_t length = rr == RANGE_VALID ? last - first + 1 : size;
    char header[1024], content_range[150] = "";
    if (rr == RANGE_VALID)
        snprintf(content_range, sizeof(content_range), "Content-Range: bytes %jd-%jd/%jd\r\n",
                 (intmax_t)first, (intmax_t)last, (intmax_t)size);
    int n = snprintf(header, sizeof(header),
         "HTTP/1.1 %d %s\r\nContent-Length: %jd\r\n%s"
         "Accept-Ranges: bytes\r\nContent-Type: application/octet-stream\r\n"
         "Content-Disposition: attachment; filename=\"%s\"\r\n"
         "Cache-Control: no-store\r\nReferrer-Policy: no-referrer\r\n"
         "X-Content-Type-Options: nosniff\r\nConnection: close\r\n\r\n",
         rr == RANGE_VALID ? 206 : 200, rr == RANGE_VALID ? "Partial Content" : "OK",
         (intmax_t)length, content_range, offer->filename);
    if (n <= 0 || (size_t)n >= sizeof(header) || !write_fully(connection, header, (size_t)n) || is_head) return;
    char buffer[COPY_BUFFER];
    off_t offset = first, remaining = length;
    while (remaining > 0) {
        size_t amount = remaining < COPY_BUFFER ? (size_t)remaining : COPY_BUFFER;
        ssize_t got = pread(offer->source, buffer, amount, offset);
        if (got < 0 && errno == EINTR) continue;
        if (got <= 0 || !write_fully(connection, buffer, (size_t)got)) break;
        offset += got;
        remaining -= got;
    }
}

static int create_listener(unsigned port, unsigned *bound_port) {
    int listener = socket(AF_INET, SOCK_STREAM, 0);
    if (listener < 0) fail("Create TCP socket");
    int one = 1;
    (void)setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
    struct sockaddr_in address = {0};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port = htons((uint16_t)port);
    if (bind(listener, (struct sockaddr *)&address, sizeof(address))) fail("Bind loopback address");
    if (listen(listener, 8)) fail("Listen for connections");
    socklen_t address_size = sizeof(address);
    if (getsockname(listener, (struct sockaddr *)&address, &address_size)) fail("Read server address");
    *bound_port = (unsigned)ntohs(address.sin_port);
    return listener;
}

int main(int argc, char **argv) {
    if (argc < 2 || argc > 4) {
        fprintf(stderr, "Usage: %s FILE [PORT (0=automatic)] [TTL_SECONDS (max 86400)]\n", argv[0]);
        return 2;
    }
    uint64_t port = 0, ttl = 3600;
    if ((argc > 2 && (!parse_number(argv[2], &port) || port > 65535)) ||
        (argc > 3 && (!parse_number(argv[3], &ttl) || ttl == 0 || ttl > 86400))) {
        fputs("Invalid port or lifetime\n", stderr); return 2;
    }
    struct offer offer = {0};
    offer.source = open(argv[1], O_RDONLY | O_CLOEXEC | O_NOFOLLOW);
    if (offer.source < 0) fail("Open source file");
    if (fstat(offer.source, &offer.snapshot)) fail("Inspect source file");
    if (!S_ISREG(offer.snapshot.st_mode) || offer.snapshot.st_size < 0) {
        fputs("Source must be a regular file, not a symlink\n", stderr); return 2;
    }
    clean_name(argv[1], offer.filename);
    create_token(offer.token);
    unsigned actual_port;
    int listener = create_listener((unsigned)port, &actual_port);
    printf("URL http://127.0.0.1:%u/f/%s\nNAME %s\nSIZE %jd\nTTL %ju\n",
           actual_port, offer.token, offer.filename, (intmax_t)offer.snapshot.st_size, (uintmax_t)ttl);
    fflush(stdout);
    signal(SIGPIPE, SIG_IGN);
    time_t deadline = time(NULL) + (time_t)ttl;
    for (;;) {
        time_t now = time(NULL);
        if (now >= deadline) break;
        fd_set sockets;
        FD_ZERO(&sockets);
        FD_SET(listener, &sockets);
        struct timeval wait = {.tv_sec = deadline - now, .tv_usec = 0};
        int ready = select(listener + 1, &sockets, NULL, NULL, &wait);
        if (ready < 0 && errno == EINTR) continue;
        if (ready <= 0) break;
        int connection = accept(listener, NULL, NULL);
        if (connection < 0) continue;
        struct timeval timeout = {.tv_sec = 5, .tv_usec = 0};
        (void)setsockopt(connection, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
        (void)setsockopt(connection, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
        handle_request(connection, &offer);
        close(connection);
    }
    close(listener);
    close(offer.source);
    return 0;
}
