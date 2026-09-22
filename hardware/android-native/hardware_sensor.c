#include "accelerometer_snapshot.h"
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
static const char *accelerometer_inspect_path =
    "/hardware/sensors/accelerometer/inspect";
static const char *accelerometer_inspect_events_path =
    "/hardware/sensors/accelerometer/inspect-events";
static const char *accelerometer_control_path =
    "/hardware/sensors/accelerometer/control";

/* ALooper accepts any non-negative application-defined identifier. */
static const int sensor_looper_id = 1;

static volatile sig_atomic_t stop_requested = 0;

enum output_kind {
    OUTPUT_ANDROID_READING = 0,
    OUTPUT_INSPECTION
};

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
            "  %s\n"
            "  %s\n"
            "  %s\n",
            program,
            accelerometer_info_path,
            accelerometer_sample_path,
            accelerometer_events_path,
            accelerometer_inspect_path,
            accelerometer_inspect_events_path,
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

static const char *encode_status_text(enum compact_acceleration_encode_status status)
{
    switch (status) {
        case COMPACT_ACCELERATION_ENCODE_OK:
            return "ok";
        case COMPACT_ACCELERATION_ENCODE_SATURATED:
            return "saturated";
        case COMPACT_ACCELERATION_ENCODE_NONFINITE:
            return "nonfinite";
        default:
            return "unknown";
    }
}

