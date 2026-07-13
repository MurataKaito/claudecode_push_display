#pragma once
#include <stdint.h>

// /dance の ms を正規化する。ms<=0（未指定）は既定6000ms、それ以外は[500,30000]にクランプ。
uint32_t clampDanceMs(long ms);
