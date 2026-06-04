#include <unity.h>
#include "wavparse.h"

// 24kHz/16bit/mono、PCM 4バイト(2サンプル)の最小WAV。
static const uint8_t WAV[] = {
  'R','I','F','F', 0x28,0,0,0, 'W','A','V','E',
  'f','m','t',' ', 16,0,0,0, 1,0, 1,0,
  0xC0,0x5D,0,0,            // sampleRate = 24000
  0x80,0xBB,0,0,            // byteRate
  2,0, 16,0,               // blockAlign=2, bitsPerSample=16
  'd','a','t','a', 4,0,0,0, 0x11,0x22,0x33,0x44
};

void test_parses_fmt_and_data(void) {
  WavInfo info;
  TEST_ASSERT_TRUE(parseWav(WAV, sizeof(WAV), &info));
  TEST_ASSERT_EQUAL_UINT32(24000, info.sampleRate);
  TEST_ASSERT_EQUAL_UINT16(1, info.channels);
  TEST_ASSERT_EQUAL_UINT16(16, info.bitsPerSample);
  TEST_ASSERT_EQUAL_UINT32(4, info.dataLen);
  TEST_ASSERT_EQUAL_UINT8(0x22, WAV[info.dataOffset + 1]);
}

void test_rejects_non_riff(void) {
  const uint8_t bad[12] = {0};
  WavInfo info;
  TEST_ASSERT_FALSE(parseWav(bad, sizeof(bad), &info));
}

void setUp(void) {}
void tearDown(void) {}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_parses_fmt_and_data);
  RUN_TEST(test_rejects_non_riff);
  return UNITY_END();
}
