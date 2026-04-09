#define BLYNK_TEMPLATE_ID "TMPL22CniNIDH"
#define BLYNK_TEMPLATE_NAME "Baby monitor"
#define BLYNK_AUTH_TOKEN "intyooB_aZHJfDYFM_f1pa3ThnlZRCic"

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <BlynkSimpleEsp32.h>

/*
  Programming target:
  - Arduino IDE or PlatformIO using the Arduino core for ESP32.

  STM32 -> ESP32 UART wiring expected:
  - STM32 PA9  (USART1_TX) -> ESP32 RX pin below
  - STM32 PA10 (USART1_RX) <- ESP32 TX pin below
  - GND common

  Default pins below match the common ESP32 DevKit usage of UART2 on GPIO16/GPIO17.
*/

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

#define VPIN_TEMP     V0
#define VPIN_HUM      V1
#define VPIN_HR       V2
#define VPIN_SPO2     V3
#define VPIN_MQ2      V4
#define VPIN_STATE    V5
#define VPIN_WARN     V6
#define VPIN_ALARM    V7

struct MonitorPacket {
  int state = 0;
  int warn = 0;
  int alarm = 0;
  float temp = 0.0f;
  float hum = 0.0f;
  float hr = 0.0f;
  float spo2 = 0.0f;
  float mq2 = 0.0f;
  bool hasState = false;
  bool hasWarn = false;
  bool hasAlarm = false;
};

HardwareSerial StmUart(2);

static String lineBuffer;
static unsigned long lastWifiAttemptMs = 0;
static unsigned long lastStatusPrintMs = 0;
static unsigned long lastAlertSentMs = 0;
static unsigned long lastAlarmSeenMs = 0;
static bool alarmLatched = false;
static bool warningLatched = false;

static bool extractInt(const String &json, const char *key, int &out);
static bool extractFloat(const String &json, const char *key, float &out);
static bool parsePacket(const String &json, MonitorPacket &pkt);
static void connectWifiIfNeeded();
static void processUart();
static void handlePacket(const MonitorPacket &pkt, const String &raw);
static void pushToBlynk(const MonitorPacket &pkt);
static bool sendWebhookAlert(const String &title, const String &body, bool highPriority);
static String buildSummary(const MonitorPacket &pkt);
static void printPacket(const MonitorPacket &pkt, const String &raw);

void setup() {
  Serial.begin(DEBUG_BAUD);
  delay(200);

  Serial.println();
  Serial.println("[ESP32] infant monitor notifier starting");

  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.persistent(false);

  StmUart.begin(STM_UART_BAUD, SERIAL_8N1, STM_UART_RX_PIN, STM_UART_TX_PIN);
  Serial.printf("[ESP32] UART2 started @ %lu baud RX=%d TX=%d\n",
                (unsigned long)STM_UART_BAUD, STM_UART_RX_PIN, STM_UART_TX_PIN);

  connectWifiIfNeeded();

  if (WiFi.status() == WL_CONNECTED) {
    Blynk.config(BLYNK_AUTH_TOKEN);
    Blynk.connect(5000);
    Serial.println("[ESP32] Blynk configured");
  }
}

void loop() {
  if (WiFi.status() == WL_CONNECTED) {
    Blynk.run();
  }

  connectWifiIfNeeded();
  processUart();

  unsigned long now = millis();
  if ((now - lastStatusPrintMs) >= 5000UL) {
    lastStatusPrintMs = now;
    Serial.printf("[ESP32] WiFi=%s IP=%s Blynk=%s alarmLatched=%d warningLatched=%d\n",
                  (WiFi.status() == WL_CONNECTED) ? "connected" : "down",
                  (WiFi.status() == WL_CONNECTED) ? WiFi.localIP().toString().c_str() : "0.0.0.0",
                  Blynk.connected() ? "connected" : "down",
                  alarmLatched ? 1 : 0,
                  warningLatched ? 1 : 0);
  }
}

