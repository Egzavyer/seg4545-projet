#include "app_tasks.h"

#include <stdio.h>
#include <string.h>

#include "main.h"
#include "app_config.h"
#include "app_types.h"
#include "drv_dht11.h"
#include "drv_ds18b20.h"
#include "drv_max30102.h"
#include "drv_mpu6050.h"
#include "drv_mq2.h"
#include "drv_esp_uart.h"
#include "debug.h"
#include "event_log.h"
#include "ui.h"

extern I2C_HandleTypeDef hi2c1;
extern UART_HandleTypeDef huart1;
extern UART_HandleTypeDef huart2;
extern ADC_HandleTypeDef hadc1;

static uint32_t s_max_last_good_ms = 0U;
static float s_max_last_good_hr = 0.0f;
static float s_max_last_good_spo2 = 0.0f;

typedef enum {
	APP_TASK_DHT = 0,
	APP_TASK_DS18,
	APP_TASK_MAX,
	APP_TASK_MPU,
	APP_TASK_MQ2,
	APP_TASK_DECISION,
	APP_TASK_UI,
	APP_TASK_ESP,
	APP_TASK_COUNT
} app_task_id_t;

typedef enum {
	DEMO_MODE_LIVE = 0,
	DEMO_MODE_FORCE_WARNING,
	DEMO_MODE_FORCE_ALARM,
	DEMO_MODE_FORCE_DEGRADED,
	DEMO_MODE_FORCE_FAULT,
	DEMO_MODE_COUNT
} demo_mode_t;

typedef struct {
	uint8_t temp_warn;
	uint8_t humidity_warn;
	uint8_t mq2_warn;
	uint8_t hr_warn;
	uint8_t spo2_warn;
	uint8_t motion_warn;
	uint8_t body_temp_low_warn;
	uint8_t body_temp_high_warn;

	uint8_t mq2_alarm;
	uint8_t hr_alarm;
	uint8_t spo2_alarm;
	uint8_t motion_alarm;
	uint8_t body_temp_low_alarm;
	uint8_t body_temp_high_alarm;
} cause_flags_t;

osMutexId_t g_i2cMutex = NULL;
osMessageQueueId_t g_sensorQueue = NULL;
osEventFlagsId_t g_eventFlags = NULL;
osEventFlagsId_t g_heartbeatFlags = NULL;
osMutexId_t g_snapshotMutex = NULL;
system_snapshot_t g_snapshot = { 0 };

static volatile uint32_t s_lastHeartbeatMs[APP_TASK_COUNT] = { 0U };
static cause_flags_t s_causes = { 0 };
static volatile demo_mode_t s_demoMode = DEMO_MODE_LIVE;

static osThreadId_t decisionTaskHandle;

static void DhtTask(void *argument);
static void MaxTask(void *argument);
static void MpuTask(void *argument);
static void Mq2Task(void *argument);
static void DecisionTask(void *argument);
static void UiTask(void *argument);
static void EspTask(void *argument);
static void SupervisorTask(void *argument);
static void Ds18Task(void *argument);

static void publish_sensor_msg(const sensor_msg_t *msg);
static void heartbeat(app_task_id_t id);
static void set_system_state(system_state_t state);
static void evaluate_snapshot(system_snapshot_t *snapshot);
static const char* state_to_str(system_state_t state);
static void decision_warmup_with_queue_drain(uint32_t delay_ms);
static void update_snapshot_from_msg(const sensor_msg_t *msg);
static uint8_t check_task_deadline(app_task_id_t id, uint32_t timeout_ms,
		uint32_t now_ms);
static void log_state_causes(const system_snapshot_t *snapshot);
static const char* highest_cause_name(void);
static const char* demo_mode_name(demo_mode_t mode);

static inline void red_led_set(uint8_t on) {
#if APP_RED_LED_ACTIVE_LOW
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_2, on ? GPIO_PIN_RESET : GPIO_PIN_SET);
#else
	HAL_GPIO_WritePin(GPIOB, GPIO_PIN_2, on ? GPIO_PIN_SET : GPIO_PIN_RESET);
#endif
}

static inline void red_led_toggle(void) {
	HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_2);
}

static inline void green_led_set(uint8_t on) {
	HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, on ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

static inline void green_led_toggle(void) {
	HAL_GPIO_TogglePin(GPIOA, GPIO_PIN_5);
}

static inline void buzzer_set(uint8_t on) {
#if APP_BUZZER_ACTIVE_LOW
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_1, on ? GPIO_PIN_RESET : GPIO_PIN_SET);
#else
	HAL_GPIO_WritePin(GPIOB, GPIO_PIN_1, on ? GPIO_PIN_SET : GPIO_PIN_RESET);
#endif
}

static inline void buzzer_toggle(void) {
	HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_1);
}

static const osThreadAttr_t dhtTaskAttr = { .name = "dhtTask", .priority =
		osPriorityNormal, .stack_size = 1024 };
static const osThreadAttr_t maxTaskAttr = { .name = "maxTask", .priority =
		osPriorityAboveNormal, .stack_size = 1280 };
static const osThreadAttr_t mpuTaskAttr = { .name = "mpuTask", .priority =
		osPriorityAboveNormal, .stack_size = 1024 };
static const osThreadAttr_t mq2TaskAttr = { .name = "mq2Task", .priority =
		osPriorityNormal, .stack_size = 1024 };
static const osThreadAttr_t decisionTaskAttr = { .name = "decisionTask",
		.priority = osPriorityHigh, .stack_size = 2048 };
static const osThreadAttr_t uiTaskAttr = { .name = "uiTask", .priority =
		osPriorityBelowNormal, .stack_size = 1024 };
static const osThreadAttr_t espTaskAttr = { .name = "espTask", .priority =
		osPriorityBelowNormal, .stack_size = 1280 };
static const osThreadAttr_t supervisorTaskAttr = { .name = "supervisorTask",
		.priority = osPriorityAboveNormal, .stack_size = 1024 };
