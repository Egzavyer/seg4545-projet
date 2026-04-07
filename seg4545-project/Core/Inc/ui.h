#ifndef UI_H
#define UI_H

#include "app_types.h"

void ui_init(void);
void ui_update(const system_snapshot_t *snapshot);
void ui_acknowledge(void);

#endif
