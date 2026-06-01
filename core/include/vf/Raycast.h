#pragma once

#include "World.h"
#include "Math.h"

namespace vf {

struct RaycastHit {
    bool hit = false;
    Vec3i block;    // solid voxel that was struck
    Vec3i normal;   // face normal pointing back toward the ray origin
    float distance = 0.0f;
};

// Amanatides and Woo voxel traversal. Steps the grid from `origin` along
// `direction` up to `maxDistance` world units, stopping at the first solid
// voxel. `normal` gives the entered face, so `block + normal` is the empty
// neighbour where a new block can be placed.
RaycastHit raycastVoxels(const World& world, const Vec3& origin, const Vec3& direction,
                         float maxDistance);

} // namespace vf
