#ifndef DRV_MPU6050_H
#define DRV_MPU6050_H

#include <stdbool.h>
#include "app_types.h"

bool mpu6050_init(void);
bool mpu6050_read(mpu6050_data_t *out);

#endif
