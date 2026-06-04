#include <M5Unified.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "secrets.h"
#include "display.h"
#include "audio.h"
#include "net.h"
#include "app_state.h"
#include "gesture.h"

static GestureDetector gesture;
static uint32_t usageUntil = 0;  // この時刻(ms)までUSAGE表示

static void requestAndShowUsage() {
  if (g_daemonBase.length() == 0) {
    showNotify("worried", "まだつながってないのだ");
    usageUntil = millis() + 2500;
    return;
  }
  HTTPClient http;
  http.begin(g_daemonBase + "/usage");
  http.setTimeout(4000);
  int code = http.GET();
  if (code == 200) {
    String body = http.getString();
    JsonDocument doc;
    if (deserializeJson(doc, body) == DeserializationError::Ok) {
      int percent = doc["percent"] | 0;
      int resetMin = doc["resetMin"] | 0;
      showUsage(percent, resetMin);
    } else {
      showNotify("worried", "へんじがへんなのだ");
    }
  } else {
    showNotify("worried", "しゅとくしっぱいなのだ");
  }
  http.end();
  usageUntil = millis() + 5000;
}

void setup() {
  auto cfg = M5.config();
  M5.begin(cfg);
  displayBegin();
  audioBegin();

  M5.Display.fillScreen(TFT_BLACK);
  M5.Display.setCursor(0, 0);
  M5.Display.print("WiFi...");
  netBegin(WIFI_SSID, WIFI_PASS);
  showIdle();
}

void loop() {
  M5.update();
  uint32_t now = millis();

  auto t = M5.Touch.getDetail();
  if (t.wasPressed()) gesture.down(now, t.x, t.y);
  if (t.wasReleased()) {
    Gesture g = gesture.up(now, t.x, t.y);
    if (g == GESTURE_DOUBLETAP && usageUntil == 0) {
      requestAndShowUsage();
    }
  }

  if (g_notify.ready) {
    g_notify.ready = false;
    showNotify(g_notify.expr, g_notify.text);
    if (g_notify.wav && g_notify.wavLen > 0) {
      playWav(g_notify.wav, g_notify.wavLen);
    }
    delay(3000);
    showIdle();
    usageUntil = 0;
  }

  if (usageUntil != 0 && now > usageUntil) {
    usageUntil = 0;
    showIdle();
  }

  delay(10);
}