static const osThreadAttr_t ds18TaskAttr = { .name = "ds18Task", .priority =
		osPriorityNormal, .stack_size = 768 };

void app_tasks_create_all(void) {
	const osMutexAttr_t i2cMutexAttr = { .name = "i2cMutex" };
	const osMutexAttr_t snapshotMutexAttr = { .name = "snapshotMutex" };

	debug_init();
	debug_log("\r\n[BOOT] app_tasks_create_all()\r\n");

	event_log_init();

	g_i2cMutex = osMutexNew(&i2cMutexAttr);
	g_snapshotMutex = osMutexNew(&snapshotMutexAttr);
	g_sensorQueue = osMessageQueueNew(SENSOR_QUEUE_LENGTH, sizeof(sensor_msg_t),
	NULL);
	g_eventFlags = osEventFlagsNew(NULL);
	g_heartbeatFlags = osEventFlagsNew(NULL);

	memset((void*) s_lastHeartbeatMs, 0, sizeof(s_lastHeartbeatMs));
	memset(&g_snapshot, 0, sizeof(g_snapshot));
	memset(&s_causes, 0, sizeof(s_causes));
	g_snapshot.state = SYS_BOOT;
	g_snapshot.wifi_link_ok = false;

	osThreadId_t dhtHandle;
	osThreadId_t ds18Handle;
	osThreadId_t maxHandle;
	osThreadId_t mpuHandle;
	osThreadId_t mq2Handle;
	osThreadId_t decisionHandle;
	osThreadId_t uiHandle;
	osThreadId_t espHandle;
	osThreadId_t supervisorHandle;

	dhtHandle = osThreadNew(DhtTask, NULL, &dhtTaskAttr);
	debug_log("[TASK] DHT %s\r\n", dhtHandle ? "OK" : "FAIL");

	ds18Handle = osThreadNew(Ds18Task, NULL, &ds18TaskAttr);
	debug_log("[TASK] DS18 %s\r\n", ds18Handle ? "OK" : "FAIL");

	maxHandle = osThreadNew(MaxTask, NULL, &maxTaskAttr);
	debug_log("[TASK] MAX %s\r\n", maxHandle ? "OK" : "FAIL");

	mpuHandle = osThreadNew(MpuTask, NULL, &mpuTaskAttr);
	debug_log("[TASK] MPU %s\r\n", mpuHandle ? "OK" : "FAIL");

	mq2Handle = osThreadNew(Mq2Task, NULL, &mq2TaskAttr);
	debug_log("[TASK] MQ2 %s\r\n", mq2Handle ? "OK" : "FAIL");

	decisionHandle = osThreadNew(DecisionTask, NULL, &decisionTaskAttr);
	debug_log("[TASK] DECISION %s\r\n", decisionHandle ? "OK" : "FAIL");

	uiHandle = osThreadNew(UiTask, NULL, &uiTaskAttr);
	debug_log("[TASK] UI %s\r\n", uiHandle ? "OK" : "FAIL");

	espHandle = osThreadNew(EspTask, NULL, &espTaskAttr);
	debug_log("[TASK] ESP %s\r\n", espHandle ? "OK" : "FAIL");

	supervisorHandle = osThreadNew(SupervisorTask, NULL, &supervisorTaskAttr);
	debug_log("[TASK] SUPERVISOR %s\r\n", supervisorHandle ? "OK" : "FAIL");
}

static void publish_sensor_msg(const sensor_msg_t *msg) {
	if (msg == NULL)
		return;

	if (osMessageQueuePut(g_sensorQueue, msg, 0U, 0U) != osOK) {
		debug_log("[QUEUE] drop id=%u count=%lu\r\n", (unsigned) msg->id,
				(unsigned long) osMessageQueueGetCount(g_sensorQueue));
		event_log_push("queue drop id=%u", (unsigned) msg->id);
	}
}

static void heartbeat(app_task_id_t id) {
	if (id < APP_TASK_COUNT) {
		s_lastHeartbeatMs[id] = osKernelGetTickCount();
	}
}

static void set_system_state(system_state_t state) {
	static system_state_t previous = SYS_BOOT;

	osMutexAcquire(g_snapshotMutex, osWaitForever);
	g_snapshot.state = state;
	osMutexRelease(g_snapshotMutex);

	if (state != previous) {
		debug_log("[STATE] %s -> %s\r\n", state_to_str(previous),
				state_to_str(state));
		event_log_push("state %s->%s", state_to_str(previous),
				state_to_str(state));
		previous = state;
	}

	(void) osEventFlagsSet(g_eventFlags, EVT_NEW_STATUS);
}

static void DhtTask(void *argument) {
	(void) argument;
	sensor_msg_t msg = { 0 };
	uint32_t next = osKernelGetTickCount();
	uint8_t consecutive_failures = 0U;
	uint8_t had_good_data = 0U;

	had_good_data = dht11_init() ? 1U : 0U;
	debug_log("[DHT11] init %s\r\n", had_good_data ? "OK" : "FAIL");

	osDelay(1500U);

	for (;;) {
		next += DHT_TASK_PERIOD_MS;
		msg.id = SENSOR_ID_DHT11;
		msg.tick_ms = osKernelGetTickCount();

		if (dht11_read(&msg.data.dht11)) {
			msg.status = SENSOR_STATUS_OK;
			consecutive_failures = 0U;

			if (!had_good_data) {
				debug_log("[DHT11] recovered T=%.1f H=%.1f\r\n",
						msg.data.dht11.ambient_temp_c,
						msg.data.dht11.humidity_pct);
			}

			had_good_data = 1U;
			publish_sensor_msg(&msg);
		} else {
			consecutive_failures++;

			if (consecutive_failures >= DHT_CONSECUTIVE_FAIL_LIMIT) {
				msg.status = SENSOR_STATUS_NO_DATA;

				if (had_good_data) {
					debug_log("[DHT11] read failed\r\n");
				}

				had_good_data = 0U;
				publish_sensor_msg(&msg);
			}
		}

		heartbeat(APP_TASK_DHT);
		osDelayUntil(next);
	}
}

