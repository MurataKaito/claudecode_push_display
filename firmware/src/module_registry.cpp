#include "module_registry.h"
#include "modules/approve/approve.h"
#include "modules/dance/dance.h"
#include "modules/notify/notify.h"
#include "modules/usage/usage.h"
#include "modules/clawd/clawd.h"

static ApproveModule approveModule;
static DanceModule danceModule;
static NotifyModule notifyModule;
static UsageModule usageModule;
static ClawdModule clawdModule;

Module* const MODULES[] = {
    &approveModule,  // 100（割り込み禁止）
    &danceModule,    // 40（演出）
    &notifyModule,   // 50
    &usageModule,    // タップ/ダブルタップ
    &clawdModule,    // 0（背景常駐オーナー）
};
const size_t MODULE_COUNT = sizeof(MODULES) / sizeof(MODULES[0]);
