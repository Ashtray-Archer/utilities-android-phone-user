#include <android/looper.h>
#include <android/sensor.h>

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

static void request_stop(int signal_number) {
    (void) signal_number;
    stop_requested = 1;
}

static void print_usage(const char *program) {
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

static int parse_event_count(const char *text, long *event_count) {
    char *end = NULL;
    long value = strtol(text, &end, 10);

    if (text[0] == '\0' || end == NULL || *end != '\0' || value < 1) {
        return -1;
    }

    *event_count = value;
    return 0;
}

static const ASensor *find_accelerometer(ASensorManager *manager) {
    return ASensorManager_getDefaultSensor(manager, ASENSOR_TYPE_ACCELEROMETER);
}

static int write_info(const ASensor *sensor) {
    printf("name\t%s\n", ASensor_getName(sensor));
    printf("vendor\t%s\n", ASensor_getVendor(sensor));
    printf("type\t%d\n", ASensor_getType(sensor));
    printf("units\tm/s^2\n");
    printf("resolution\t%.9g\n", ASensor_getResolution(sensor));
    printf("minimum_delay_us\t%d\n", ASensor_getMinDelay(sensor));
    return 0;
}

static int configure_signal_handlers(void) {
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

static int stream_accelerometer(ASensorManager *manager,
                                const ASensor *sensor,
                                int include_timestamp,
                                long event_limit) {
    const int default_period_us = 20000;
    int period_us = default_period_us;
    int minimum_delay_us = ASensor_getMinDelay(sensor);
    long emitted = 0;
    ALooper *looper = NULL;
    ASensorEventQueue *queue = NULL;
    int result = 1;

    if (minimum_delay_us > period_us) {
        period_us = minimum_delay_us;
    }

    looper = ALooper_prepare(ALOOPER_PREPARE_ALLOW_NON_CALLBACKS);
    if (looper == NULL) {
        fprintf(stderr, "could not create Android looper\n");
        return 1;
    }

    queue = ASensorManager_createEventQueue(
        manager, looper, sensor_looper_id, NULL, NULL);
    if (queue == NULL) {
        fprintf(stderr, "could not create accelerometer event queue\n");
        return 1;
    }

    if (ASensorEventQueue_enableSensor(queue, sensor) < 0) {
        fprintf(stderr, "could not enable accelerometer\n");
        goto cleanup;
    }

    if (ASensorEventQueue_setEventRate(queue, sensor, period_us) < 0) {
        fprintf(stderr, "could not set accelerometer event rate\n");
        goto cleanup;
    }

    if (configure_signal_handlers() != 0) {
        goto cleanup;
    }

    while (!stop_requested && (event_limit == 0 || emitted < event_limit)) {
        int poll_result = ALooper_pollOnce(-1, NULL, NULL, NULL);

        if (poll_result == ALOOPER_POLL_ERROR) {
            fprintf(stderr, "Android looper reported an error\n");
            goto cleanup;
        }

        for (;;) {
            ASensorEvent event;
            ssize_t received = ASensorEventQueue_getEvents(queue, &event, 1);

            if (received < 0) {
                fprintf(stderr, "could not read accelerometer event\n");
                goto cleanup;
            }
            if (received == 0) {
                break;
            }
            if (event.type != ASENSOR_TYPE_ACCELEROMETER) {
                continue;
            }

            if (include_timestamp) {
                printf("%" PRId64 " %.9g %.9g %.9g\n",
                       event.timestamp,
                       event.acceleration.x,
                       event.acceleration.y,
                       event.acceleration.z);
            } else {
                printf("%.9g %.9g %.9g\n",
                       event.acceleration.x,
                       event.acceleration.y,
                       event.acceleration.z);
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
    ASensorEventQueue_disableSensor(queue, sensor);
    ASensorManager_destroyEventQueue(manager, queue);
    return result;
}

int main(int argc, char **argv) {
    const char *path;
    ASensorManager *manager;
    const ASensor *sensor;
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

    manager = ASensorManager_getInstance();
    if (manager == NULL) {
        fprintf(stderr, "Android sensor manager is unavailable\n");
        return 1;
    }

    sensor = find_accelerometer(manager);
    if (sensor == NULL) {
        fprintf(stderr, "no default accelerometer is available\n");
        return 1;
    }

    if (strcmp(path, accelerometer_info_path) == 0) {
        return write_info(sensor);
    }
    if (strcmp(path, accelerometer_sample_path) == 0) {
        return stream_accelerometer(manager, sensor, 0, 1);
    }
    if (strcmp(path, accelerometer_events_path) == 0) {
        return stream_accelerometer(manager, sensor, 1, event_count);
    }

    fprintf(stderr, "unknown hardware path: %s\n", path);
    print_usage(argv[0]);
    return 2;
}