static void MaxTask(void *argument) {
	(void) argument;
	sensor_msg_t msg = { 0 };
	uint32_t next = osKernelGetTickCount();

	if (g_i2cMutex != NULL) {
		osMutexAcquire(g_i2cMutex, osWaitForever);
		msg.status =
				max30102_init() ? SENSOR_STATUS_OK : SENSOR_STATUS_NOT_PRESENT;
		osMutexRelease(g_i2cMutex);
	} else {
		msg.status = SENSOR_STATUS_COMM_ERROR;
	}

	debug_log("[MAX30102] init %s\r\n",
			(msg.status == SENSOR_STATUS_OK) ? "OK" : "FAIL");

	for (;;) {
		next += MAX_TASK_PERIOD_MS;
		msg.id = SENSOR_ID_MAX30102;
		msg.tick_ms = osKernelGetTickCount();

		if (g_i2cMutex != NULL) {
			osMutexAcquire(g_i2cMutex, osWaitForever);

			if (max30102_read(&msg.data.max30102)) {
				msg.status = SENSOR_STATUS_OK;
			} else {
				msg.status = SENSOR_STATUS_NO_DATA;
			}

			osMutexRelease(g_i2cMutex);
		} else {
			msg.status = SENSOR_STATUS_COMM_ERROR;
		}

		publish_sensor_msg(&msg);
		heartbeat(APP_TASK_MAX);
		osDelayUntil(next);
	}
}

static void MpuTask(void *argument) {
	(void) argument;
	sensor_msg_t msg = { 0 };
	uint32_t next = osKernelGetTickCount();
	uint8_t last_ok = 0U;

	if (g_i2cMutex != NULL) {
		osMutexAcquire(g_i2cMutex, osWaitForever);
		last_ok = mpu6050_init() ? 1U : 0U;
		osMutexRelease(g_i2cMutex);
	}
	debug_log("[MPU6050] init %s\r\n", last_ok ? "OK" : "FAIL");

	for (;;) {
		next += MPU_TASK_PERIOD_MS;
		msg.id = SENSOR_ID_MPU6050;
		msg.tick_ms = osKernelGetTickCount();

		if (g_i2cMutex != NULL) {
			osMutexAcquire(g_i2cMutex, osWaitForever);
			if (mpu6050_read(&msg.data.mpu6050)) {
				msg.status = SENSOR_STATUS_OK;
				last_ok = 1U;
			} else {
				msg.status = SENSOR_STATUS_NO_DATA;
				if (last_ok != 0U) {
					debug_log("[MPU6050] read failed\r\n");
					event_log_push("MPU fail");
				}
				last_ok = 0U;
			}
			osMutexRelease(g_i2cMutex);
		} else {
			msg.status = SENSOR_STATUS_COMM_ERROR;
			last_ok = 0U;
		}

		publish_sensor_msg(&msg);
		heartbeat(APP_TASK_MPU);
		osDelayUntil(next);
	}
}

static void Mq2Task(void *argument) {
	(void) argument;
	sensor_msg_t msg = { 0 };
	uint32_t next = osKernelGetTickCount();
	uint8_t last_ok = 0U;

	last_ok = mq2_init() ? 1U : 0U;
	debug_log("[MQ2] init %s\r\n", last_ok ? "OK" : "FAIL");

	for (;;) {
		next += MQ2_TASK_PERIOD_MS;
		msg.id = SENSOR_ID_MQ2;
		msg.tick_ms = osKernelGetTickCount();

		if (mq2_read(&msg.data.mq2)) {
			msg.status = SENSOR_STATUS_OK;
			last_ok = 1U;
		} else {
			msg.status = SENSOR_STATUS_NO_DATA;
			if (last_ok != 0U) {
				debug_log("[MQ2] read failed\r\n");
				event_log_push("MQ2 fail");
			}
			last_ok = 0U;
		}

		publish_sensor_msg(&msg);
		heartbeat(APP_TASK_MQ2);
		osDelayUntil(next);
	}
}

static void Ds18Task(void *argument) {
	(void) argument;
	sensor_msg_t msg = { 0 };
	uint32_t next = osKernelGetTickCount();
	uint8_t last_ok = 0U;
	float temp_c = 0.0f;
	uint8_t conversion_started = 0U;

	last_ok = ds18b20_init() ? 1U : 0U;
	debug_log("[DS18B20] init %s\r\n", last_ok ? "OK" : "FAIL");

	for (;;) {
		next += DS18_TASK_PERIOD_MS;
		msg.id = SENSOR_ID_DS18B20;
		msg.tick_ms = osKernelGetTickCount();

		if (!conversion_started) {
			conversion_started = ds18b20_start_conversion() ? 1U : 0U;

			if (!conversion_started) {
				msg.status = SENSOR_STATUS_NO_DATA;
				msg.data.ds18b20.valid = false;

				if (last_ok != 0U) {
					debug_log("[DS18B20] start conversion failed\r\n");
				}
				last_ok = 0U;

				publish_sensor_msg(&msg);
				heartbeat(APP_TASK_DS18);
				osDelayUntil(next);
				continue;
			}

			/* Give the sensor time to convert without blocking the CPU */
			osDelay(750U);
		}

		if (ds18b20_read_temp_c(&temp_c)) {
			msg.status = SENSOR_STATUS_OK;
			msg.data.ds18b20.body_temp_c = temp_c;
			msg.data.ds18b20.valid = true;

			if (last_ok == 0U) {
				debug_log("[DS18oB20] recovered T=%.2fC\r\n", temp_c);
			}
			last_ok = 1U;
		} else {
			msg.status = SENSOR_STATUS_NO_DATA;
			msg.data.ds18b20.valid = false;

			if (last_ok != 0U) {
				debug_log("[DS18B20] read failed\r\n");
			}
			last_ok = 0U;
		}

		conversion_started = 0U;
		publish_sensor_msg(&msg);
		heartbeat(APP_TASK_DS18);
		osDelayUntil(next);
	}
}

