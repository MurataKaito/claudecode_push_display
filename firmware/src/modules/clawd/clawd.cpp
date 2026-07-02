#include "clawd.h"
#include "display.h"

static const uint32_t kRenewMs = 1000;  // 背景オーナーの期限更新間隔。毎ループ更新されるので実質切れない

void ClawdModule::setup(Services& s) {
  svc_ = &s;
  // /clip: 指定prefixのクリップをms間強制表示（デバッグ用）。net_coreから移設。
  // asyncハンドラでは描画状態を触らず、pendingに積んで loop() で displayForceClip する。
  s.http->on("/clip", HTTP_POST, [this](AsyncWebServerRequest* req) {
    pendingClip_.prefix = req->hasParam("p") ? req->getParam("p")->value() : String("look");
    long raw = req->hasParam("ms") ? req->getParam("ms")->value().toInt() : 4000;
    // 負値/巨大値でg_forcedUntilが壊れるのを防ぐ（未指定/不正は既定4000、上限60000）。
    pendingClip_.ms = raw <= 0 ? 4000 : (raw > 60000 ? 60000 : (uint32_t)raw);
    pendingClip_.ready = true;  // 最後に立てる
    req->send(200, "application/json", "{\"ok\":true}");
  });
}

void ClawdModule::loop(uint32_t now) {
  // /clip の適用（メインタスクで実行し、asyncタスクとdisplayグローバルの競合を避ける）
  if (pendingClip_.ready) {
    pendingClip_.ready = false;
    displayForceClip(pendingClip_.prefix, pendingClip_.ms);
  }
  // 優先度0で背景に常駐。自分がownerなら期限更新、空きなら取得、上位ownerがいればfalse（何もしない）。
  svc_->requestScreen(0, kRenewMs, now);
}

void ClawdModule::draw(uint32_t now) {
  bool working = svc_->baseState() == 1;
  tickFace(working ? "working" : "normal",
           working ? "おしごとちゅうなのだ" : "まってるのだ");
}

void ClawdModule::appendState(String& json) {
  json += ",\"dbg\":\"" + displayDebug() + "\"";
}
