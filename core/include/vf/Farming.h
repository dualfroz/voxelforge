#pragma once

#include <cstdint>
#include <unordered_map>
#include <vector>
#include "World.h"
#include "Math.h"

namespace vf {

enum class CropType : uint8_t {
    None = 0,
    Wheat = 1,
    Carrot = 2,
    Potato = 3,
    PumpkinStem = 4,
    MelonStem = 5,
    SugarCane = 6,
    Cactus = 7,
    Sapling = 8,
    Vine = 9,
};

const char* cropTypeName(CropType type);
int cropMaxStage(CropType type);

struct CropState {
    CropType type = CropType::None;
    int stage = 0;
    int maxStage = 0;

    bool mature() const { return stage >= maxStage; }
};

struct HarvestDrop {
    Block item;
    int count;
};

struct FarmingConfig {
    int hydrationRadius = 4;
    int minGrowthLight = 9;
    uint32_t growthDenominator = 6;
    uint32_t saplingDenominator = 8;
    uint32_t verticalDenominator = 6;
    int caneMaxHeight = 3;
    int cactusMaxHeight = 3;
    int vineMaxLength = 4;
    int treeTrunkMin = 4;
    int treeTrunkMax = 6;
    int bonemealMinStages = 2;
    int bonemealMaxStages = 5;
    uint32_t bonemealSaplingChance = 45;
};

class Farming {
public:
    explicit Farming(uint32_t seed = 1u, const FarmingConfig& config = {});

    void plant(World& world, const Vec3i& pos, CropType type);
    bool randomTick(World& world, const Vec3i& pos, int skyLight);
    int growStep(World& world, int skyLight);

    bool applyBonemeal(World& world, const Vec3i& pos);
    bool trample(World& world, const Vec3i& farmland);

    std::vector<HarvestDrop> harvest(World& world, const Vec3i& pos);

    bool isHydrated(const World& world, const Vec3i& farmland) const;

    const CropState* stateAt(const Vec3i& pos) const;
    std::size_t trackedCount() const { return crops.size(); }

    const FarmingConfig& settings() const { return config; }

private:
    enum class GrowthResult { None, Advanced, Removed };

    uint32_t nextRandom();
    bool rollGrowth(uint32_t denominator, int points);

    GrowthResult growGroundCrop(World& world, const Vec3i& pos, CropState& state, int skyLight);
    GrowthResult growStem(World& world, const Vec3i& pos, CropState& state, int skyLight);
    GrowthResult growColumn(World& world, const Vec3i& pos, CropState& state, bool cane);
    GrowthResult growVine(World& world, const Vec3i& pos, CropState& state);
    GrowthResult growSapling(World& world, const Vec3i& pos, CropState& state, int skyLight);

    void stampTree(World& world, const Vec3i& base);

    FarmingConfig config;
    uint32_t rngState;
    std::unordered_map<Vec3i, CropState, Vec3iHash> crops;
};

}
