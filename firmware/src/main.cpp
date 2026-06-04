#include <M5Unified.h>
#include "secrets.h"
#include "display.h"
#include "audio.h"
#include "net.h"
#include "app_state.h"

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

  if (g_notify.ready) {
    g_notify.ready = false;
    showNotify(g_notify.expr, g_notify.text);
    if (g_notify.wav && g_notify.wavLen > 0) {
      playWav(g_notify.wav, g_notify.wavLen);   // 再生
    }
    // 簡易: 3秒後にIDLEへ
    delay(3000);
    showIdle();
  }

  delay(10);
}
