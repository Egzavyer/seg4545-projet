#ifndef DRV_DHT11_H
#define DRV_DHT11_H

#include <stdbool.h>
#include "app_types.h"

bool dht11_init(void);
bool dht11_read(dht11_data_t *out);

#endif
