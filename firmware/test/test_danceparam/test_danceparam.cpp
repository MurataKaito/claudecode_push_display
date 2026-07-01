#include <unity.h>
#include "danceparam.h"

void test_default_when_zero(void) {
  TEST_ASSERT_EQUAL_UINT32(6000, clampDanceMs(0));  // 未指定
}

void test_default_when_negative(void) {
  TEST_ASSERT_EQUAL_UINT32(6000, clampDanceMs(-100));
}

void test_clamp_below_min(void) {
  TEST_ASSERT_EQUAL_UINT32(500, clampDanceMs(100));  // 下限
}

void test_clamp_above_max(void) {
  TEST_ASSERT_EQUAL_UINT32(30000, clampDanceMs(999999));  // 上限
}

void test_passthrough_in_range(void) {
  TEST_ASSERT_EQUAL_UINT32(500, clampDanceMs(500));      // 境界(下)
  TEST_ASSERT_EQUAL_UINT32(6000, clampDanceMs(6000));    // 中間
  TEST_ASSERT_EQUAL_UINT32(30000, clampDanceMs(30000));  // 境界(上)
}

void setUp(void) {}
void tearDown(void) {}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_default_when_zero);
  RUN_TEST(test_default_when_negative);
  RUN_TEST(test_clamp_below_min);
  RUN_TEST(test_clamp_above_max);
  RUN_TEST(test_passthrough_in_range);
  return UNITY_END();
}
