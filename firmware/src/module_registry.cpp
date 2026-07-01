#include "module_registry.h"
#include "modules/approve/approve.h"
#include "modules/notify/notify.h"
#include "modules/usage/usage.h"
#include "modules/clawd/clawd.h"

static ApproveModule approveModule;
static NotifyModule notifyModule;
static UsageModule usageModule;
static ClawdModule clawdModule;

Module* const MODULES[] = {
    &approveModule,  // フォールバック先頭（承認表示中は全ジェスチャを消費する）
    &notifyModule,
    &usageModule,    // タップ/ダブルタップを拾う（通知オーバーレイ中でも有効）
    &clawdModule,    // 背景(0) 常駐オーナー。onGestureは消費しない
};
const size_t MODULE_COUNT = sizeof(MODULES) / sizeof(MODULES[0]);