static void update_snapshot_from_msg(const sensor_msg_t *msg) {
	switch (msg->id) {
	case SENSOR_ID_DHT11:
		if (msg->status == SENSOR_STATUS_OK) {
			g_snapshot.dht11 = msg->data.dht11;
		} else {
			g_snapshot.dht11.valid = false;
		}
		break;

	case SENSOR_ID_MAX30102:
		if (msg->status == SENSOR_STATUS_OK) {
			g_snapshot.max30102 = msg->data.max30102;

			if (g_snapshot.max30102.valid) {
				s_max_last_good_ms = HAL_GetTick();
				s_max_last_good_hr = g_snapshot.max30102.heart_rate_bpm;
				s_max_last_good_spo2 = g_snapshot.max30102.spo2_pct;
			} else {
				if (g_snapshot.max30102.finger_present
						&& ((HAL_GetTick() - s_max_last_good_ms)
								<= MAX_DISPLAY_HOLD_MS)) {
					g_snapshot.max30102.valid = true;
					g_snapshot.max30102.heart_rate_bpm = s_max_last_good_hr;
					g_snapshot.max30102.spo2_pct = s_max_last_good_spo2;
				} else {
					g_snapshot.max30102.heart_rate_bpm = 0.0f;
					g_snapshot.max30102.spo2_pct = 0.0f;
				}
			}
		} else {
			g_snapshot.max30102.valid = false;
			g_snapshot.max30102.signal_ok = false;
			g_snapshot.max30102.heart_rate_bpm = 0.0f;
			g_snapshot.max30102.spo2_pct = 0.0f;
		}
		break;

	case SENSOR_ID_MPU6050:
		if (msg->status == SENSOR_STATUS_OK) {
			g_snapshot.mpu6050 = msg->data.mpu6050;
		} else {
			g_snapshot.mpu6050.valid = false;
		}
		break;

	case SENSOR_ID_MQ2:
		if (msg->status == SENSOR_STATUS_OK) {
			g_snapshot.mq2 = msg->data.mq2;
		} else {
			g_snapshot.mq2.valid = false;
		}
		break;

	case SENSOR_ID_DS18B20:
		if (msg->status == SENSOR_STATUS_OK) {
			g_snapshot.ds18b20 = msg->data.ds18b20;
		} else {
			g_snapshot.ds18b20.valid = false;
		}
		break;

	default:
		break;
	}
}

