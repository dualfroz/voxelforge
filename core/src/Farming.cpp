#include "vf/Farming.h"

#include <algorithm>

namespace vf {

namespace {

const Vec3i horizontalDirs[4] = {
    {1, 0, 0}, {-1, 0, 0}, {0, 0, 1}, {0, 0, -1},
};

Block cropBlock(CropType type) {
    switch (type) {
        case CropType::Wheat: return Block::Wheat;
        case CropType::Carrot: return Block::Carrot;
        case CropType::Potato: return Block::Potato;
        case CropType::PumpkinStem: return Block::PumpkinStem;
        case CropType::MelonStem: return Block::MelonStem;
        case CropType::SugarCane: return Block::SugarCane;
        case CropType::Cactus: return Block::Cactus;
        case CropType::Sapling: return Block::Sapling;
        case CropType::Vine: return Block::Vine;
        case CropType::None: return Block::Air;
    }
    return Block::Air;
}

bool bonemealable(CropType type) {
    switch (type) {
        case CropType::Wheat:
        case CropType::Carrot:
        case CropType::Potato:
        case CropType::PumpkinStem:
        case CropType::MelonStem:
        case CropType::Sapling:
            return true;
        default:
            return false;
    }
}

bool tillable(Block b) {
    return b == Block::Farmland || b == Block::Dirt || b == Block::Grass;
}

bool waterAdjacent(const World& world, const Vec3i& pos) {
    for (const Vec3i& dir : horizontalDirs) {
        if (world.blockAt(pos.x + dir.x, pos.y + dir.y, pos.z + dir.z) == Block::Water) return true;
    }
    return false;
}

} // namespace

const char* cropTypeName(CropType type) {
    switch (type) {
        case CropType::None: return "none";
        case CropType::Wheat: return "wheat";
        case CropType::Carrot: return "carrot";
        case CropType::Potato: return "potato";
        case CropType::PumpkinStem: return "pumpkinStem";
        case CropType::MelonStem: return "melonStem";
        case CropType::SugarCane: return "sugarCane";
        case CropType::Cactus: return "cactus";
        case CropType::Sapling: return "sapling";
        case CropType::Vine: return "vine";
    }
    return "unknown";
}

int cropMaxStage(CropType type) {
    switch (type) {
        case CropType::Wheat:
        case CropType::Carrot:
        case CropType::Potato:
        case CropType::PumpkinStem:
        case CropType::MelonStem:
            return 7;
        case CropType::SugarCane:
        case CropType::Cactus:
            return 2;
        case CropType::Sapling:
            return 3;
        case CropType::Vine:
            return 4;
        case CropType::None:
            return 0;
    }
    return 0;
}

Farming::Farming(uint32_t seed, const FarmingConfig& config)
    : config(config), rngState(seed == 0 ? 0x1234567u : seed) {}

uint32_t Farming::nextRandom() {
    rngState ^= rngState << 13;
    rngState ^= rngState >> 17;
    rngState ^= rngState << 5;
    return rngState;
}

bool Farming::rollGrowth(uint32_t denominator, int points) {
    if (points <= 0) return false;
    return nextRandom() % denominator < static_cast<uint32_t>(points);
}

void Farming::plant(World& world, const Vec3i& pos, CropType type) {
    world.setBlock(pos.x, pos.y, pos.z, cropBlock(type));
    crops[pos] = CropState{type, 0, cropMaxStage(type)};
}

bool Farming::isHydrated(const World& world, const Vec3i& farmland) const {
    int r = config.hydrationRadius;
    for (int dx = -r; dx <= r; ++dx) {
        for (int dz = -r; dz <= r; ++dz) {
            for (int dy = 0; dy <= 1; ++dy) {
                if (world.blockAt(farmland.x + dx, farmland.y + dy, farmland.z + dz) == Block::Water)
                    return true;
            }
        }
    }
    return false;
}

const CropState* Farming::stateAt(const Vec3i& pos) const {
    auto it = crops.find(pos);
    return it == crops.end() ? nullptr : &it->second;
}

Farming::GrowthResult Farming::growGroundCrop(World& world, const Vec3i& pos, CropState& state,
                                              int skyLight) {
    if (state.mature()) return GrowthResult::None;
    if (skyLight < config.minGrowthLight) return GrowthResult::None;
    Vec3i soil{pos.x, pos.y - 1, pos.z};
    if (world.blockAt(soil.x, soil.y, soil.z) != Block::Farmland) return GrowthResult::None;
    if (world.blockAt(pos.x, pos.y, pos.z) != cropBlock(state.type)) return GrowthResult::None;

    int points = 1 + (isHydrated(world, soil) ? 2 : 0);
    if (rollGrowth(config.growthDenominator, points)) {
        ++state.stage;
        return GrowthResult::Advanced;
    }
    return GrowthResult::None;
}

Farming::GrowthResult Farming::growStem(World& world, const Vec3i& pos, CropState& state,
                                        int skyLight) {
    if (skyLight < config.minGrowthLight) return GrowthResult::None;
    Vec3i soil{pos.x, pos.y - 1, pos.z};
    if (world.blockAt(soil.x, soil.y, soil.z) != Block::Farmland) return GrowthResult::None;

    if (!state.mature()) {
        int points = 1 + (isHydrated(world, soil) ? 2 : 0);
        if (rollGrowth(config.growthDenominator, points)) {
            ++state.stage;
            return GrowthResult::Advanced;
        }
        return GrowthResult::None;
    }

    Block fruit = state.type == CropType::PumpkinStem ? Block::Pumpkin : Block::Melon;
    for (const Vec3i& dir : horizontalDirs) {
        if (world.blockAt(pos.x + dir.x, pos.y + dir.y, pos.z + dir.z) == fruit)
            return GrowthResult::None;
    }
    if (!rollGrowth(config.growthDenominator, 1)) return GrowthResult::None;

    for (const Vec3i& dir : horizontalDirs) {
        Vec3i spot{pos.x + dir.x, pos.y, pos.z + dir.z};
        if (world.blockAt(spot.x, spot.y, spot.z) != Block::Air) continue;
        if (!tillable(world.blockAt(spot.x, spot.y - 1, spot.z))) continue;
        world.setBlock(spot.x, spot.y, spot.z, fruit);
        return GrowthResult::Advanced;
    }
    return GrowthResult::None;
}

Farming::GrowthResult Farming::growColumn(World& world, const Vec3i& pos, CropState& state,
                                          bool cane) {
    int maxHeight = cane ? config.caneMaxHeight : config.cactusMaxHeight;
    if (state.stage + 1 >= maxHeight) return GrowthResult::None;

    Vec3i top{pos.x, pos.y + state.stage + 1, pos.z};
    if (world.blockAt(top.x, top.y, top.z) != Block::Air) return GrowthResult::None;

    if (cane) {
        if (!waterAdjacent(world, Vec3i{pos.x, pos.y - 1, pos.z})) return GrowthResult::None;
    } else {
        for (const Vec3i& dir : horizontalDirs) {
            if (isSolid(world.blockAt(top.x + dir.x, top.y + dir.y, top.z + dir.z)))
                return GrowthResult::None;
        }
    }

    if (!rollGrowth(config.verticalDenominator, 1)) return GrowthResult::None;
    world.setBlock(top.x, top.y, top.z, cane ? Block::SugarCane : Block::Cactus);
    ++state.stage;
    return GrowthResult::Advanced;
}

Farming::GrowthResult Farming::growVine(World& world, const Vec3i& pos, CropState& state) {
    if (state.stage >= state.maxStage) return GrowthResult::None;

    bool anchored = false;
    for (const Vec3i& dir : horizontalDirs) {
        if (isSolid(world.blockAt(pos.x + dir.x, pos.y + dir.y, pos.z + dir.z))) {
            anchored = true;
            break;
        }
    }
    if (!anchored) return GrowthResult::None;

    Vec3i below{pos.x, pos.y - state.stage - 1, pos.z};
    if (world.blockAt(below.x, below.y, below.z) != Block::Air) return GrowthResult::None;
    if (!rollGrowth(config.verticalDenominator, 1)) return GrowthResult::None;

    world.setBlock(below.x, below.y, below.z, Block::Vine);
    ++state.stage;
    return GrowthResult::Advanced;
}

Farming::GrowthResult Farming::growSapling(World& world, const Vec3i& pos, CropState& state,
                                           int skyLight) {
    if (skyLight < config.minGrowthLight) return GrowthResult::None;
    if (world.blockAt(pos.x, pos.y, pos.z) != Block::Sapling) return GrowthResult::None;

    if (!state.mature()) {
        if (rollGrowth(config.saplingDenominator, 1)) {
            ++state.stage;
            return GrowthResult::Advanced;
        }
        return GrowthResult::None;
    }

    stampTree(world, pos);
    return GrowthResult::Removed;
}

void Farming::stampTree(World& world, const Vec3i& base) {
    int span = config.treeTrunkMax - config.treeTrunkMin + 1;
    int trunk = config.treeTrunkMin + static_cast<int>(nextRandom() % static_cast<uint32_t>(span));
    for (int i = 0; i < trunk; ++i) {
        world.setBlock(base.x, base.y + i, base.z, Block::Wood);
    }
    int topY = base.y + trunk - 1;
    for (int dy = -1; dy <= 1; ++dy) {
        int radius = dy == 1 ? 1 : 2;
        for (int dx = -radius; dx <= radius; ++dx) {
            for (int dz = -radius; dz <= radius; ++dz) {
                if (dx == 0 && dz == 0 && dy <= 0) continue;
                if (std::abs(dx) == radius && std::abs(dz) == radius) continue;
                int y = topY + dy;
                if (world.blockAt(base.x + dx, y, base.z + dz) == Block::Air)
                    world.setBlock(base.x + dx, y, base.z + dz, Block::Leaves);
            }
        }
    }
    world.setBlock(base.x, topY + 1, base.z, Block::Leaves);
}

bool Farming::randomTick(World& world, const Vec3i& pos, int skyLight) {
    auto it = crops.find(pos);
    if (it == crops.end()) return false;
    CropState& state = it->second;

    GrowthResult result = GrowthResult::None;
    switch (state.type) {
        case CropType::Wheat:
        case CropType::Carrot:
        case CropType::Potato:
            result = growGroundCrop(world, pos, state, skyLight);
            break;
        case CropType::PumpkinStem:
        case CropType::MelonStem:
            result = growStem(world, pos, state, skyLight);
            break;
        case CropType::SugarCane:
            result = growColumn(world, pos, state, true);
            break;
        case CropType::Cactus:
            result = growColumn(world, pos, state, false);
            break;
        case CropType::Vine:
            result = growVine(world, pos, state);
            break;
        case CropType::Sapling:
            result = growSapling(world, pos, state, skyLight);
            break;
        case CropType::None:
            break;
    }

    if (result == GrowthResult::Removed) crops.erase(pos);
    return result != GrowthResult::None;
}

int Farming::growStep(World& world, int skyLight) {
    std::vector<Vec3i> positions;
    positions.reserve(crops.size());
    for (const auto& entry : crops) positions.push_back(entry.first);
    std::sort(positions.begin(), positions.end(), [](const Vec3i& a, const Vec3i& b) {
        if (a.x != b.x) return a.x < b.x;
        if (a.y != b.y) return a.y < b.y;
        return a.z < b.z;
    });

    int advanced = 0;
    for (const Vec3i& pos : positions) {
        if (randomTick(world, pos, skyLight)) ++advanced;
    }
    return advanced;
}

bool Farming::applyBonemeal(World& world, const Vec3i& pos) {
    auto it = crops.find(pos);
    if (it == crops.end()) return false;
    CropState& state = it->second;
    if (!bonemealable(state.type)) return false;

    if (state.type == CropType::Sapling) {
        if (nextRandom() % 100 < config.bonemealSaplingChance) {
            stampTree(world, pos);
            crops.erase(pos);
        }
        return true;
    }

    if (state.mature()) return false;
    int span = config.bonemealMaxStages - config.bonemealMinStages + 1;
    int boost = config.bonemealMinStages + static_cast<int>(nextRandom() % static_cast<uint32_t>(span));
    state.stage += boost;
    if (state.stage > state.maxStage) state.stage = state.maxStage;
    return true;
}

bool Farming::trample(World& world, const Vec3i& farmland) {
    if (world.blockAt(farmland.x, farmland.y, farmland.z) != Block::Farmland) return false;
    world.setBlock(farmland.x, farmland.y, farmland.z, Block::Dirt);

    Vec3i above{farmland.x, farmland.y + 1, farmland.z};
    auto it = crops.find(above);
    if (it != crops.end()) {
        CropType type = it->second.type;
        if (type == CropType::Wheat || type == CropType::Carrot || type == CropType::Potato ||
            type == CropType::PumpkinStem || type == CropType::MelonStem) {
            world.setBlock(above.x, above.y, above.z, Block::Air);
            crops.erase(above);
        }
    }
    return true;
}

std::vector<HarvestDrop> Farming::harvest(World& world, const Vec3i& pos) {
    auto it = crops.find(pos);
    if (it == crops.end()) return {};
    CropState state = it->second;
    std::vector<HarvestDrop> drops;

    switch (state.type) {
        case CropType::Wheat:
            if (state.mature()) drops.push_back({Block::Wheat, 1});
            world.setBlock(pos.x, pos.y, pos.z, Block::Air);
            break;
        case CropType::Carrot:
            if (state.mature()) drops.push_back({Block::Carrot, 1 + static_cast<int>(nextRandom() % 3)});
            world.setBlock(pos.x, pos.y, pos.z, Block::Air);
            break;
        case CropType::Potato:
            if (state.mature()) drops.push_back({Block::Potato, 1 + static_cast<int>(nextRandom() % 3)});
            world.setBlock(pos.x, pos.y, pos.z, Block::Air);
            break;
        case CropType::PumpkinStem:
        case CropType::MelonStem:
            world.setBlock(pos.x, pos.y, pos.z, Block::Air);
            break;
        case CropType::SugarCane: {
            int height = state.stage + 1;
            drops.push_back({Block::SugarCane, height});
            for (int i = 0; i < height; ++i) world.setBlock(pos.x, pos.y + i, pos.z, Block::Air);
            break;
        }
        case CropType::Cactus: {
            int height = state.stage + 1;
            drops.push_back({Block::Cactus, height});
            for (int i = 0; i < height; ++i) world.setBlock(pos.x, pos.y + i, pos.z, Block::Air);
            break;
        }
        case CropType::Vine: {
            int length = state.stage + 1;
            for (int i = 0; i < length; ++i) world.setBlock(pos.x, pos.y - i, pos.z, Block::Air);
            break;
        }
        case CropType::Sapling:
            drops.push_back({Block::Sapling, 1});
            world.setBlock(pos.x, pos.y, pos.z, Block::Air);
            break;
        case CropType::None:
            break;
    }

    crops.erase(pos);
    return drops;
}

} // namespace vf
