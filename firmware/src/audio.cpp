#include "audio.h"
#include <M5Unified.h>
#include "wavparse.h"

void audioBegin() {
  auto cfg = M5.Speaker.config();
  cfg.sample_rate = 24000;  // VOICEVOX 既定
  M5.Speaker.config(cfg);
  M5.Speaker.begin();
  M5.Speaker.setVolume(255);  // 0-255。最大。
}

bool playWav(const uint8_t* data, size_t len) {
  WavInfo info;
  if (!parseWav(data, len, &info)) return false;
  if (info.bitsPerSample != 16) return false;
  const int16_t* pcm = reinterpret_cast<const int16_t*>(data + info.dataOffset);
  const size_t samples = info.dataLen / 2;
  const bool stereo = info.channels > 1;
  // (data, len, sample_rate, stereo, repeat, channel=-1, stop_current=true)
  return M5.Speaker.playRaw(pcm, samples, info.sampleRate, stereo, 1, -1, true);
}
