#pragma once

#include <cstddef>

#include "vf/World.h"

namespace vf {

enum class LoadOutcome { NothingSaved, Damaged, Loaded };

std::size_t saveTerrain(const World& world);
LoadOutcome loadTerrain(World& world);

}
