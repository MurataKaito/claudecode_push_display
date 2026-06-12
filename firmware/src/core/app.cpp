#include "core/app.h"
#include "core/module.h"
#include "core/net_core.h"
#include "module_registry.h"
#include "display.h"
#include "audio.h"
#include "secrets.h"
#include "gesture.h"
#include "screen_arbiter.h"
#include <M5Unified.h>

static GestureDetector gestureDetector;
static ScreenArbiter arbiter;
static Services* services = nullptr;
static volatile int touchReleases = 0;
static volatile int lastGesture = -1;

int appTouchReleases() { return touchReleases; }
int appLastGesture() { return lastGesture; }

bool Services::requestScreen(int priority, uint32_t timeoutMs, uint32_t now) {
  int preempted = -1;
  if (!arbiter.request(selfId, priority, timeoutMs, now, &preempted)) return false;
  if (preempted >= 0) MODULES[preempted]->onScreenLost();
  return true;
}

void Services::releaseScreen() { arbiter.release(selfId); }
const String& Services::daemonBase() const { return netDaemonBase(); }
int Services::baseState() const { return netBaseState(); }

// 前面モジュール → レジストリ順。最初にtrueを返したところで止める。
static void dispatchGesture(Gesture g, uint32_t now) {
  int own = arbiter.owner();
  if (own >= 0 && MODULES[own]->onGesture(g, now)) return;
  for (size_t i = 0; i < MODULE_COUNT; i++) {
    if ((int)i == own) continue;
    if (MODULES[i]->onGesture(g, now)) return;
  }
}

void appSetup() {
  auto cfg = M5.config();
  M5.begin(cfg);
  displayBegin();
  audioBegin();

  M5.Display.fillScreen(TFT_BLACK);
  M5.Display.setCursor(0, 0);
  M5.Display.print("WiFi...");
  netCoreBegin(WIFI_SSID, WIFI_PASS);

  services = new Services[MODULE_COUNT > 0 ? MODULE_COUNT : 1];
  for (size_t i = 0; i < MODULE_COUNT; i++) {
    services[i].http = &netServer();
    services[i].selfId = (int)i;
    MODULES[i]->setup(services[i]);
  }
  netCoreStart();  // 全モジュールのルート登録が済んでからlisten開始
}

void appLoop() {
  M5.update();
  uint32_t now = millis();

  auto t = M5.Touch.getDetail();
  if (t.wasPressed()) gestureDetector.down(now, t.x, t.y);
  if (t.wasReleased()) {
    touchReleases = touchReleases + 1;
    Gesture g = gestureDetector.up(now, t.x, t.y);
    lastGesture = (int)g;
    dispatchGesture(g, now);  // GESTURE_NONEも配る（承認の「スワイプ以外は何でもallow」のため）
  }

  // 期限切れの画面を解放
  int expired = arbiter.tick(now);
  if (expired >= 0) MODULES[expired]->onScreenLost();

  for (size_t i = 0; i < MODULE_COUNT; i++) MODULES[i]->loop(now);

  // 描画: ownerがいればそのdraw()、いなければベース顔（baseState連動）
  int own = arbiter.owner();
  if (own >= 0) {
    MODULES[own]->draw(now);
  } else {
    bool working = netBaseState() == 1;
    tickFace(working ? "working" : "normal",
             working ? "おしごとちゅうなのだ" : "まってるのだ");
  }
  delay(10);
}
