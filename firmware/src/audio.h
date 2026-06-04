#pragma once
#include <stdint.h>
#include <stddef.h>

void audioBegin();
// WAVバイト列を解析してスピーカー再生する。成功時 true。
// data はこの関数の戻り後も呼び出し側が再生終了まで保持すること。
bool playWav(const uint8_t* data, size_t len);
