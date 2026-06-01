#include "vf/Raycast.h"

#include <cmath>

namespace vf {

RaycastHit raycastVoxels(const World& world, const Vec3& origin, const Vec3& direction,
                         float maxDistance) {
    RaycastHit result;
    Vec3 dir = normalize(direction);
    if (length(direction) <= 1e-8f) return result;

    int vx = static_cast<int>(std::floor(origin.x));
    int vy = static_cast<int>(std::floor(origin.y));
    int vz = static_cast<int>(std::floor(origin.z));

    int stepX = dir.x > 0 ? 1 : (dir.x < 0 ? -1 : 0);
    int stepY = dir.y > 0 ? 1 : (dir.y < 0 ? -1 : 0);
    int stepZ = dir.z > 0 ? 1 : (dir.z < 0 ? -1 : 0);

    const float inf = 1e30f;
    float tDeltaX = dir.x != 0.0f ? std::fabs(1.0f / dir.x) : inf;
    float tDeltaY = dir.y != 0.0f ? std::fabs(1.0f / dir.y) : inf;
    float tDeltaZ = dir.z != 0.0f ? std::fabs(1.0f / dir.z) : inf;

    auto boundary = [](float o, int step) {
        float cell = std::floor(o);
        return step > 0 ? (cell + 1.0f - o) : (o - cell);
    };
    float tMaxX = dir.x != 0.0f ? boundary(origin.x, stepX) * tDeltaX : inf;
    float tMaxY = dir.y != 0.0f ? boundary(origin.y, stepY) * tDeltaY : inf;
    float tMaxZ = dir.z != 0.0f ? boundary(origin.z, stepZ) * tDeltaZ : inf;

    Vec3i normal;
    float t = 0.0f;

    if (world.solidAt(vx, vy, vz)) {
        result.hit = true;
        result.block = {vx, vy, vz};
        result.normal = {0, 0, 0};
        return result;
    }

    while (t <= maxDistance) {
        if (tMaxX < tMaxY && tMaxX < tMaxZ) {
            vx += stepX;
            t = tMaxX;
            tMaxX += tDeltaX;
            normal = {-stepX, 0, 0};
        } else if (tMaxY < tMaxZ) {
            vy += stepY;
            t = tMaxY;
            tMaxY += tDeltaY;
            normal = {0, -stepY, 0};
        } else {
            vz += stepZ;
            t = tMaxZ;
            tMaxZ += tDeltaZ;
            normal = {0, 0, -stepZ};
        }

        if (t > maxDistance) break;

        if (world.solidAt(vx, vy, vz)) {
            result.hit = true;
            result.block = {vx, vy, vz};
            result.normal = normal;
            result.distance = t;
            return result;
        }
    }

    return result;
}

} // namespace vf