static void evaluate_snapshot(system_snapshot_t *snapshot) {
	static uint32_t hrWarnCount = 0U;
	static uint32_t hrAlarmCount = 0U;
	static uint32_t spo2WarnCount = 0U;
	static uint32_t spo2AlarmCount = 0U;
	static uint32_t tempWarnCount = 0U;
	static uint32_t humidityWarnCount = 0U;
	static uint32_t mq2WarnCount = 0U;
	static uint32_t mq2AlarmCount = 0U;
	static uint32_t bodyTempLowWarnCount = 0U;
	static uint32_t bodyTempLowAlarmCount = 0U;
	static uint32_t bodyTempHighWarnCount = 0U;
	static uint32_t bodyTempHighAlarmCount = 0U;
	static uint32_t lastMovementTick = 0U;
	static uint8_t motionWarnLatched = 0U;
	static uint8_t motionAlarmLatched = 0U;

	uint32_t now = HAL_GetTick();
	uint32_t inactiveMs = 0U;

	memset(&s_causes, 0, sizeof(s_causes));

	snapshot->sensor_fault_active = false;
	snapshot->warning_active = false;
	snapshot->alarm_active = false;

	if (lastMovementTick == 0U) {
		lastMovementTick = now;
	}

	if (!snapshot->dht11.valid || !snapshot->mq2.valid
			|| !snapshot->mpu6050.valid) {
		snapshot->sensor_fault_active = true;
	}

	if (snapshot->max30102.valid && snapshot->max30102.finger_present) {
		if ((snapshot->max30102.heart_rate_bpm < HR_LOW_BPM)
				|| (snapshot->max30102.heart_rate_bpm > HR_HIGH_BPM)) {
			hrWarnCount++;
			hrAlarmCount++;
		} else {
			hrWarnCount = 0U;
			hrAlarmCount = 0U;
		}

		if (snapshot->max30102.spo2_pct < SPO2_WARN_PCT) {
			spo2WarnCount++;
		} else {
			spo2WarnCount = 0U;
		}
		if (snapshot->max30102.spo2_pct < SPO2_ALARM_PCT) {
			spo2AlarmCount++;
		} else {
			spo2AlarmCount = 0U;
		}
	} else {
		hrWarnCount = 0U;
		hrAlarmCount = 0U;
		spo2WarnCount = 0U;
		spo2AlarmCount = 0U;
	}

	if (snapshot->dht11.valid
			&& ((snapshot->dht11.ambient_temp_c < TEMP_LOW_C)
					|| (snapshot->dht11.ambient_temp_c > TEMP_HIGH_C))) {
		tempWarnCount++;
	} else {
		tempWarnCount = 0U;
	}

	if (snapshot->dht11.valid
			&& ((snapshot->dht11.humidity_pct < HUMIDITY_LOW_PCT)
					|| (snapshot->dht11.humidity_pct > HUMIDITY_HIGH_PCT))) {
		humidityWarnCount++;
	} else {
		humidityWarnCount = 0U;
	}

	if (snapshot->ds18b20.valid) {
		if (snapshot->ds18b20.body_temp_c <= BODY_TEMP_LOW_WARN_C) {
			bodyTempLowWarnCount++;
		} else {
			bodyTempLowWarnCount = 0U;
		}
		if (snapshot->ds18b20.body_temp_c <= BODY_TEMP_LOW_ALARM_C) {
			bodyTempLowAlarmCount++;
		} else {
			bodyTempLowAlarmCount = 0U;
		}

		if (snapshot->ds18b20.body_temp_c >= BODY_TEMP_HIGH_WARN_C) {
			bodyTempHighWarnCount++;
		} else {
			bodyTempHighWarnCount = 0U;
		}
		if (snapshot->ds18b20.body_temp_c >= BODY_TEMP_HIGH_ALARM_C) {
			bodyTempHighAlarmCount++;
		} else {
			bodyTempHighAlarmCount = 0U;
		}
	} else {
		bodyTempLowWarnCount = 0U;
		bodyTempLowAlarmCount = 0U;
		bodyTempHighWarnCount = 0U;
		bodyTempHighAlarmCount = 0U;
	}

	if (snapshot->mq2.valid) {
		if (snapshot->mq2.normalized_level >= MQ2_WARN_RATIO) {
			mq2WarnCount++;
		} else {
			mq2WarnCount = 0U;
		}
		if (snapshot->mq2.normalized_level >= MQ2_ALARM_RATIO) {
			mq2AlarmCount++;
		} else {
			mq2AlarmCount = 0U;
		}
	} else {
		mq2WarnCount = 0U;
		mq2AlarmCount = 0U;
	}

	if (snapshot->mpu6050.valid
			&& (snapshot->mpu6050.moving
					|| (snapshot->mpu6050.motion_score
							>= APP_INACTIVITY_ACTIVE_SCORE))) {
		lastMovementTick = now;
	}

	inactiveMs = now - lastMovementTick;

	s_causes.temp_warn = (tempWarnCount >= TEMP_WARN_COUNT) ? 1U : 0U;
	s_causes.humidity_warn =
			(humidityWarnCount >= HUMIDITY_WARN_COUNT) ? 1U : 0U;
	s_causes.mq2_warn = (mq2WarnCount >= MQ2_WARN_COUNT) ? 1U : 0U;
	s_causes.hr_warn = (hrWarnCount >= HR_WARN_COUNT) ? 1U : 0U;
	s_causes.spo2_warn = (spo2WarnCount >= SPO2_WARN_COUNT) ? 1U : 0U;
	s_causes.motion_warn = (inactiveMs >= APP_INACTIVITY_WARN_MS) ? 1U : 0U;

	s_causes.mq2_alarm = (mq2AlarmCount >= MQ2_ALARM_COUNT) ? 1U : 0U;
	s_causes.hr_alarm = (hrAlarmCount >= HR_ALARM_COUNT) ? 1U : 0U;
	s_causes.spo2_alarm = (spo2AlarmCount >= SPO2_ALARM_COUNT) ? 1U : 0U;
	s_causes.motion_alarm = (inactiveMs >= APP_INACTIVITY_ALARM_MS) ? 1U : 0U;

	s_causes.body_temp_low_warn =
			(bodyTempLowWarnCount >= BODY_TEMP_WARN_COUNT) ? 1U : 0U;
	s_causes.body_temp_high_warn =
			(bodyTempHighWarnCount >= BODY_TEMP_WARN_COUNT) ? 1U : 0U;
	s_causes.body_temp_low_alarm =
			(bodyTempLowAlarmCount >= BODY_TEMP_ALARM_COUNT) ? 1U : 0U;
	s_causes.body_temp_high_alarm =
			(bodyTempHighAlarmCount >= BODY_TEMP_ALARM_COUNT) ? 1U : 0U;

	if (s_causes.motion_warn && !motionWarnLatched) {
		motionWarnLatched = 1U;
		debug_log("[MOTION] inactivity warning at %lus\r\n",
				(unsigned long) (inactiveMs / 1000UL));
		event_log_push("motion warn %lus",
				(unsigned long) (inactiveMs / 1000UL));
	} else if (!s_causes.motion_warn && motionWarnLatched) {
		motionWarnLatched = 0U;
		debug_log("[MOTION] inactivity warning cleared\r\n");
		event_log_push("motion warn clear");
	}

	if (s_causes.motion_alarm && !motionAlarmLatched) {
		motionAlarmLatched = 1U;
		debug_log("[MOTION] inactivity alarm at %lus\r\n",
				(unsigned long) (inactiveMs / 1000UL));
		event_log_push("motion alarm %lus",
				(unsigned long) (inactiveMs / 1000UL));
	} else if (!s_causes.motion_alarm && motionAlarmLatched) {
		motionAlarmLatched = 0U;
		debug_log("[MOTION] inactivity alarm cleared\r\n");
		event_log_push("motion alarm clear");
	}

#if APP_DEMO_MODE_ENABLE
	switch (s_demoMode) {
	case DEMO_MODE_FORCE_WARNING:
		snapshot->warning_active = true;
		snapshot->alarm_active = false;
		snapshot->sensor_fault_active = false;
		snapshot->state = SYS_WARNING;
		return;

	case DEMO_MODE_FORCE_ALARM:
		snapshot->alarm_active = true;
		snapshot->warning_active = false;
		snapshot->sensor_fault_active = false;
		snapshot->state = SYS_ALARM_LOCAL;
		return;

	case DEMO_MODE_FORCE_DEGRADED:
		snapshot->alarm_active = false;
		snapshot->warning_active = false;
		snapshot->sensor_fault_active = true;
		snapshot->state = SYS_DEGRADED;
		return;

	case DEMO_MODE_FORCE_FAULT:
		snapshot->alarm_active = false;
		snapshot->warning_active = false;
		snapshot->sensor_fault_active = true;
		snapshot->state = SYS_FAULT;
		return;

	default:
		break;
	}
#endif

	if (s_causes.hr_alarm || s_causes.spo2_alarm || s_causes.mq2_alarm
			|| s_causes.motion_alarm || s_causes.body_temp_low_alarm
			|| s_causes.body_temp_high_alarm) {
		snapshot->alarm_active = true;
		snapshot->state = SYS_ALARM_LOCAL;
	} else if (s_causes.temp_warn || s_causes.humidity_warn || s_causes.mq2_warn
			|| s_causes.hr_warn || s_causes.spo2_warn || s_causes.motion_warn
			|| s_causes.body_temp_low_warn || s_causes.body_temp_high_warn) {
		snapshot->warning_active = true;
		snapshot->state = SYS_WARNING;
	} else if (snapshot->sensor_fault_active) {
		snapshot->state = SYS_DEGRADED;
	} else {
		snapshot->state = SYS_MONITORING;
	}
}

