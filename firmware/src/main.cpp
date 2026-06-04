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

enum Mode { MODE_IDLE, MODE_USAGE, MODE_APPROVE };
static Mode mode = MODE_IDLE;
static uint32_t approveDeadline = 0;

static void requestAndShowUsage() {
  if (g_daemonBase.length() == 0) {
    showNotify("worried", "まだつながってないのだ");
    usageUntil = millis() + 2500;
    mode = MODE_USAGE;
    return;
  }
  HTTPClient http;
  http.begin(g_daemonBase + "/usage");
  http.setTimeout(4000);
  int code = http.GET();
  if (code == 200) {
    JsonDocument doc;
    if (deserializeJson(doc, http.getString()) == DeserializationError::Ok) {
      showUsage(doc["percent"] | 0, doc["resetMin"] | 0);
    } else {
      showNotify("worried", "へんじがへんなのだ");
    }
  } else {
    showNotify("worried", "しゅとくしっぱいなのだ");
  }
  http.end();
  usageUntil = millis() + 5000;
  mode = MODE_USAGE;
}

static void postApproveResult(const char* decision) {
  if (g_daemonBase.length() == 0) return;
  HTTPClient http;
  http.begin(g_daemonBase + "/approve_result/" + g_approve.id);
  http.addHeader("Content-Type", "application/json");
  http.setTimeout(4000);
  http.POST(String("{\"decision\":\"") + decision + "\"}");
  http.end();
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
  mode = MODE_IDLE;
}

void loop() {
  M5.update();
  uint32_t now = millis();

  // 承認要求の受信（最優先で画面を奪う）
  if (g_approve.ready) {
    g_approve.ready = false;
    g_approveShown++;
    showApprove(g_approve.title, g_approve.detail);
    mode = MODE_APPROVE;
    approveDeadline = now + 30000;
  }

  auto t = M5.Touch.getDetail();
  if (t.wasPressed()) gesture.down(now, t.x, t.y);
  if (t.wasReleased()) {
    g_touchReleases++;
    Gesture g = gesture.up(now, t.x, t.y);
    if (mode == MODE_APPROVE) {
      // 承認画面では寛容に：横スワイプ=拒否、それ以外のタッチ(長押し含む)=許可。
      if (g == GESTURE_SWIPE) {
        postApproveResult("deny");
        showNotify("worried", "やめておくのだ");
      } else {
        postApproveResult("allow");
        showNotify("happy", "ゴーサインなのだ！");
      }
      delay(1500);
      showIdle();
      mode = MODE_IDLE;
    } else if (g == GESTURE_DOUBLETAP && mode == MODE_IDLE) {
      requestAndShowUsage();
    }
  }

  // 通知（承認中は割り込ませない）
  if (g_notify.ready && mode != MODE_APPROVE) {
    g_notify.ready = false;
    showNotify(g_notify.expr, g_notify.text);
    if (g_notify.wav && g_notify.wavLen > 0) playWav(g_notify.wav, g_notify.wavLen);
    delay(3000);
    showIdle();
    mode = MODE_IDLE;
    usageUntil = 0;
  }

  // USAGE自動復帰
  if (mode == MODE_USAGE && usageUntil != 0 && now > usageUntil) {
    usageUntil = 0;
    showIdle();
    mode = MODE_IDLE;
  }

  // 承認タイムアウト（daemon側もtimeout→ask）
  if (mode == MODE_APPROVE && now > approveDeadline) {
    showIdle();
    mode = MODE_IDLE;
  }

  delay(10);
}
