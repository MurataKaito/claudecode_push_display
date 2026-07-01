#include "clawd.h"
#include "display.h"

void ClawdModule::setup(Services& s) {
  svc_ = &s;
  // /clip: 指定prefixのクリップをms間強制表示（デバッグ用）。net_coreから移設。
  s.http->on("/clip", HTTP_POST, [](AsyncWebServerRequest* req) {
    String p = req->hasParam("p") ? req->getParam("p")->value() : String("look");
    uint32_t ms = req->hasParam("ms") ? (uint32_t)req->getParam("ms")->value().toInt() : 4000;
    displayForceClip(p, ms);
    req->send(200, "application/json", "{\"ok\":true}");
  });
}

void ClawdModule::loop(uint32_t now) {
  // 優先度0で背景に常駐。自分がownerなら期限更新、空きなら取得、上位ownerがいればfalse（何もしない）。
  svc_->requestScreen(0, 1000, now);
}

void ClawdModule::draw(uint32_t now) {
  bool working = svc_->baseState() == 1;
  tickFace(working ? "working" : "normal",
           working ? "おしごとちゅうなのだ" : "まってるのだ");
}

void ClawdModule::appendState(String& json) {
  json += ",\"dbg\":\"" + displayDebug() + "\"";
}