static const char* highest_cause_name(void) {
#if APP_DEMO_MODE_ENABLE
	if (s_demoMode != DEMO_MODE_LIVE)
		return demo_mode_name(s_demoMode);
#endif
	if (s_causes.motion_alarm)
		return "motion_alarm";
	if (s_causes.mq2_alarm)
		return "mq2_alarm";
	if (s_causes.hr_alarm)
		return "hr_alarm";
	if (s_causes.spo2_alarm)
		return "spo2_alarm";
	if (s_causes.body_temp_low_alarm)
		return "body_temp_low_alarm";
	if (s_causes.body_temp_high_alarm)
		return "body_temp_high_alarm";
	if (s_causes.body_temp_low_warn)
		return "body_temp_low_warn";
	if (s_causes.body_temp_high_warn)
		return "body_temp_high_warn";
	if (s_causes.motion_warn)
		return "motion_warn";
	if (s_causes.temp_warn)
		return "temp_warn";
	if (s_causes.humidity_warn)
		return "humidity_warn";
	if (s_causes.mq2_warn)
		return "mq2_warn";
	if (s_causes.hr_warn)
		return "hr_warn";
	if (s_causes.spo2_warn)
		return "spo2_warn";
	return "none";
}

static const char* demo_mode_name(demo_mode_t mode) {
	switch (mode) {
	case DEMO_MODE_LIVE:
		return "live";
	case DEMO_MODE_FORCE_WARNING:
		return "demo_warning";
	case DEMO_MODE_FORCE_ALARM:
		return "demo_alarm";
	case DEMO_MODE_FORCE_DEGRADED:
		return "demo_degraded";
	case DEMO_MODE_FORCE_FAULT:
		return "demo_fault";
	default:
		return "demo_unknown";
	}
}

static void log_state_causes(const system_snapshot_t *snapshot) {
	debug_log(
			"[CAUSE] %s T=%.1f H=%.1f BT=%.2f MQ=%.2f HR=%.1f SP=%.1f MV=%.3f FP=%u SIG=%u QS=%u PK=%u\r\n",
			highest_cause_name(), snapshot->dht11.ambient_temp_c,
			snapshot->dht11.humidity_pct, snapshot->ds18b20.body_temp_c,
			snapshot->mq2.normalized_level, snapshot->max30102.heart_rate_bpm,
			snapshot->max30102.spo2_pct, snapshot->mpu6050.motion_score,
			snapshot->max30102.finger_present ? 1U : 0U,
			snapshot->max30102.signal_ok ? 1U : 0U,
			snapshot->max30102.quality_score, snapshot->max30102.peak_count);
}

static void decision_warmup_with_queue_drain(uint32_t delay_ms) {
	uint32_t end_tick = osKernelGetTickCount() + delay_ms;
	sensor_msg_t msg;

	while ((int32_t) (end_tick - osKernelGetTickCount()) > 0) {
		heartbeat(APP_TASK_DECISION);

		while (osMessageQueueGet(g_sensorQueue, &msg, NULL, 0U) == osOK) {
			osMutexAcquire(g_snapshotMutex, osWaitForever);
			update_snapshot_from_msg(&msg);
			osMutexRelease(g_snapshotMutex);
		}

		osDelay(20U);
	}
}

static void DecisionTask(void *argument) {
	(void) argument;
	sensor_msg_t msg = { 0 };
	uint32_t last_summary_tick = 0U;
	system_state_t last_logged_state = SYS_BOOT;

	debug_log("[DECISION] started\r\n");

	set_system_state(SYS_SELF_TEST);
	decision_warmup_with_queue_drain(500U);

	set_system_state(SYS_WARMUP);
	decision_warmup_with_queue_drain(3000U);

	debug_log("[DECISION] entering main loop q=%lu stack=%luB\r\n",
			(unsigned long) osMessageQueueGetCount(g_sensorQueue),
			(unsigned long) osThreadGetStackSpace(osThreadGetId()));

	for (;;) {
		heartbeat(APP_TASK_DECISION);

		if (osMessageQueueGet(g_sensorQueue, &msg, NULL, 100U) == osOK) {
			osMutexAcquire(g_snapshotMutex, osWaitForever);
			update_snapshot_from_msg(&msg);
			evaluate_snapshot(&g_snapshot);

			if (g_snapshot.alarm_active) {
				(void) osEventFlagsSet(g_eventFlags,
				EVT_ALARM_ACTIVE | EVT_NEW_STATUS);
			} else if (g_snapshot.warning_active) {
				(void) osEventFlagsSet(g_eventFlags,
				EVT_WARN_ACTIVE | EVT_NEW_STATUS);
			} else {
				(void) osEventFlagsClear(g_eventFlags,
				EVT_WARN_ACTIVE | EVT_ALARM_ACTIVE);
				(void) osEventFlagsSet(g_eventFlags, EVT_NEW_STATUS);
			}

			if (g_snapshot.sensor_fault_active) {
				(void) osEventFlagsSet(g_eventFlags, EVT_SENSOR_FAULT);
			} else {
				(void) osEventFlagsClear(g_eventFlags, EVT_SENSOR_FAULT);
			}

			if (g_snapshot.state != last_logged_state) {
				debug_log("[STATE] now=%s warn=%u alarm=%u fault=%u\r\n",
						state_to_str(g_snapshot.state),
						g_snapshot.warning_active ? 1U : 0U,
						g_snapshot.alarm_active ? 1U : 0U,
						g_snapshot.sensor_fault_active ? 1U : 0U);
				log_state_causes(&g_snapshot);
				last_logged_state = g_snapshot.state;
			}

			if ((HAL_GetTick() - last_summary_tick) >= APP_LOG_SUMMARY_PERIOD_MS) {
				debug_log(
						"[SUMMARY] state=%s T=%.1f H=%.1f BT=%.2f MQ=%.2f HR=%.1f SP=%.1f MV=%.3f flags=%u%u%u%u%u demo=%s q=%lu\r\n",
						state_to_str(g_snapshot.state),
						g_snapshot.dht11.ambient_temp_c,
						g_snapshot.dht11.humidity_pct,
						g_snapshot.ds18b20.body_temp_c,
						g_snapshot.mq2.normalized_level,
						g_snapshot.max30102.heart_rate_bpm,
						g_snapshot.max30102.spo2_pct,
						g_snapshot.mpu6050.motion_score,
						g_snapshot.dht11.valid ? 1U : 0U,
						g_snapshot.ds18b20.valid ? 1U : 0U,
						g_snapshot.max30102.valid ? 1U : 0U,
						g_snapshot.mpu6050.valid ? 1U : 0U,
						g_snapshot.mq2.valid ? 1U : 0U,
						demo_mode_name(s_demoMode),
						(unsigned long) osMessageQueueGetCount(g_sensorQueue));
				last_summary_tick = HAL_GetTick();
			}

			osMutexRelease(g_snapshotMutex);
		}
	}
}

