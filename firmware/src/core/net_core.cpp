#include "core/net_core.h"
#include "core/app.h"
#include "module_registry.h"
#include <WiFi.h>
#include <ESPmDNS.h>

static AsyncWebServer server(80);
static String daemonBase;
static volatile int baseState = 0;  // 0=idle / 1=working

AsyncWebServer& netServer() { return server; }
const String& netDaemonBase() { return daemonBase; }
int netBaseState() { return baseState; }

void netCoreBegin(const char* ssid, const char* pass) {
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, pass);
  uint32_t t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < 20000) delay(300);

  if (MDNS.begin("stackchan")) {
    MDNS.addService("http", "tcp", 80);
  }

  server.on("/heartbeat", HTTP_POST, [](AsyncWebServerRequest* req) {
    String ip = req->client()->remoteIP().toString();
    int port = req->hasParam("port") ? req->getParam("port")->value().toInt() : 4920;
    daemonBase = "http://" + ip + ":" + String(port);
    Serial.printf("[HB] daemon=%s\n", daemonBase.c_str());
    req->send(200, "application/json", "{\"ok\":true}");
  });

  server.on("/base", HTTP_POST, [](AsyncWebServerRequest* req) {
    String s = req->hasParam("s") ? req->getParam("s")->value() : String("idle");
    baseState = (s == "working") ? 1 : 0;
    req->send(200, "application/json", "{\"ok\":true}");
  });

  // core分のフィールド + 各モジュールのappendState()を連結（既存キーは名前・型とも維持）
  // ※ /clip と dbg は clawd モジュールへ移設済み
  server.on("/state", HTTP_GET, [](AsyncWebServerRequest* req) {
    String j = "{\"daemonBase\":\"" + daemonBase + "\",\"touch\":" + String(appTouchReleases()) +
               ",\"g\":" + String(appLastGesture());
    for (size_t i = 0; i < MODULE_COUNT; i++) MODULES[i]->appendState(j);
    j += "}";  // dbg は clawd の appendState() が追記する
    req->send(200, "application/json", j);
  });
}

void netCoreStart() { server.begin(); }
