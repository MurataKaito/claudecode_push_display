#include "approve.h"
#include "display.h"
#include <HTTPClient.h>

void ApproveModule::setup(Services& s) {
  svc_ = &s;
  s.http->on("/approve", HTTP_POST, [this](AsyncWebServerRequest* req) {
    pending_.id = req->hasParam("id") ? req->getParam("id")->value() : String("");
    pending_.title = req->hasParam("title") ? req->getParam("title")->value() : String("CLAUDE OK?");
    pending_.detail = req->hasParam("detail") ? req->getParam("detail")->value() : String("");
    pending_.ready = true;
    recv_ = recv_ + 1;
    req->send(200, "application/json", "{\"ok\":true}");
  });
}

void ApproveModule::loop(uint32_t now) {
  if (!pending_.ready) return;
  pending_.ready = false;
  // priority 100は常にgrant（承認中に新しい承認が来たら後勝ちで置き換え=現行挙動）
  if (svc_->requestScreen(100, 30000, now)) {
    id_ = pending_.id;
    title_ = pending_.title;
    detail_ = pending_.detail;
    shown_++;
    phase_ = PHASE_SHOWING;
    drawn_ = false;
  }
}

bool ApproveModule::onGesture(Gesture g, uint32_t now) {
  if (phase_ != PHASE_SHOWING) return false;
  // スワイプ=deny、それ以外のタッチ（タップ/長押し等）=allow（現行挙動）
  if (g == GESTURE_SWIPE) {
    postResult("deny");
    fbExpr_ = "worried";
    fbText_ = "やめておくのだ";
  } else {
    postResult("allow");
    fbExpr_ = "wink";
    fbText_ = "オッケーなのだ";
  }
  phase_ = PHASE_FEEDBACK;
  // フィードバック顔は通知と同格(50)に自己降格。通知やタップで上書きされてよい（現行挙動）
  svc_->requestScreen(50, 1800, now);
  return true;
}

void ApproveModule::draw(uint32_t now) {
  if (phase_ == PHASE_SHOWING) {
    if (!drawn_) {
      showApprove(title_, detail_);  // 静止画面なので初回のみ描画
      drawn_ = true;
    }
  } else if (phase_ == PHASE_FEEDBACK) {
    tickFace(fbExpr_, fbText_);
  }
}

void ApproveModule::postResult(const char* decision) {
  if (svc_->daemonBase().length() == 0) return;
  HTTPClient http;
  http.begin(svc_->daemonBase() + "/approve_result/" + id_);
  http.addHeader("Content-Type", "application/json");
  http.setTimeout(4000);
  http.POST(String("{\"decision\":\"") + decision + "\"}");
  http.end();
}

// /state の既存キー recv/shown/ready/lastId はapproveの所有（現行のg_approve系の移管）
void ApproveModule::appendState(String& json) {
  json += ",\"recv\":" + String(recv_) + ",\"shown\":" + String(shown_) +
          ",\"ready\":" + String(pending_.ready ? 1 : 0) + ",\"lastId\":\"" + pending_.id + "\"";
}