static void connectWifiIfNeeded() {
  unsigned long now = millis();

  if (WiFi.status() == WL_CONNECTED) {
    return;
  }

  if ((now - lastWifiAttemptMs) < WIFI_RETRY_MS) {
    return;
  }
  lastWifiAttemptMs = now;

  Serial.printf("[ESP32] connecting to WiFi SSID '%s'\n", WIFI_SSID);
  WiFi.disconnect(true, true);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  unsigned long start = millis();
  while ((WiFi.status() != WL_CONNECTED) && ((millis() - start) < WIFI_CONNECT_TIMEOUT_MS)) {
    delay(250);
    Serial.print(".");
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("[ESP32] WiFi connected, IP=%s RSSI=%d dBm\n",
                  WiFi.localIP().toString().c_str(), WiFi.RSSI());
    Blynk.config(BLYNK_AUTH_TOKEN);
    Blynk.connect(5000);
  } else {
    Serial.println("[ESP32] WiFi connect timeout");
  }
}

static void processUart() {
  while (StmUart.available() > 0) {
    char c = (char)StmUart.read();

    if (c == '\r') {
      continue;
    }

    if (c == '\n') {
      if (lineBuffer.length() > 0) {
        MonitorPacket pkt;
        String raw = lineBuffer;
        lineBuffer = "";

        if (parsePacket(raw, pkt)) {
          printPacket(pkt, raw);
          handlePacket(pkt, raw);
        } else {
          Serial.printf("[ESP32] ignored line: %s\n", raw.c_str());
        }
      }
      continue;
    }

    if (lineBuffer.length() < 512) {
      lineBuffer += c;
    } else {
      lineBuffer = "";
      Serial.println("[ESP32] dropped overlong UART line");
    }
  }
}

static void pushToBlynk(const MonitorPacket &pkt) {
  if (!Blynk.connected()) {
    Serial.println("[ESP32] Blynk not connected, skipping push");
    return;
  }
  Blynk.virtualWrite(VPIN_TEMP,  pkt.temp);
  Blynk.virtualWrite(VPIN_HUM,   pkt.hum);
  Blynk.virtualWrite(VPIN_HR,    pkt.hr);
  Blynk.virtualWrite(VPIN_SPO2,  pkt.spo2);
  Blynk.virtualWrite(VPIN_MQ2,   pkt.mq2);
  Blynk.virtualWrite(VPIN_STATE, pkt.state);
  Blynk.virtualWrite(VPIN_WARN,  pkt.warn);
  Blynk.virtualWrite(VPIN_ALARM, pkt.alarm);
  Serial.println("[ESP32] pushed to Blynk");
}

static void handlePacket(const MonitorPacket &pkt, const String &raw) {
  pushToBlynk(pkt);

  unsigned long now = millis();
  bool alarmActive = pkt.hasAlarm && (pkt.alarm != 0);
  bool warningActive = pkt.hasWarn && (pkt.warn != 0);

  if (alarmActive) {
    lastAlarmSeenMs = now;
  }

  if (alarmActive && !alarmLatched) {
    String title = "Infant monitor ALARM";
    String body = buildSummary(pkt);
    if (sendWebhookAlert(title, body, true)) {
      alarmLatched = true;
      warningLatched = warningActive;
      lastAlertSentMs = now;
    }
    return;
  }

  if (!alarmActive && alarmLatched) {
    String title = "Infant monitor alarm cleared";
    String body = buildSummary(pkt);
    (void)sendWebhookAlert(title, body, false);
    alarmLatched = false;
    warningLatched = warningActive;
    lastAlertSentMs = now;
    return;
  }

  if (!alarmLatched && warningActive && !warningLatched && SEND_WARNING_ALERTS) {
    String title = "Infant monitor warning";
    String body = buildSummary(pkt);
    if (sendWebhookAlert(title, body, false)) {
      warningLatched = true;
      lastAlertSentMs = now;
    }
    return;
  }

  if (!alarmLatched && !warningActive && warningLatched) {
    warningLatched = false;
  }

  if (alarmActive && ((now - lastAlertSentMs) >= ALARM_REMINDER_MS)) {
    String title = "Infant monitor ALARM reminder";
    String body = buildSummary(pkt);
    if (sendWebhookAlert(title, body, true)) {
      lastAlertSentMs = now;
    }
  }
}

static bool sendWebhookAlert(const String &title, const String &body, bool highPriority) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[ESP32] cannot send alert, WiFi not connected");
    return false;
  }

  Serial.printf("[ESP32] sending alert: %s\n", title.c_str());

  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient http;
  bool ok = false;

  if (!http.begin(client, ALERT_WEBHOOK_URL)) {
    Serial.println("[ESP32] HTTP begin failed");
    return false;
  }

  http.setConnectTimeout(HTTP_CONNECT_TIMEOUT_MS);
  http.setTimeout(HTTP_TIMEOUT_MS);

  int code = -1;

