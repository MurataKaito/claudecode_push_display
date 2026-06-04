#include "wavparse.h"
#include <string.h>

static uint32_t rdU32(const uint8_t* p) {
  return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}
static uint16_t rdU16(const uint8_t* p) {
  return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

bool parseWav(const uint8_t* buf, size_t len, WavInfo* out) {
  if (!buf || !out || len < 12) return false;
  if (memcmp(buf, "RIFF", 4) != 0) return false;
  if (memcmp(buf + 8, "WAVE", 4) != 0) return false;

  size_t pos = 12;
  bool haveFmt = false, haveData = false;
  while (pos + 8 <= len) {
    const uint8_t* id = buf + pos;
    uint32_t sz = rdU32(buf + pos + 4);
    size_t body = pos + 8;
    if (memcmp(id, "fmt ", 4) == 0 && body + 16 <= len) {
      out->channels = rdU16(buf + body + 2);
      out->sampleRate = rdU32(buf + body + 4);
      out->bitsPerSample = rdU16(buf + body + 14);
      haveFmt = true;
    } else if (memcmp(id, "data", 4) == 0) {
      out->dataOffset = body;
      out->dataLen = (body + sz <= len) ? (size_t)sz : (len - body);
      haveData = true;
    }
    pos = body + sz + (sz & 1);  // チャンクは偶数境界
  }
  return haveFmt && haveData;
}
