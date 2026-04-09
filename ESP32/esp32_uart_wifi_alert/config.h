#ifndef CONFIG_H
#define CONFIG_H

#define WIFI_SSID       "BrotherLaserPrinter"
#define WIFI_PASSWORD   "1333RoCk5!"

#define ALERT_WEBHOOK_URL "https://ntfy.sh/infant_alarm"
#define ALERT_MODE_NTFY   1

#define SEND_WARNING_ALERTS     1
#define ALARM_REMINDER_MS       60000UL

#define STM_UART_BAUD           115200UL
#define STM_UART_RX_PIN         16
#define STM_UART_TX_PIN         17

#define DEBUG_BAUD              115200UL
#define WIFI_RETRY_MS           5000UL
#define WIFI_CONNECT_TIMEOUT_MS 15000UL
#define HTTP_CONNECT_TIMEOUT_MS 8000
#define HTTP_TIMEOUT_MS         12000

#define VERBOSE_RAW_JSON        1

#endif
