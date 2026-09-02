#include <unity.h>
#include "Vec3.h"

void test_norm_of_345_triangle(void) {
    Vec3 v{3.0, 4.0, 0.0};
    TEST_ASSERT_DOUBLE_WITHIN(1e-12, 5.0, v.norm());
}

void test_dot_product(void) {
    Vec3 a{1.0, 2.0, 3.0};
    Vec3 b{4.0, -5.0, 6.0};
    TEST_ASSERT_DOUBLE_WITHIN(1e-12, 12.0, a.dot(b));
}

void test_subtraction(void) {
    Vec3 a{5.0, 5.0, 5.0};
    Vec3 b{1.0, 2.0, 3.0};
    Vec3 c = a - b;
    TEST_ASSERT_DOUBLE_WITHIN(1e-12, 4.0, c.x);
    TEST_ASSERT_DOUBLE_WITHIN(1e-12, 3.0, c.y);
    TEST_ASSERT_DOUBLE_WITHIN(1e-12, 2.0, c.z);
}

void test_unit_has_length_one(void) {
    Vec3 v{3.0, 4.0, 0.0};
    TEST_ASSERT_DOUBLE_WITHIN(1e-12, 1.0, v.unit().norm());
}

void test_unit_of_zero_vector_is_zero_not_nan(void) {
    Vec3 z{0.0, 0.0, 0.0};
    Vec3 u = z.unit();
    TEST_ASSERT_DOUBLE_WITHIN(1e-12, 0.0, u.x);
    TEST_ASSERT_DOUBLE_WITHIN(1e-12, 0.0, u.y);
    TEST_ASSERT_DOUBLE_WITHIN(1e-12, 0.0, u.z);
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_norm_of_345_triangle);
    RUN_TEST(test_dot_product);
    RUN_TEST(test_subtraction);
    RUN_TEST(test_unit_has_length_one);
    RUN_TEST(test_unit_of_zero_vector_is_zero_not_nan);
    return UNITY_END();
}
