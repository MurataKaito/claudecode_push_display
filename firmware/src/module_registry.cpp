#include "module_registry.h"
#include "modules/approve/approve.h"
#include "modules/notify/notify.h"

static ApproveModule approveModule;
static NotifyModule notifyModule;

Module* const MODULES[] = {
    &approveModule,  // フォールバック先頭（承認表示中は全ジェスチャを消費する）
    &notifyModule,
};
const size_t MODULE_COUNT = sizeof(MODULES) / sizeof(MODULES[0]);
