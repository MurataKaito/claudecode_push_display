#pragma once
#include <Arduino.h>
#include <ESPAsyncWebServer.h>

void netCoreBegin(const char* ssid, const char* pass);  // WiFi接続 + mDNS + 基盤ルート登録
void netCoreStart();                                    // server.begin()。全モジュールsetup後に呼ぶ
AsyncWebServer& netServer();
const String& netDaemonBase();  // /heartbeat で学習したdaemonのURL。未学習なら空文字
int netBaseState();             // 0=idle / 1=working