#if ALERT_MODE_NTFY
  http.addHeader("Title", title);
  http.addHeader("Priority", highPriority ? "urgent" : "default");
  http.addHeader("Tags", highPriority ? "warning,rotating_light,baby" : "warning,baby");
  http.addHeader("Content-Type", "text/plain");
  code = http.POST(body);
#else
  http.addHeader("Content-Type", "application/json");
  String payload = "{\"title\":\"" + title + "\",\"body\":\"" + body + "\"}";
  code = http.POST(payload);
#endif

  if (code > 0) {
    String resp = http.getString();
    Serial.printf("[ESP32] alert sent, HTTP %d, response=%s\n", code, resp.c_str());
    ok = (code >= 200 && code < 300);
  } else {
    Serial.printf("[ESP32] alert failed, HTTPClient code=%d error=%s\n",
                  code, http.errorToString(code).c_str());
  }

  http.end();
  return ok;
}

static String buildSummary(const MonitorPacket &pkt) {
  String s;
  s.reserve(200);
  s += "state=" + String(pkt.state);
  s += ", warn=" + String(pkt.warn);
  s += ", alarm=" + String(pkt.alarm);
  s += ", temp=" + String(pkt.temp, 1) + "C";
  s += ", hum=" + String(pkt.hum, 1) + "%";
  s += ", hr=" + String(pkt.hr, 1) + " bpm";
  s += ", spo2=" + String(pkt.spo2, 1) + "%";
  s += ", mq2=" + String(pkt.mq2, 2);
  return s;
}

static void printPacket(const MonitorPacket &pkt, const String &raw) {
  Serial.printf("[ESP32] RX state=%d warn=%d alarm=%d temp=%.1f hum=%.1f hr=%.1f spo2=%.1f mq2=%.2f\n",
                pkt.state, pkt.warn, pkt.alarm, pkt.temp, pkt.hum, pkt.hr, pkt.spo2, pkt.mq2);
  if (VERBOSE_RAW_JSON) {
    Serial.printf("[ESP32] RAW %s\n", raw.c_str());
  }
}

static bool parsePacket(const String &json, MonitorPacket &pkt) {
  bool any = false;
  any |= extractInt(json, "\"state\"", pkt.state);
  any |= extractInt(json, "\"warn\"", pkt.warn);
  any |= extractInt(json, "\"alarm\"", pkt.alarm);
  any |= extractFloat(json, "\"temp\"", pkt.temp);
  any |= extractFloat(json, "\"hum\"", pkt.hum);
  any |= extractFloat(json, "\"hr\"", pkt.hr);
  any |= extractFloat(json, "\"spo2\"", pkt.spo2);
  any |= extractFloat(json, "\"mq2\"", pkt.mq2);

  pkt.hasState = json.indexOf("\"state\"") >= 0;
  pkt.hasWarn = json.indexOf("\"warn\"") >= 0;
  pkt.hasAlarm = json.indexOf("\"alarm\"") >= 0;

  return any;
}

static bool extractInt(const String &json, const char *key, int &out) {
  int idx = json.indexOf(key);
  if (idx < 0) return false;

  idx = json.indexOf(':', idx);
  if (idx < 0) return false;
  idx++;

  while (idx < (int)json.length() && (json[idx] == ' ' || json[idx] == '"')) idx++;

  int end = idx;
  while (end < (int)json.length() &&
         (json[end] == '-' || (json[end] >= '0' && json[end] <= '9'))) {
    end++;
  }

  if (end <= idx) return false;
  out = json.substring(idx, end).toInt();
  return true;
}

static bool extractFloat(const String &json, const char *key, float &out) {
  int idx = json.indexOf(key);
  if (idx < 0) return false;

  idx = json.indexOf(':', idx);
  if (idx < 0) return false;
  idx++;

  while (idx < (int)json.length() && (json[idx] == ' ' || json[idx] == '"')) idx++;

  int end = idx;
  while (end < (int)json.length() &&
         (json[end] == '-' || json[end] == '.' || (json[end] >= '0' && json[end] <= '9'))) {
    end++;
  }

  if (end <= idx) return false;
  out = json.substring(idx, end).toFloat();
  return true;
}