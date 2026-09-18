#pragma once

#include "World.h"
#include "Math.h"

namespace vf {

struct RaycastHit {
    bool hit = false;
    Vec3i block;
    Vec3i normal;
    float distance = 0.0f;
};

RaycastHit raycastVoxels(const World& world, const Vec3& origin, const Vec3& direction,
                         float maxDistance);

}
