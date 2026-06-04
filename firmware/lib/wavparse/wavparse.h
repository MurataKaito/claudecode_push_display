#pragma once
#include <stdint.h>
#include <stddef.h>

struct WavInfo {
  uint32_t sampleRate;
  uint16_t channels;
  uint16_t bitsPerSample;
  size_t dataOffset;  // PCM本体の先頭オフセット
  size_t dataLen;     // PCM本体のバイト数
};

// RIFFチャンクを走査して fmt と data を見つける。成功時 true。
bool parseWav(const uint8_t* buf, size_t len, WavInfo* out);
