#ifndef DRV_MAX30102_H
#define DRV_MAX30102_H

#include <stdbool.h>
#include "app_types.h"

bool max30102_init(void);
bool max30102_read(max30102_data_t *out);

#endif
