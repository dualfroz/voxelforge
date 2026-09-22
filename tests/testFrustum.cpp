#include "testHarness.h"
#include "vf/Frustum.h"

using namespace vf;

namespace {

Frustum makeFrustum() {
    Mat4 proj = perspective(1.0472f, 1.3333f, 0.1f, 100.0f);
    Mat4 view = lookAt({0, 0, 5}, {0, 0, 0}, {0, 1, 0});
    return Frustum::fromMatrix(proj * view);
}

}

VF_TEST(frustumPointInFrontIsVisible) {
    Frustum f = makeFrustum();
    CHECK(f.containsPoint({0, 0, 0}));
    CHECK(f.containsPoint({0, 0, -10}));
}

VF_TEST(frustumPointBehindCameraIsCulled) {
    Frustum f = makeFrustum();
    CHECK(!f.containsPoint({0, 0, 10}));
}

VF_TEST(frustumPointBeyondFarIsCulled) {
    Frustum f = makeFrustum();
    CHECK(!f.containsPoint({0, 0, -200}));
}

VF_TEST(frustumWideOffAxisPointIsCulled) {
    Frustum f = makeFrustum();
    CHECK(!f.containsPoint({100, 0, -1}));
}

VF_TEST(frustumAabbInViewIntersects) {
    Frustum f = makeFrustum();
    Aabb box{{-1, -1, -2}, {1, 1, 0}};
    CHECK(f.intersectsAabb(box));
}

VF_TEST(frustumAabbBehindCameraCulled) {
    Frustum f = makeFrustum();
    Aabb box{{-1, -1, 20}, {1, 1, 22}};
    CHECK(!f.intersectsAabb(box));
}
