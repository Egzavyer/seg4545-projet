#ifndef DRV_MQ2_H
#define DRV_MQ2_H

#include <stdbool.h>
#include <stdint.h>
#include "app_types.h"

bool mq2_init(void);
bool mq2_read(mq2_data_t *out);

#endif