static void UiTask(void *argument) {
	(void) argument;
	uint32_t next = osKernelGetTickCount();
	uint32_t buzzer_silence_until_ms = 0U;
	uint8_t red_div = 0U;
	uint8_t green_div = 0U;
	uint8_t buzzer_div = 0U;
	system_snapshot_t snapshot;

#if APP_DEMO_MODE_ENABLE
	uint8_t button_prev = 0U;
	uint32_t button_press_ms = 0U;
	uint8_t long_press_handled = 0U;
#endif

	ui_init();
	debug_log("[UI] started\r\n");

	green_led_set(1U);
	red_led_set(1U);
	buzzer_set(1U);
	osDelay(APP_OUTPUT_SELFTEST_MS);
	green_led_set(0U);
	red_led_set(0U);
	buzzer_set(0U);

	for (;;) {
		next += UI_TASK_PERIOD_MS;

		osMutexAcquire(g_snapshotMutex, osWaitForever);
		snapshot = g_snapshot;
		osMutexRelease(g_snapshotMutex);

		ui_update(&snapshot);

		uint8_t button_now =
				(HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_13) == GPIO_PIN_RESET) ?
						1U : 0U;

#if APP_DEMO_MODE_ENABLE
		if (button_now && !button_prev) {
			button_press_ms = HAL_GetTick();
			long_press_handled = 0U;
		}

		if (button_now && !long_press_handled
				&& ((HAL_GetTick() - button_press_ms)
						>= APP_BUTTON_LONG_PRESS_MS)) {
			s_demoMode = (demo_mode_t) (((uint32_t) s_demoMode + 1U)
					% (uint32_t) DEMO_MODE_COUNT);
			debug_log("[DEMO] mode -> %s\r\n", demo_mode_name(s_demoMode));
			event_log_push("demo %s", demo_mode_name(s_demoMode));
			long_press_handled = 1U;
			osDelay(250U);
		}

		if (!button_now && button_prev) {
			uint32_t press_ms = HAL_GetTick() - button_press_ms;

			if (!long_press_handled && (press_ms < APP_BUTTON_LONG_PRESS_MS)) {
				if (snapshot.alarm_active || snapshot.warning_active) {
					buzzer_silence_until_ms = HAL_GetTick() + BUZZER_SILENCE_MS;
					ui_acknowledge();
					debug_log("[UI] buzzer silenced for %lu ms\r\n",
							(unsigned long) BUZZER_SILENCE_MS);
					event_log_push("button ack");
				}

				(void) osEventFlagsSet(g_eventFlags, EVT_BUTTON_ACK);
			}
		}

		button_prev = button_now;
#else
        if (button_now)
        {
            if (snapshot.alarm_active || snapshot.warning_active)
            {
                buzzer_silence_until_ms = HAL_GetTick() + BUZZER_SILENCE_MS;
                ui_acknowledge();
                debug_log("[UI] buzzer silenced for %lu ms\r\n", (unsigned long)BUZZER_SILENCE_MS);
                event_log_push("button ack");
            }

            (void)osEventFlagsSet(g_eventFlags, EVT_BUTTON_ACK);
            osDelay(200U);
        }
#endif

		if (!(snapshot.alarm_active || snapshot.warning_active)) {
			buzzer_silence_until_ms = 0U;
		}

		if (snapshot.alarm_active) {
			if (++red_div >= 1U) {
				red_led_toggle();
				red_div = 0U;
			}

			green_led_set(0U);

			if ((buzzer_silence_until_ms != 0U)
					&& ((int32_t) (buzzer_silence_until_ms - HAL_GetTick()) > 0)) {
				buzzer_set(0U);
			} else {
				if (++buzzer_div >= 1U) {
					buzzer_toggle();
					buzzer_div = 0U;
				}
			}

			green_div = 0U;
		} else if (snapshot.warning_active) {
			if (++red_div >= 2U) {
				red_led_toggle();
				red_div = 0U;
			}

			green_led_set(1U);
			buzzer_set(0U);
			green_div = 0U;
			buzzer_div = 0U;
		} else {
			if (++green_div >= 5U) {
				green_led_toggle();
				green_div = 0U;
			}

			red_led_set(0U);
			buzzer_set(0U);
			red_div = 0U;
			buzzer_div = 0U;
		}

		heartbeat(APP_TASK_UI);
		osDelayUntil(next);
	}
}

