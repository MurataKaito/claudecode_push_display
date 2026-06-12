#include "module_registry.h"
#include "modules/notify/notify.h"

static NotifyModule notifyModule;

Module* const MODULES[] = {
    &notifyModule,
};
const size_t MODULE_COUNT = sizeof(MODULES) / sizeof(MODULES[0]);
