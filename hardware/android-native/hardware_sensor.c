#include "android_accelerometer.h"

#include <android/looper.h>

#include <inttypes.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *accelerometer_info_path =
    "/hardware/sensors/accelerometer/info";
static const char *accelerometer_sample_path =
    "/hardware/sensors/accelerometer/sample";
static const char *accelerometer_events_path =
    "/hardware/sensors/accelerometer/events";
static const char *accelerometer_control_path =
    "/hardware/sensors/accelerometer/control";

/* ALooper accepts any non-negative application-defined identifier. */
static const int sensor_looper_id = 1;

static volatile sig_atomic_t stop_requested = 0;

static void request_stop(int signal_number)
{
    (void)signal_number;
    stop_requested = 1;
}

static void print_usage(const char *program)
{
    fprintf(stderr,
            "usage: %s read PATH [event_count]\n"
            "\n"
            "paths:\n"
            "  %s\n"
            "  %s\n"
            "  %s\n"
            "  %s\n",
            program,
            accelerometer_info_path,
            accelerometer_sample_path,
            accelerometer_events_path,
            accelerometer_control_path);
}

static int parse_event_count(const char *text, long *event_count)
{
    char *end = NULL;
    long value = strtol(text, &end, 10);

    if (text[0] == '\0' || end == NULL || *end != '\0' || value < 1) {
        return -1;
    }

    *event_count = value;
    return 0;
}

static int write_info(const struct android_accelerometer *source)
{
    printf("name\t%s\n", android_accelerometer_name(source));
    printf("vendor\t%s\n", android_accelerometer_vendor(source));
    printf("type\t%d\n", android_accelerometer_type(source));
    printf("units\tm/s^2\n");
    printf("resolution\t%.9g\n", android_accelerometer_resolution(source));
    printf("minimum_delay_us\t%d\n", android_accelerometer_minimum_delay_us(source));
    return 0;
}

static int configure_signal_handlers(void)
{
    struct sigaction action;

    memset(&action, 0, sizeof(action));
    action.sa_handler = request_stop;
    sigemptyset(&action.sa_mask);

    if (sigaction(SIGINT, &action, NULL) != 0) {
        perror("sigaction(SIGINT)");
        return -1;
    }
    if (sigaction(SIGTERM, &action, NULL) != 0) {
        perror("sigaction(SIGTERM)");
        return -1;
    }
    return 0;
}

static int stream_accelerometer(
    struct android_accelerometer *source,
    int include_timestamp,
    long event_limit)
{
    const int requested_period_us = 20000;
    long emitted = 0;
    int result = 1;

    enum android_accelerometer_status enable_status =
        android_accelerometer_enable(source, requested_period_us);
    if (enable_status != ANDROID_ACCELEROMETER_OK) {
        fprintf(stderr, "%s\n", android_accelerometer_status_text(enable_status));
        return 1;
    }

    if (configure_signal_handlers() != 0) {
        goto cleanup;
    }

    while (!stop_requested && (event_limit == 0 || emitted < event_limit)) {
        int poll_result = ALooper_pollOnce(-1, NULL, NULL, NULL);

        if (poll_result == ALOOPER_POLL_ERROR) {
            fprintf(stderr, "Android event loop reported an error\n");
            goto cleanup;
        }

        for (;;) {
            struct android_accelerometer_reading reading;
            enum android_accelerometer_read_result read_result =
                android_accelerometer_next(source, &reading);

            if (read_result == ANDROID_ACCELEROMETER_READ_ERROR) {
                fprintf(stderr, "could not read an Android accelerometer event\n");
                goto cleanup;
            }
            if (read_result == ANDROID_ACCELEROMETER_READ_EMPTY) {
                break;
            }

            if (include_timestamp) {
                printf("%" PRId64 " %.9g %.9g %.9g\n",
                       reading.timestamp_ns,
                       reading.x,
                       reading.y,
                       reading.z);
            } else {
                printf("%.9g %.9g %.9g\n",
                       reading.x,
                       reading.y,
                       reading.z);
            }
            fflush(stdout);
            emitted += 1;

            if (event_limit != 0 && emitted >= event_limit) {
                break;
            }
        }
    }

    result = 0;

cleanup:
    android_accelerometer_disable(source);
    return result;
}

int main(int argc, char **argv)
{
    const char *path;
    long event_count = 0;

    if (argc < 3 || strcmp(argv[1], "read") != 0) {
        print_usage(argv[0]);
        return 2;
    }

    path = argv[2];

    if (strcmp(path, accelerometer_control_path) == 0) {
        fprintf(stderr,
                "%s is reserved for explicit sensor settings; "
                "write semantics are not implemented yet\n",
                accelerometer_control_path);
        return 3;
    }

    if (strcmp(path, accelerometer_events_path) == 0 && argc == 4) {
        if (parse_event_count(argv[3], &event_count) != 0) {
            fprintf(stderr, "event_count must be a positive integer\n");
            return 2;
        }
    } else if (argc != 3) {
        print_usage(argv[0]);
        return 2;
    }

    ALooper *looper = ALooper_prepare(ALOOPER_PREPARE_ALLOW_NON_CALLBACKS);
    if (looper == NULL) {
        fprintf(stderr, "Android event loop is unavailable\n");
        return 1;
    }

    struct android_accelerometer source;
    enum android_accelerometer_status open_status =
        android_accelerometer_open(&source, looper, sensor_looper_id, NULL);
    if (open_status != ANDROID_ACCELEROMETER_OK) {
        fprintf(stderr, "%s\n", android_accelerometer_status_text(open_status));
        return 1;
    }

    int result;
    if (strcmp(path, accelerometer_info_path) == 0) {
        result = write_info(&source);
    } else if (strcmp(path, accelerometer_sample_path) == 0) {
        result = stream_accelerometer(&source, 0, 1);
    } else if (strcmp(path, accelerometer_events_path) == 0) {
        result = stream_accelerometer(&source, 1, event_count);
    } else {
        fprintf(stderr, "unknown hardware path: %s\n", path);
        print_usage(argv[0]);
        result = 2;
    }

    android_accelerometer_close(&source);
    return result;
}