static void EspTask(void *argument) {
	(void) argument;
	uint32_t next = osKernelGetTickCount();
	char line[192];
	uint8_t last_link_ok = 0U;
	uint8_t consecutive_failures = 0U;

	(void) esp_uart_init();
	debug_log("[ESP] started\r\n");

	for (;;) {
		next += ESP_TASK_PERIOD_MS;

		osMutexAcquire(g_snapshotMutex, osWaitForever);
		(void) snprintf(line, sizeof(line),
				"{\"state\":%u,\"warn\":%u,\"alarm\":%u,\"temp\":%.1f,\"hum\":%.1f,\"body_temp\":%.2f,\"hr\":%.1f,\"spo2\":%.1f,\"mq2\":%.2f}\r\n",
				(unsigned) g_snapshot.state,
				g_snapshot.warning_active ? 1U : 0U,
				g_snapshot.alarm_active ? 1U : 0U,
				g_snapshot.dht11.ambient_temp_c, g_snapshot.dht11.humidity_pct,
				g_snapshot.ds18b20.body_temp_c,
				g_snapshot.max30102.heart_rate_bpm,
				g_snapshot.max30102.spo2_pct, g_snapshot.mq2.normalized_level);
		osMutexRelease(g_snapshotMutex);

		if (esp_uart_send_line(line)) {
			consecutive_failures = 0U;

			osMutexAcquire(g_snapshotMutex, osWaitForever);
			g_snapshot.wifi_link_ok = true;
			osMutexRelease(g_snapshotMutex);

			(void) osEventFlagsClear(g_eventFlags, EVT_COMMS_FAULT);

			if (last_link_ok == 0U) {
				debug_log("[ESP] UART TX recovered\r\n");
				event_log_push("ESP TX recovered");
			}

			last_link_ok = 1U;
		} else {
			consecutive_failures++;

			if (consecutive_failures >= 3U) {
				osMutexAcquire(g_snapshotMutex, osWaitForever);
				g_snapshot.wifi_link_ok = false;
				osMutexRelease(g_snapshotMutex);

				(void) osEventFlagsSet(g_eventFlags, EVT_COMMS_FAULT);

				if (last_link_ok != 0U) {
					debug_log("[ESP] UART TX failed\r\n");
					event_log_push("ESP TX failed");
				}

				last_link_ok = 0U;
			}
		}

		heartbeat(APP_TASK_ESP);
		osDelayUntil(next);
	}
}

static uint8_t check_task_deadline(app_task_id_t id, uint32_t timeout_ms,
		uint32_t now_ms) {
	uint32_t last = s_lastHeartbeatMs[id];
	if (last == 0U)
		return 0U;
	return ((now_ms - last) <= timeout_ms) ? 1U : 0U;
}

static void SupervisorTask(void *argument) {
	(void) argument;
	uint32_t next = osKernelGetTickCount();
	uint32_t startup_deadline = osKernelGetTickCount() + STARTUP_GRACE_MS;

	debug_log("[SUPERVISOR] started\r\n");

	for (;;) {
		next += SUPERVISOR_TASK_PERIOD_MS;
		uint32_t now = osKernelGetTickCount();

		if (now < startup_deadline) {
			osDelayUntil(next);
			continue;
		}

		uint8_t ok_dht = check_task_deadline(APP_TASK_DHT,
		DHT_HEARTBEAT_TIMEOUT_MS, now);
		uint8_t ok_ds18 = check_task_deadline(APP_TASK_DS18,
		DS18_HEARTBEAT_TIMEOUT_MS, now);
		uint8_t ok_max = check_task_deadline(APP_TASK_MAX,
		MAX_HEARTBEAT_TIMEOUT_MS, now);
		uint8_t ok_mpu = check_task_deadline(APP_TASK_MPU,
		MPU_HEARTBEAT_TIMEOUT_MS, now);
		uint8_t ok_mq2 = check_task_deadline(APP_TASK_MQ2,
		MQ2_HEARTBEAT_TIMEOUT_MS, now);
		uint8_t ok_dec = check_task_deadline(APP_TASK_DECISION,
		DECISION_HEARTBEAT_TIMEOUT_MS, now);
		uint8_t ok_ui = check_task_deadline(APP_TASK_UI,
		UI_HEARTBEAT_TIMEOUT_MS, now);
		uint8_t ok_esp = check_task_deadline(APP_TASK_ESP,
		ESP_HEARTBEAT_TIMEOUT_MS, now);

		if (!(ok_dht && ok_ds18 && ok_max && ok_mpu && ok_mq2 && ok_dec && ok_ui
				&& ok_esp)) {
			osMutexAcquire(g_snapshotMutex, osWaitForever);
			g_snapshot.state = SYS_FAULT;
			g_snapshot.sensor_fault_active = true;
			osMutexRelease(g_snapshotMutex);

			debug_log(
					"[SUPERVISOR] deadline miss dht=%u ds18=%u max=%u mpu=%u mq2=%u dec=%u ui=%u esp=%u\r\n",
					ok_dht, ok_ds18, ok_max, ok_mpu, ok_mq2, ok_dec, ok_ui,
					ok_esp);
			event_log_push("supervisor deadline miss");
			(void) osEventFlagsSet(g_eventFlags,
			EVT_SENSOR_FAULT | EVT_NEW_STATUS);
		}

		osDelayUntil(next);
	}
}

static const char* state_to_str(system_state_t state) {
	switch (state) {
	case SYS_BOOT:
		return "BOOT";
	case SYS_SELF_TEST:
		return "SELF_TEST";
	case SYS_WARMUP:
		return "WARMUP";
	case SYS_MONITORING:
		return "MONITORING";
	case SYS_WARNING:
		return "WARNING";
	case SYS_ALARM_LOCAL:
		return "ALARM_LOCAL";
	case SYS_ALARM_REMOTE:
		return "ALARM_REMOTE";
	case SYS_DEGRADED:
		return "DEGRADED";
	case SYS_FAULT:
		return "FAULT";
	default:
		return "UNKNOWN";
	}
}
