#include "net.h"
#include "app_state.h"
#include "display.h"
#include <WiFi.h>
#include <ESPmDNS.h>
#include <ESPAsyncWebServer.h>

PendingNotify g_notify;
String g_daemonBase;
PendingApprove g_approve;
volatile int g_approveRecv = 0;
volatile int g_approveShown = 0;
volatile int g_touchReleases = 0;
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

  server.on("/heartbeat", HTTP_POST, [](AsyncWebServerRequest* req) {
    String ip = req->client()->remoteIP().toString();
    int port = req->hasParam("port") ? req->getParam("port")->value().toInt() : 4920;
    g_daemonBase = "http://" + ip + ":" + String(port);
    Serial.printf("[HB] daemon=%s\n", g_daemonBase.c_str());
    req->send(200, "application/json", "{\"ok\":true}");
  });

  server.on("/approve", HTTP_POST, [](AsyncWebServerRequest* req) {
    g_approve.id = req->hasParam("id") ? req->getParam("id")->value() : String("");
    g_approve.title = req->hasParam("title") ? req->getParam("title")->value() : String("CLAUDE OK?");
    g_approve.detail = req->hasParam("detail") ? req->getParam("detail")->value() : String("");
    g_approve.ready = true;
    g_approveRecv++;
    req->send(200, "application/json", "{\"ok\":true}");
  });

  server.on("/state", HTTP_GET, [](AsyncWebServerRequest* req) {
    String j = "{\"daemonBase\":\"" + g_daemonBase + "\",\"recv\":" + String(g_approveRecv) +
               ",\"shown\":" + String(g_approveShown) + ",\"touch\":" + String(g_touchReleases) +
               ",\"ready\":" + String(g_approve.ready ? 1 : 0) + ",\"lastId\":\"" + g_approve.id +
               "\",\"dbg\":\"" + displayDebug() + "\"}";
    req->send(200, "application/json", j);
  });

  server.begin();
}
