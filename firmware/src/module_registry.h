#pragma once
#include "core/module.h"

// 有効モジュール一覧。ここに登録されたものだけがビルドに含まれる（部分組み込み）。
// 並び順 = ジェスチャのフォールバック順（前面モジュールの次に先頭から試行）。
extern Module* const MODULES[];
extern const size_t MODULE_COUNT;
