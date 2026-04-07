#ifndef EVENT_LOG_H
#define EVENT_LOG_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void event_log_init(void);
void event_log_push(const char *fmt, ...);
uint32_t event_log_count(void);

#ifdef __cplusplus
}
#endif

#endif
