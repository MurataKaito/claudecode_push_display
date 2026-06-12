#include "usage.h"
#include "display.h"
#include <HTTPClient.h>
#include <ArduinoJson.h>

bool UsageModule::onGesture(Gesture g, uint32_t now) {
  if (g != GESTURE_TAP && g != GESTURE_DOUBLETAP) return false;
  if (phase_ == PHASE_USAGE) return true;  // 使用率表示中の再タップは無視（現行どおり）
  // シングルタップでも使用率（重いアニメで2連打を取りこぼすため）。
  // 承認(100)が前面ならrequestScreenが拒否され何も起きない（現行: 承認中タップはapproveが消費）。
  fetchAndShow(now);
  return true;
}

void UsageModule::fetchAndShow(uint32_t now) {
  if (svc_->daemonBase().length() == 0) {
    showFeedback("worried", "まだつながってないのだ", now);
    return;
  }
  HTTPClient http;
  http.begin(svc_->daemonBase() + "/usage");
  http.setTimeout(4000);  // この間loopはブロックする（現行と同じ）
  int code = http.GET();
  if (code == 200) {
    JsonDocument doc;
    if (deserializeJson(doc, http.getString()) == DeserializationError::Ok) {
      if (svc_->requestScreen(50, 5000, now)) {
        percent_ = doc["percent"] | 0;
        resetMin_ = doc["resetMin"] | 0;
        phase_ = PHASE_USAGE;
        drawn_ = false;
      }
    }
  } else {
    showFeedback("worried", "しゅとくしっぱいなのだ", now);
  }
  http.end();
}

void UsageModule::showFeedback(const char* expr, const char* text, uint32_t now) {
  if (!svc_->requestScreen(50, 2500, now)) return;
  fbExpr_ = expr;
  fbText_ = text;
  phase_ = PHASE_FEEDBACK;
}

void UsageModule::draw(uint32_t now) {
  if (phase_ == PHASE_USAGE) {
    if (!drawn_) {
      showUsage(percent_, resetMin_);  // 静止画面なので初回のみ描画
      drawn_ = true;
    }
  } else if (phase_ == PHASE_FEEDBACK) {
    tickFace(fbExpr_, fbText_);
  }
}
