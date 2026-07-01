#include "dance.h"
#include "display.h"
#include "danceparam.h"

void DanceModule::setup(Services& s) {
  svc_ = &s;
  s.http->on("/dance", HTTP_POST, [this](AsyncWebServerRequest* req) {
    pending_.clip = req->hasParam("clip") ? req->getParam("clip")->value() : String("");
    long ms = req->hasParam("ms") ? req->getParam("ms")->value().toInt() : 0;
    pending_.ms = clampDanceMs(ms);
    pending_.ready = true;  // 描画・音声はここでしない。最後にreadyを立てる
    req->send(200, "application/json", "{\"ok\":true}");
  });
}

void DanceModule::loop(uint32_t now) {
  if (!pending_.ready) return;
  // priority 40。承認(100)・通知(50)が前面なら拒否され、pendingのまま次ループ再試行。
  if (!svc_->requestScreen(40, pending_.ms, now)) return;
  pending_.ready = false;
  // clip指定時のみ強制クリップ。未指定はdraw()のtickFace("done")がdoneプールから選ぶ。
  if (pending_.clip.length()) displayForceClip(pending_.clip, pending_.ms);
}

void DanceModule::draw(uint32_t now) {
  // 画面所有中のみ呼ばれる。timeout(ms)満了でclawdが背景を取り戻しidle/workingへ復帰。
  tickFace("done", "おどるのだ");
}
