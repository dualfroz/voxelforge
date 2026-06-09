#pragma once

#include <cstdint>
#include <vector>
#include "World.h"

namespace vf {

// Binary world format "VXF1": magic, version, chunk count, then per chunk the
// coordinate and a run-length-encoded block stream. Compact for the large
// runs of air and stone that voxel terrain produces.
std::vector<uint8_t> serializeWorld(const World& world);

// Rebuilds `out` from a serialized buffer. Returns false on a bad magic,
// version, or truncated stream.
bool deserializeWorld(const uint8_t* data, std::size_t size, World& out);

} // namespace vf
