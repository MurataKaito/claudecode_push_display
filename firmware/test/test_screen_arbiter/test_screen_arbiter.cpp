#include <unity.h>
#include "screen_arbiter.h"

void test_grant_when_vacant(void) {
  ScreenArbiter a;
  int pre = -2;
  TEST_ASSERT_TRUE(a.request(0, 50, 1000, 0, &pre));
  TEST_ASSERT_EQUAL_INT(-1, pre);
  TEST_ASSERT_EQUAL_INT(0, a.owner());
}

void test_vacant_owner_is_minus1(void) {
  ScreenArbiter a;
  TEST_ASSERT_EQUAL_INT(-1, a.owner());
  TEST_ASSERT_EQUAL_INT(-1, a.tick(100));
}

void test_deny_lower_priority(void) {
  ScreenArbiter a;
  a.request(0, 100, 30000, 0, nullptr);  // 承認が前面
  int pre = -2;
  TEST_ASSERT_FALSE(a.request(1, 50, 4000, 10, &pre));  // 通知は拒否
  TEST_ASSERT_EQUAL_INT(-1, pre);
  TEST_ASSERT_EQUAL_INT(0, a.owner());
}

void test_equal_priority_last_wins(void) {
  ScreenArbiter a;
  a.request(0, 50, 4000, 0, nullptr);  // 通知が前面
  int pre = -2;
  TEST_ASSERT_TRUE(a.request(1, 50, 5000, 10, &pre));  // タップ→使用率が奪う
  TEST_ASSERT_EQUAL_INT(0, pre);  // 被横取りを通知
  TEST_ASSERT_EQUAL_INT(1, a.owner());
}

void test_higher_priority_preempts(void) {
  ScreenArbiter a;
  a.request(1, 50, 4000, 0, nullptr);
  int pre = -2;
  TEST_ASSERT_TRUE(a.request(0, 100, 30000, 10, &pre));
  TEST_ASSERT_EQUAL_INT(1, pre);
  TEST_ASSERT_EQUAL_INT(0, a.owner());
}

void test_same_owner_rerequest_always_granted(void) {
  ScreenArbiter a;
  a.request(0, 100, 30000, 0, nullptr);
  int pre = -2;
  // 承認がフィードバック表示のため自分の優先度を下げるケース
  TEST_ASSERT_TRUE(a.request(0, 50, 1800, 10, &pre));
  TEST_ASSERT_EQUAL_INT(-1, pre);  // 自分自身はpreemptedにしない
  TEST_ASSERT_EQUAL_INT(0, a.owner());
}

void test_timeout_releases(void) {
  ScreenArbiter a;
  a.request(0, 50, 1000, 0, nullptr);
  TEST_ASSERT_EQUAL_INT(-1, a.tick(999));   // 期限前
  TEST_ASSERT_EQUAL_INT(0, a.tick(1000));   // 期限到来でowner idを返す
  TEST_ASSERT_EQUAL_INT(-1, a.owner());
  TEST_ASSERT_EQUAL_INT(-1, a.tick(1001));  // 二重発火しない
}

void test_release_by_owner(void) {
  ScreenArbiter a;
  a.request(0, 50, 1000, 0, nullptr);
  a.release(0);
  TEST_ASSERT_EQUAL_INT(-1, a.owner());
}

void test_release_by_non_owner_ignored(void) {
  ScreenArbiter a;
  a.request(0, 50, 1000, 0, nullptr);
  a.release(1);
  TEST_ASSERT_EQUAL_INT(0, a.owner());
}

void test_grant_after_timeout(void) {
  ScreenArbiter a;
  a.request(0, 100, 1000, 0, nullptr);
  a.tick(1000);
  TEST_ASSERT_TRUE(a.request(1, 50, 4000, 1100, nullptr));
}

void test_millis_wraparound(void) {
  ScreenArbiter a;
  // millis()が32bit上限間際 → deadlineがラップしても正しく判定できること
  a.request(0, 50, 0x200, 0xFFFFFF00u, nullptr);
  TEST_ASSERT_EQUAL_INT(-1, a.tick(0xFFFFFFF0u));  // まだ期限前
  TEST_ASSERT_EQUAL_INT(0, a.tick(0x100u));        // ラップ後に期限到来
}

void setUp(void) {}
void tearDown(void) {}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_grant_when_vacant);
  RUN_TEST(test_vacant_owner_is_minus1);
  RUN_TEST(test_deny_lower_priority);
  RUN_TEST(test_equal_priority_last_wins);
  RUN_TEST(test_higher_priority_preempts);
  RUN_TEST(test_same_owner_rerequest_always_granted);
  RUN_TEST(test_timeout_releases);
  RUN_TEST(test_release_by_owner);
  RUN_TEST(test_release_by_non_owner_ignored);
  RUN_TEST(test_grant_after_timeout);
  RUN_TEST(test_millis_wraparound);
  return UNITY_END();
}
