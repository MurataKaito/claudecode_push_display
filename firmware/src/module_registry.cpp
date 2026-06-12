#include "module_registry.h"

// まだモジュールなし（後続タスクで追加）。nullptrは空配列を避ける番兵でCOUNT=0なら参照されない。
Module* const MODULES[] = { nullptr };
const size_t MODULE_COUNT = 0;
