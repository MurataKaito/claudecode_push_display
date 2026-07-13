#include "notify.h"
#include "audio.h"
#include "display.h"

void NotifyModule::setup(Services& s) {
  svc_ = &s;
  s.http->on(
      "/notify", HTTP_POST,
      [](AsyncWebServerRequest* req) { req->send(200, "application/json", "{\"ok\":true}"); },
      nullptr,
      [this](AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total) {
        onBody(req, data, len, index, total);
      });
}

// /notify?expr=&text=  body=WAVバイナリ
void NotifyModule::onBody(AsyncWebServerRequest* req, uint8_t* data, size_t len,
                          size_t index, size_t total) {
  if (index == 0) {
    // 受信開始: 前のバッファを解放しPSRAMに確保
    if (pending_.wav) { free(pending_.wav); pending_.wav = nullptr; }
    pending_.wav = (uint8_t*)ps_malloc(total);
    pending_.wavLen = 0;
  }
  if (pending_.wav && index + len <= total) {
    memcpy(pending_.wav + index, data, len);
    pending_.wavLen = index + len;
  }
  if (index + len == total) {
    pending_.expr = req->hasParam("expr") ? req->getParam("expr")->value() : String("normal");
    pending_.text = req->hasParam("text") ? req->getParam("text")->value() : String("");
    pending_.ready = true;  // loop()が拾う
  }
}

void NotifyModule::loop(uint32_t now) {
  if (!pending_.ready) return;
  // 承認(100)が前面の間はgrantされず、pendingのまま次ループ再試行（現行挙動: 承認中は割り込まない）
  if (!svc_->requestScreen(50, 4000, now)) return;
  pending_.ready = false;
  expr_ = pending_.expr;
  text_ = pending_.text.length() ? pending_.text : "おしらせなのだ";
  if (pending_.wav && pending_.wavLen > 0) playWav(pending_.wav, pending_.wavLen);
}

void NotifyModule::draw(uint32_t now) { tickFace(expr_, text_); }
