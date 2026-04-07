#ifndef APP_TASKS_H
#define APP_TASKS_H

#include "cmsis_os2.h"
#include "app_types.h"

#ifdef __cplusplus
extern "C" {
#endif

extern osMutexId_t        g_i2cMutex;
extern osMessageQueueId_t g_sensorQueue;
extern osEventFlagsId_t   g_eventFlags;
extern osEventFlagsId_t   g_heartbeatFlags;
extern osMutexId_t        g_snapshotMutex;
extern system_snapshot_t  g_snapshot;

void app_tasks_create_all(void);

#ifdef __cplusplus
}
#endif

#endif
