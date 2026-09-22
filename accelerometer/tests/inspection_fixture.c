#include "accelerometer_inspection.h"

#include <stdio.h>

int main(void)
{
    struct physical_acceleration reading = {1.25F, -2.5F, 9.0F};
    return accelerometer_inspection_write(
        stdout,
        stderr,
        123456789,
        reading,
        0) == 0
        ? 0
        : 1;
}
