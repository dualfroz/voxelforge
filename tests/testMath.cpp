#include "testHarness.h"
#include "vf/Math.h"

using namespace vf;

VF_TEST(mathDotAndCross) {
    Vec3 a{1, 0, 0};
    Vec3 b{0, 1, 0};
    CHECK_NEAR(dot(a, b), 0.0f, 1e-6f);
    Vec3 c = cross(a, b);
    CHECK_NEAR(c.x, 0.0f, 1e-6f);
    CHECK_NEAR(c.y, 0.0f, 1e-6f);
    CHECK_NEAR(c.z, 1.0f, 1e-6f);
}

VF_TEST(mathNormalizeUnitLength) {
    Vec3 n = normalize(Vec3{3, 0, 4});
    CHECK_NEAR(length(n), 1.0f, 1e-6f);
    CHECK_NEAR(n.x, 0.6f, 1e-6f);
    CHECK_NEAR(n.z, 0.8f, 1e-6f);
}

VF_TEST(mathIdentityMultiply) {
    Mat4 id = Mat4::identity();
    Mat4 t = translation({2, 3, 4});
    Mat4 product = id * t;
    for (int i = 0; i < 16; ++i) CHECK_NEAR(product.m[i], t.m[i], 1e-6f);
}

VF_TEST(mathTranslationLayout) {
    Mat4 t = translation({5, 6, 7});
    CHECK_NEAR(t.at(3, 0), 5.0f, 1e-6f);
    CHECK_NEAR(t.at(3, 1), 6.0f, 1e-6f);
    CHECK_NEAR(t.at(3, 2), 7.0f, 1e-6f);
    CHECK_NEAR(t.at(0, 0), 1.0f, 1e-6f);
}

VF_TEST(mathMatrixMultiplyComposes) {
    Mat4 a = translation({1, 0, 0});
    Mat4 b = translation({0, 2, 0});
    Mat4 ab = a * b;
    CHECK_NEAR(ab.at(3, 0), 1.0f, 1e-6f);
    CHECK_NEAR(ab.at(3, 1), 2.0f, 1e-6f);
}

VF_TEST(mathYawPitchDirection) {
    Vec3 forward = directionFromYawPitch(0.0f, 0.0f);
    CHECK_NEAR(forward.x, 0.0f, 1e-6f);
    CHECK_NEAR(forward.y, 0.0f, 1e-6f);
    CHECK_NEAR(forward.z, -1.0f, 1e-6f);
}
