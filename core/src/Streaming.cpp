#include "vf/Streaming.h"

#include <algorithm>

namespace vf {

namespace {

long long distanceSquared(ColumnCoord a, ColumnCoord b) {
    long long dx = a.x - b.x;
    long long dz = a.z - b.z;
    return dx * dx + dz * dz;
}

} // namespace

std::vector<ColumnCoord> columnsInRadius(ColumnCoord centre, int radius) {
    std::vector<ColumnCoord> columns;
    long long limit = static_cast<long long>(radius) * radius;
    for (int dz = -radius; dz <= radius; ++dz) {
        for (int dx = -radius; dx <= radius; ++dx) {
            ColumnCoord column{centre.x + dx, centre.z + dz};
            if (distanceSquared(column, centre) <= limit) columns.push_back(column);
        }
    }
    std::stable_sort(columns.begin(), columns.end(),
                     [&](ColumnCoord lhs, ColumnCoord rhs) {
                         return distanceSquared(lhs, centre) < distanceSquared(rhs, centre);
                     });
    return columns;
}

StreamDelta ChunkStreamer::update(ColumnCoord centre) {
    StreamDelta delta;

    for (ColumnCoord column : columnsInRadius(centre, config.loadRadius)) {
        if (resident.count(column) == 0) delta.load.push_back(column);
    }

    long long unloadLimit = static_cast<long long>(config.unloadRadius) * config.unloadRadius;
    for (ColumnCoord column : resident) {
        if (distanceSquared(column, centre) > unloadLimit) delta.unload.push_back(column);
    }

    for (ColumnCoord column : delta.load) resident.insert(column);
    for (ColumnCoord column : delta.unload) resident.erase(column);

    return delta;
}

} // namespace vf
