#include "vf/Climate.h"

#include <cmath>

namespace vf {

namespace {

bool isHeatSource(Block b) {
    switch (b) {
        case Block::Lava:
        case Block::Torch:
        case Block::Glowstone:
        case Block::FurnaceLit:
        case Block::RedstoneTorch:
            return true;
        default:
            return false;
    }
}

bool isFreezable(Block b) { return b == Block::Water; }

const Vec3i neighbourOffsets[6] = {
    {1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1},
};

} // namespace

Climate::Climate(const BiomeGen& biomes, const ClimateConfig& config)
    : biomes(biomes), config(config) {}

float Climate::temperatureAt(int gx, int gy, int gz) const {
    ClimateSample sample = biomes.climateAt(gx, gz);
    float elevation = static_cast<float>(gy - config.seaLevel);
    float heightAdjust = elevation > 0.0f ? -config.heightLapse * elevation : 0.0f;
    float latitudeAdjust = -config.latitudeLapse * static_cast<float>(std::abs(gz));
    return sample.temperature + heightAdjust + latitudeAdjust;
}

float Climate::humidityAt(int gx, int gz) const {
    return biomes.climateAt(gx, gz).humidity;
}

bool Climate::freezingAt(int gx, int gy, int gz) const {
    return temperatureAt(gx, gy, gz) < config.freezeTemperature;
}

bool Climate::thawingAt(int gx, int gy, int gz) const {
    return temperatureAt(gx, gy, gz) >= config.meltTemperature;
}

bool Climate::heatSourceNear(const World& world, const Vec3i& pos) const {
    for (const Vec3i& off : neighbourOffsets) {
        if (isHeatSource(world.blockAt(pos.x + off.x, pos.y + off.y, pos.z + off.z))) return true;
    }
    return false;
}

bool Climate::iceNear(const World& world, const Vec3i& pos) const {
    for (const Vec3i& off : neighbourOffsets) {
        Block b = world.blockAt(pos.x + off.x, pos.y + off.y, pos.z + off.z);
        if (b == Block::Ice || b == Block::PackedIce) return true;
    }
    return false;
}

int Climate::snowDepthAt(const Vec3i& pos) const {
    auto it = snowDepth.find(pos);
    return it == snowDepth.end() ? 0 : it->second;
}

int Climate::iceAgeAt(const Vec3i& pos) const {
    auto it = iceAge.find(pos);
    return it == iceAge.end() ? 0 : it->second;
}

std::vector<ClimateChange> Climate::tick(World& world, const Vec3i& origin, const Vec3i& dims,
                                       Precipitation precip) {
    std::vector<ClimateChange> changes;
    int x0 = origin.x, x1 = origin.x + dims.x;
    int y0 = origin.y, y1 = origin.y + dims.y;
    int z0 = origin.z, z1 = origin.z + dims.z;

    for (int x = x0; x < x1; ++x) {
        for (int z = z0; z < z1; ++z) {
            for (int y = y1 - 1; y >= y0; --y) {
                Vec3i cell{x, y, z};
                Block here = world.blockAt(x, y, z);
                Block above = world.blockAt(x, y + 1, z);
                Block below = world.blockAt(x, y - 1, z);
                float temperature = temperatureAt(x, y, z);
                bool cold = temperature < config.freezeTemperature;
                bool warm = temperature >= config.meltTemperature;
                bool warmOrHeated = warm || heatSourceNear(world, cell);

                if (isFreezable(here) && above == Block::Air) {
                    bool frostCreep = temperature < config.frostSpreadTemperature &&
                                      iceNear(world, cell);
                    if (cold || frostCreep) {
                        changes.push_back({cell, here, Block::Ice});
                        continue;
                    }
                }

                if (here == Block::Ice) {
                    if (warmOrHeated) {
                        changes.push_back({cell, here, Block::Water});
                        snowDepth.erase(cell);
                        iceAge.erase(cell);
                    } else if (cold) {
                        int age = iceAgeAt(cell) + 1;
                        if (age >= config.packThreshold) {
                            changes.push_back({cell, here, Block::PackedIce});
                            iceAge.erase(cell);
                        } else {
                            iceAge[cell] = age;
                        }
                    }
                    continue;
                }

                if (here == Block::PackedIce) {
                    if (warmOrHeated) changes.push_back({cell, here, Block::Water});
                    continue;
                }

                if (here == Block::SnowLayer) {
                    if (warmOrHeated) {
                        int depth = snowDepthAt(cell) - 1;
                        if (depth <= 0) {
                            changes.push_back({cell, here, Block::Air});
                            snowDepth.erase(cell);
                        } else {
                            snowDepth[cell] = depth;
                        }
                    } else if (precip == Precipitation::Snow && cold) {
                        int depth = snowDepthAt(cell);
                        if (depth < config.maxSnowDepth) snowDepth[cell] = depth + 1;
                    }
                    continue;
                }

                if (precip == Precipitation::Snow && cold && here == Block::Air &&
                    isOpaque(below) && below != Block::Ice) {
                    changes.push_back({cell, Block::Air, Block::SnowLayer});
                    snowDepth[cell] = 1;
                    continue;
                }
            }
        }
    }
    return changes;
}

void Climate::apply(World& world, const std::vector<ClimateChange>& changes) {
    for (const ClimateChange& change : changes) {
        world.setBlock(change.pos.x, change.pos.y, change.pos.z, change.to);
    }
}

} // namespace vf
