#ifndef ANDROID_ACCELEROMETER_H
#define ANDROID_ACCELEROMETER_H

#include <android/looper.h>
#include <android/sensor.h>

#include <stdbool.h>
#include <stdint.h>

/*
 * Android accelerometer reading
 *
 * This is the acceleration measurement reported to an ordinary program by
 * Android's sensor stack: three IEEE binary32 components in m/s^2, plus the
 * Android sensor-event timestamp.
 *
 * It is the application-side oracle for representations derived above this
 * boundary. It is not an ADC count, sensor-register value, or direct reading
 * of the accelerometer chip's bus protocol.
 */
struct android_accelerometer_reading {
    int64_t timestamp_ns;
    float x;
    float y;
    float z;
};

enum android_accelerometer_status {
    ANDROID_ACCELEROMETER_OK = 0,
    ANDROID_ACCELEROMETER_NO_LOOPER,
    ANDROID_ACCELEROMETER_NO_MANAGER,
    ANDROID_ACCELEROMETER_NO_SENSOR,
    ANDROID_ACCELEROMETER_NO_QUEUE,
    ANDROID_ACCELEROMETER_ENABLE_FAILED,
    ANDROID_ACCELEROMETER_RATE_FAILED
};

enum android_accelerometer_read_result {
    ANDROID_ACCELEROMETER_READ_ERROR = -1,
    ANDROID_ACCELEROMETER_READ_EMPTY = 0,
    ANDROID_ACCELEROMETER_READ_AVAILABLE = 1
};

struct android_accelerometer {
    ASensorManager *manager;
    const ASensor *sensor;
    ASensorEventQueue *queue;
    bool enabled;
};

enum android_accelerometer_status android_accelerometer_open(
    struct android_accelerometer *source,
    ALooper *looper,
    int looper_id,
    const char *package_name);

enum android_accelerometer_status android_accelerometer_enable(
    struct android_accelerometer *source,
    int requested_period_us);

void android_accelerometer_disable(struct android_accelerometer *source);
void android_accelerometer_close(struct android_accelerometer *source);

enum android_accelerometer_read_result android_accelerometer_next(
    struct android_accelerometer *source,
    struct android_accelerometer_reading *reading);

bool android_accelerometer_is_available(const struct android_accelerometer *source);
bool android_accelerometer_is_enabled(const struct android_accelerometer *source);

const char *android_accelerometer_name(const struct android_accelerometer *source);
const char *android_accelerometer_vendor(const struct android_accelerometer *source);
int android_accelerometer_type(const struct android_accelerometer *source);
float android_accelerometer_resolution(const struct android_accelerometer *source);
int android_accelerometer_minimum_delay_us(const struct android_accelerometer *source);

const char *android_accelerometer_status_text(enum android_accelerometer_status status);

#endif
