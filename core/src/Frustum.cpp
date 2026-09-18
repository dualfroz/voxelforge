#include "vf/Frustum.h"

#include <array>
#include <cmath>

namespace vf {

namespace {

Plane normalized(float a, float b, float c, float d) {
    float len = std::sqrt(a * a + b * b + c * c);
    if (len <= 1e-8f) return Plane{a, b, c, d};
    float inv = 1.0f / len;
    return Plane{a * inv, b * inv, c * inv, d * inv};
}

}

Frustum Frustum::fromMatrix(const Mat4& vp) {
    auto el = [&](int row, int col) { return vp.at(col, row); };
    auto row = [&](int r) {
        return std::array<float, 4>{el(r, 0), el(r, 1), el(r, 2), el(r, 3)};
    };
    auto r0 = row(0);
    auto r1 = row(1);
    auto r2 = row(2);
    auto r3 = row(3);

    Frustum f;
    f.planes[0] = normalized(r3[0] + r0[0], r3[1] + r0[1], r3[2] + r0[2], r3[3] + r0[3]);
    f.planes[1] = normalized(r3[0] - r0[0], r3[1] - r0[1], r3[2] - r0[2], r3[3] - r0[3]);
    f.planes[2] = normalized(r3[0] + r1[0], r3[1] + r1[1], r3[2] + r1[2], r3[3] + r1[3]);
    f.planes[3] = normalized(r3[0] - r1[0], r3[1] - r1[1], r3[2] - r1[2], r3[3] - r1[3]);
    f.planes[4] = normalized(r3[0] + r2[0], r3[1] + r2[1], r3[2] + r2[2], r3[3] + r2[3]);
    f.planes[5] = normalized(r3[0] - r2[0], r3[1] - r2[1], r3[2] - r2[2], r3[3] - r2[3]);
    return f;
}

bool Frustum::containsPoint(const Vec3& p) const {
    for (const Plane& pl : planes)
        if (pl.nx * p.x + pl.ny * p.y + pl.nz * p.z + pl.d < 0.0f) return false;
    return true;
}

bool Frustum::intersectsAabb(const Aabb& box) const {
    for (const Plane& pl : planes) {
        float px = pl.nx >= 0.0f ? box.max.x : box.min.x;
        float py = pl.ny >= 0.0f ? box.max.y : box.min.y;
        float pz = pl.nz >= 0.0f ? box.max.z : box.min.z;
        if (pl.nx * px + pl.ny * py + pl.nz * pz + pl.d < 0.0f) return false;
    }
    return true;
}

}
