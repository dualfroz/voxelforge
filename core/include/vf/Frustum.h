#pragma once

#include "Math.h"
#include "Physics.h"

namespace vf {

struct Plane {
    float nx = 0.0f, ny = 0.0f, nz = 0.0f, d = 0.0f;
};

class Frustum {
public:
    static Frustum fromMatrix(const Mat4& viewProjection);

    bool containsPoint(const Vec3& p) const;
    bool intersectsAabb(const Aabb& box) const;

    const Plane& plane(int i) const { return planes[i]; }

private:
    Plane planes[6];
};

}
