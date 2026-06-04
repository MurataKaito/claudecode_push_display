#include "net.h"
#include "app_state.h"
#include <WiFi.h>
#include <ESPmDNS.h>
#include <ESPAsyncWebServer.h>

PendingNotify g_notify;
static AsyncWebServer server(80);

// /notify?expr=&text=  body=WAVバイナリ
static void onNotifyBody(AsyncWebServerRequest* req, uint8_t* data, size_t len,
                         size_t index, size_t total) {
  if (index == 0) {
    // 受信開始: 前のバッファを解放しPSRAMに確保
    if (g_notify.wav) { free(g_notify.wav); g_notify.wav = nullptr; }
    g_notify.wav = (uint8_t*)ps_malloc(total);
    g_notify.wavLen = 0;
  }
  if (g_notify.wav && index + len <= total) {
    memcpy(g_notify.wav + index, data, len);
    g_notify.wavLen = index + len;
  }
  if (index + len == total) {
    g_notify.expr = req->hasParam("expr") ? req->getParam("expr")->value() : String("normal");
    g_notify.text = req->hasParam("text") ? req->getParam("text")->value() : String("");
    g_notify.ready = true;  // mainが拾う
  }
}

void netBegin(const char* ssid, const char* pass) {
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, pass);
  uint32_t t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < 20000) delay(300);

  if (MDNS.begin("stackchan")) {
    MDNS.addService("http", "tcp", 80);
  }

  server.on(
    "/notify", HTTP_POST,
    [](AsyncWebServerRequest* req) { req->send(200, "application/json", "{\"ok\":true}"); },
    nullptr, onNotifyBody);
  server.begin();
}
