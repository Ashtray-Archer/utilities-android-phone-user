#include "android_accelerometer.h"

#include <string.h>

static ASensorManager *sensor_manager_for(const char *package_name)
{
#if __ANDROID_API__ >= 26
    if (package_name != NULL && package_name[0] != '\0') {
        return ASensorManager_getInstanceForPackage(package_name);
    }
#else
    (void)package_name;
#endif

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#endif
    ASensorManager *manager = ASensorManager_getInstance();
#if defined(__clang__)
#pragma clang diagnostic pop
#endif
    return manager;
}

enum android_accelerometer_status android_accelerometer_open(
    struct android_accelerometer *source,
    ALooper *looper,
    int looper_id,
    const char *package_name)
{
    (void)memset(source, 0, sizeof(*source));

    if (looper == NULL) {
        return ANDROID_ACCELEROMETER_NO_LOOPER;
    }

    source->manager = sensor_manager_for(package_name);
    if (source->manager == NULL) {
        return ANDROID_ACCELEROMETER_NO_MANAGER;
    }

    source->sensor = ASensorManager_getDefaultSensor(
        source->manager,
        ASENSOR_TYPE_ACCELEROMETER);
    if (source->sensor == NULL) {
        return ANDROID_ACCELEROMETER_NO_SENSOR;
    }

    source->queue = ASensorManager_createEventQueue(
        source->manager,
        looper,
        looper_id,
        NULL,
        NULL);
    if (source->queue == NULL) {
        return ANDROID_ACCELEROMETER_NO_QUEUE;
    }

    return ANDROID_ACCELEROMETER_OK;
}

enum android_accelerometer_status android_accelerometer_enable(
    struct android_accelerometer *source,
    int requested_period_us)
{
    if (source->queue == NULL || source->sensor == NULL) {
        return ANDROID_ACCELEROMETER_NO_QUEUE;
    }
    if (source->enabled) {
        return ANDROID_ACCELEROMETER_OK;
    }

    if (ASensorEventQueue_enableSensor(source->queue, source->sensor) < 0) {
        return ANDROID_ACCELEROMETER_ENABLE_FAILED;
    }

    int period_us = requested_period_us;
    int minimum_delay_us = ASensor_getMinDelay(source->sensor);
    if (minimum_delay_us > period_us) {
        period_us = minimum_delay_us;
    }

    if (ASensorEventQueue_setEventRate(source->queue, source->sensor, period_us) < 0) {
        (void)ASensorEventQueue_disableSensor(source->queue, source->sensor);
        return ANDROID_ACCELEROMETER_RATE_FAILED;
    }

    source->enabled = true;
    return ANDROID_ACCELEROMETER_OK;
}

void android_accelerometer_disable(struct android_accelerometer *source)
{
    if (source->queue != NULL && source->sensor != NULL && source->enabled) {
        (void)ASensorEventQueue_disableSensor(source->queue, source->sensor);
    }
    source->enabled = false;
}

void android_accelerometer_close(struct android_accelerometer *source)
{
    android_accelerometer_disable(source);
    if (source->manager != NULL && source->queue != NULL) {
        (void)ASensorManager_destroyEventQueue(source->manager, source->queue);
    }
    (void)memset(source, 0, sizeof(*source));
}

enum android_accelerometer_read_result android_accelerometer_next(
    struct android_accelerometer *source,
    struct android_accelerometer_reading *reading)
{
    if (source->queue == NULL) {
        return ANDROID_ACCELEROMETER_READ_ERROR;
    }

    for (;;) {
        ASensorEvent event;
        ssize_t received = ASensorEventQueue_getEvents(source->queue, &event, 1);
        if (received < 0) {
            return ANDROID_ACCELEROMETER_READ_ERROR;
        }
        if (received == 0) {
            return ANDROID_ACCELEROMETER_READ_EMPTY;
        }
        if (event.type != ASENSOR_TYPE_ACCELEROMETER) {
            continue;
        }

        reading->timestamp_ns = event.timestamp;
        reading->x = event.acceleration.x;
        reading->y = event.acceleration.y;
        reading->z = event.acceleration.z;
        return ANDROID_ACCELEROMETER_READ_AVAILABLE;
    }
}

bool android_accelerometer_is_available(const struct android_accelerometer *source)
{
    return source->sensor != NULL && source->queue != NULL;
}

bool android_accelerometer_is_enabled(const struct android_accelerometer *source)
{
    return source->enabled;
}

const char *android_accelerometer_name(const struct android_accelerometer *source)
{
    return source->sensor == NULL ? NULL : ASensor_getName(source->sensor);
}

const char *android_accelerometer_vendor(const struct android_accelerometer *source)
{
    return source->sensor == NULL ? NULL : ASensor_getVendor(source->sensor);
}

int android_accelerometer_type(const struct android_accelerometer *source)
{
    return source->sensor == NULL ? 0 : ASensor_getType(source->sensor);
}

float android_accelerometer_resolution(const struct android_accelerometer *source)
{
    return source->sensor == NULL ? 0.0F : ASensor_getResolution(source->sensor);
}

int android_accelerometer_minimum_delay_us(const struct android_accelerometer *source)
{
    return source->sensor == NULL ? 0 : ASensor_getMinDelay(source->sensor);
}

const char *android_accelerometer_status_text(enum android_accelerometer_status status)
{
    switch (status) {
        case ANDROID_ACCELEROMETER_OK:
            return "Android accelerometer is ready";
        case ANDROID_ACCELEROMETER_NO_LOOPER:
            return "Android event loop is unavailable";
        case ANDROID_ACCELEROMETER_NO_MANAGER:
            return "Android sensor manager is unavailable";
        case ANDROID_ACCELEROMETER_NO_SENSOR:
            return "no default accelerometer is available";
        case ANDROID_ACCELEROMETER_NO_QUEUE:
            return "Android accelerometer event queue is unavailable";
        case ANDROID_ACCELEROMETER_ENABLE_FAILED:
            return "could not enable the Android accelerometer";
        case ANDROID_ACCELEROMETER_RATE_FAILED:
            return "could not set the Android accelerometer event rate";
        default:
            return "unknown Android accelerometer error";
    }
}
