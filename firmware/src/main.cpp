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

enum Mode { MODE_IDLE, MODE_USAGE, MODE_APPROVE };
static Mode mode = MODE_IDLE;
static uint32_t usageUntil = 0, approveDeadline = 0, notifyUntil = 0;
static String curExpr = "normal";
static String curText = "まってるのだ";

static void backToIdle() {
  mode = MODE_IDLE;
  curExpr = "normal";
  curText = "まってるのだ";
  notifyUntil = 0;
}

static void requestAndShowUsage() {
  if (g_daemonBase.length() == 0) {
    curExpr = "worried";
    curText = "まだつながってないのだ";
    notifyUntil = millis() + 2500;
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
      mode = MODE_USAGE;
      usageUntil = millis() + 5000;
    }
  } else {
    curExpr = "worried";
    curText = "しゅとくしっぱいなのだ";
    notifyUntil = millis() + 2500;
  }
  http.end();
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
  backToIdle();
}

void loop() {
  M5.update();
  uint32_t now = millis();

  // 承認要求（最優先で画面を奪う・静止表示）
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
      if (g == GESTURE_SWIPE) {
        postApproveResult("deny");
        curExpr = "worried";
        curText = "やめておくのだ";
      } else {
        postApproveResult("allow");
        curExpr = "happy";
        curText = "ゴーサインなのだ！";
      }
      mode = MODE_IDLE;
      notifyUntil = now + 1800;
    } else if (g == GESTURE_DOUBLETAP && mode == MODE_IDLE) {
      requestAndShowUsage();
    }
  }

  // 通知（承認中は割り込ませない）
  if (g_notify.ready && mode != MODE_APPROVE) {
    g_notify.ready = false;
    curExpr = g_notify.expr;
    curText = g_notify.text.length() ? g_notify.text : "おしらせなのだ";
    if (g_notify.wav && g_notify.wavLen > 0) playWav(g_notify.wav, g_notify.wavLen);
    mode = MODE_IDLE;
    notifyUntil = now + 4000;
  }

  // 通知/フィードバック表示の終了 → 通常へ
  if (notifyUntil != 0 && now > notifyUntil) backToIdle();
  if (mode == MODE_USAGE && usageUntil != 0 && now > usageUntil) {
    usageUntil = 0;
    backToIdle();
  }
  if (mode == MODE_APPROVE && now > approveDeadline) backToIdle();

  // アイドル/通知中は生き生きアニメ（USAGE/APPROVEは静止）
  if (mode == MODE_IDLE) tickFace(curExpr, curText);

  delay(10);
}