static int write_inspection(
    const struct android_accelerometer_reading *reading,
    long record_index)
{
    struct physical_acceleration android_reading = {
        reading->x,
        reading->y,
        reading->z};
    struct accelerometer_snapshot snapshot;
    enum accelerometer_snapshot_status status =
        accelerometer_snapshot_make(android_reading, &snapshot);

    if (status == ACCELEROMETER_SNAPSHOT_NONFINITE) {
        fprintf(stderr, "Android accelerometer reading contains a nonfinite value\n");
        return -1;
    }
    if (status == ACCELEROMETER_SNAPSHOT_MALFORMED) {
        fprintf(stderr, "compact accelerometer state could not be reconstructed\n");
        return -1;
    }
    if (status == ACCELEROMETER_SNAPSHOT_DISPLAY_UNAVAILABLE) {
        fprintf(stderr, "reconstructed accelerometer value cannot be shown as sevenths\n");
        return -1;
    }

    struct physical_acceleration balanced =
        compact_acceleration_balanced_reference();
    char screen_x[16];
    char screen_y[16];
    char screen_z[16];
    if (!sevenths_display_format_ascii(&snapshot.screen_x, screen_x, sizeof(screen_x)) ||
        !sevenths_display_format_ascii(&snapshot.screen_y, screen_y, sizeof(screen_y)) ||
        !sevenths_display_format_ascii(&snapshot.screen_z, screen_z, sizeof(screen_z))) {
        fprintf(stderr, "could not format the screen values\n");
        return -1;
    }

    if (record_index >= 0) {
        printf("record.index\t%ld\n", record_index);
    }

    printf("Android accelerometer reading\n");
    printf("android.timestamp_ns\t%" PRId64 "\n", reading->timestamp_ns);
    printf("android.x_m_per_s2\t%.9g\n", snapshot.android_reading.x);
    printf("android.y_m_per_s2\t%.9g\n", snapshot.android_reading.y);
    printf("android.z_m_per_s2\t%.9g\n", snapshot.android_reading.z);

    printf("\nDifference from balanced gravity\n");
    printf("balanced_gravity.x_m_per_s2\t%.9g\n", balanced.x);
    printf("balanced_gravity.y_m_per_s2\t%.9g\n", balanced.y);
    printf("balanced_gravity.z_m_per_s2\t%.9g\n", balanced.z);
    printf(
        "difference.x_m_per_s2\t%.9g\n",
        snapshot.difference_from_balanced_gravity.x);
    printf(
        "difference.y_m_per_s2\t%.9g\n",
        snapshot.difference_from_balanced_gravity.y);
    printf(
        "difference.z_m_per_s2\t%.9g\n",
        snapshot.difference_from_balanced_gravity.z);
    printf("difference.magnitude_m_per_s2\t%.9g\n", snapshot.difference_magnitude);

    printf("\nResidual direction represented by compact state (unit pure quaternion)\n");
    printf(
        "residual_direction.defined\t%s\n",
        snapshot.compact_direction_defined ? "true" : "false");
    if (snapshot.compact_direction_defined) {
        printf("residual_direction.quaternion.real\t0\n");
        printf(
            "residual_direction.quaternion.i\t%.9g\n",
            snapshot.compact_direction.x);
        printf(
            "residual_direction.quaternion.j\t%.9g\n",
            snapshot.compact_direction.y);
        printf(
            "residual_direction.quaternion.k\t%.9g\n",
            snapshot.compact_direction.z);
    } else {
        printf("residual_direction.quaternion.real\tundefined\n");
        printf("residual_direction.quaternion.i\tundefined\n");
        printf("residual_direction.quaternion.j\tundefined\n");
        printf("residual_direction.quaternion.k\tundefined\n");
    }

    printf("\nCompact geometric state\n");
    printf(
        "compact.direction_bytes_hex\t%02x%02x%02x\n",
        (unsigned int)snapshot.compact_state.direction_high,
        (unsigned int)snapshot.compact_state.direction_middle,
        (unsigned int)snapshot.compact_state.direction_low);
    printf(
        "compact.magnitude_code\t%u\n",
        (unsigned int)snapshot.compact_state.magnitude);
    printf(
        "compact.residual_magnitude_m_per_s2\t%.9g\n",
        (float)snapshot.compact_state.magnitude *
            compact_acceleration_magnitude_quantum());
    printf("compact.encode_status\t%s\n", encode_status_text(snapshot.encode_status));

    printf("\nReconstructed from compact state\n");
    printf("reconstructed.x_m_per_s2\t%.9g\n", snapshot.reconstructed.x);
    printf("reconstructed.y_m_per_s2\t%.9g\n", snapshot.reconstructed.y);
    printf("reconstructed.z_m_per_s2\t%.9g\n", snapshot.reconstructed.z);

    printf("\nWhat the screen shows\n");
    printf("screen.x_m_per_s2\t%s\n", screen_x);
    printf("screen.y_m_per_s2\t%s\n", screen_y);
    printf("screen.z_m_per_s2\t%s\n", screen_z);

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
    enum output_kind output,
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

            if (output == OUTPUT_INSPECTION) {
                if (emitted != 0) {
                    printf("\n");
                }
                if (write_inspection(&reading, emitted) != 0) {
                    goto cleanup;
                }
            } else if (include_timestamp) {
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

    bool bounded_stream =
        strcmp(path, accelerometer_events_path) == 0 ||
        strcmp(path, accelerometer_inspect_events_path) == 0;
    if (bounded_stream && argc == 4) {
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
        result = stream_accelerometer(
            &source,
            OUTPUT_ANDROID_READING,
            0,
            1);
    } else if (strcmp(path, accelerometer_events_path) == 0) {
        result = stream_accelerometer(
            &source,
            OUTPUT_ANDROID_READING,
            1,
            event_count);
    } else if (strcmp(path, accelerometer_inspect_path) == 0) {
        result = stream_accelerometer(
            &source,
            OUTPUT_INSPECTION,
            0,
            1);
    } else if (strcmp(path, accelerometer_inspect_events_path) == 0) {
        result = stream_accelerometer(
            &source,
            OUTPUT_INSPECTION,
            0,
            event_count);
    } else {
        fprintf(stderr, "unknown hardware path: %s\n", path);
        print_usage(argv[0]);
        result = 2;
    }

    android_accelerometer_close(&source);
    return result;
}
