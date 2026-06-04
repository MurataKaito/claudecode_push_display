#include <unity.h>
#include "gesture.h"

void test_single_tap(void) {
  GestureDetector g;
  g.down(0, 100, 100);
  TEST_ASSERT_EQUAL_INT(GESTURE_TAP, g.up(100, 102, 101));
}

void test_double_tap(void) {
  GestureDetector g;
  g.down(0, 100, 100);
  TEST_ASSERT_EQUAL_INT(GESTURE_TAP, g.up(100, 100, 100));
  g.down(200, 100, 100);
  TEST_ASSERT_EQUAL_INT(GESTURE_DOUBLETAP, g.up(300, 100, 100));
}

void test_swipe(void) {
  GestureDetector g;
  g.down(0, 10, 100);
  TEST_ASSERT_EQUAL_INT(GESTURE_SWIPE, g.up(150, 200, 100));
}

void test_slow_press_is_none(void) {
  GestureDetector g;
  g.down(0, 100, 100);
  TEST_ASSERT_EQUAL_INT(GESTURE_NONE, g.up(5000, 100, 100));
}

void setUp(void) {}
void tearDown(void) {}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_single_tap);
  RUN_TEST(test_double_tap);
  RUN_TEST(test_swipe);
  RUN_TEST(test_slow_press_is_none);
  return UNITY_END();
}
