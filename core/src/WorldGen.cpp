#include "vf/WorldGen.h"

#include <cmath>

#include "vf/Structures.h"

namespace vf {

namespace {

struct BiomeShape {
    int base;
    float amplitude;
};

BiomeShape shapeFor(Biome biome) {
    switch (biome) {
        case Biome::Mountains: return {40, 46.0f};
        case Biome::Snow: return {32, 22.0f};
        case Biome::Desert: return {28, 8.0f};
        case Biome::Forest: return {30, 14.0f};
        default: return {30, 11.0f};
    }
}

void placeTree(World& world, int gx, int baseY, int gz, int maxY, bool tall) {
    int trunk = tall ? 6 : 4;
    int top = baseY + trunk;
    for (int y = baseY; y <= top && y < maxY; ++y) world.setBlock(gx, y, gz, Block::Wood);
    for (int dx = -2; dx <= 2; ++dx) {
        for (int dz = -2; dz <= 2; ++dz) {
            for (int dy = 0; dy <= 2; ++dy) {
                int radius = 2 - dy;
                if (std::abs(dx) + std::abs(dz) > radius + 1) continue;
                int ly = top - 1 + dy;
                if (ly >= maxY) continue;
                if (dx == 0 && dz == 0 && ly <= top) continue;
                if (world.blockAt(gx + dx, ly, gz + dz) == Block::Air)
                    world.setBlock(gx + dx, ly, gz + dz, Block::Leaves);
            }
        }
    }
}

}

WorldGen::WorldGen(const WorldGenParams& params)
    : params(params),
      heightNoise(params.seed),
      tempNoise(params.seed ^ 0x9e3779b9u),
      humidNoise(params.seed ^ 0x85ebca6bu),
      mountainNoise(params.seed ^ 0xc2b2ae35u),
      caveNoise(params.seed ^ 0x27d4eb2fu),
      oreNoise(params.seed ^ 0x165667b1u),
      treeNoise(params.seed ^ 0xd3a2646cu) {}

Biome WorldGen::climateBiome(int gx, int gz) const {
    float temp = tempNoise.noise2D(gx * params.biomeFrequency, gz * params.biomeFrequency);
    float humid = humidNoise.noise2D(gx * params.biomeFrequency * 1.3f,
                                     gz * params.biomeFrequency * 1.3f);
    float mountain = mountainNoise.noise2D(gx * params.biomeFrequency * 0.7f,
                                           gz * params.biomeFrequency * 0.7f);
    if (mountain > 0.55f) return Biome::Mountains;
    if (temp < -0.35f) return Biome::Snow;
    if (temp > 0.4f && humid < -0.1f) return Biome::Desert;
    if (humid > 0.2f) return Biome::Forest;
    return Biome::Plains;
}

int WorldGen::surfaceHeight(int gx, int gz) const {
    BiomeShape shape = shapeFor(climateBiome(gx, gz));
    float n = heightNoise.fbm2D(gx * params.heightFrequency, gz * params.heightFrequency,
                                params.heightOctaves, 2.0f, 0.5f);
    float ridged = 1.0f - std::fabs(n);
    float shaped = n * 0.7f + (ridged - 0.5f) * 0.6f;
    int h = shape.base + static_cast<int>(std::round(shaped * shape.amplitude));
    if (h < 1) h = 1;
    if (h > params.maxY - 1) h = params.maxY - 1;
    return h;
}

Biome WorldGen::biomeAt(int gx, int gz) const {
    int h = surfaceHeight(gx, gz);
    if (h < params.seaLevel - 1) return Biome::Ocean;
    if (h <= params.seaLevel + 1) return Biome::Beach;
    return climateBiome(gx, gz);
}

bool WorldGen::isCave(int gx, int gy, int gz) const {
    float f = params.caveFrequency;
    float c1 = 1.0f - std::fabs(caveNoise.noise3D(gx * f, gy * f * 2.0f, gz * f));
    float c2 = 1.0f - std::fabs(caveNoise.noise3D(gx * f + 31.7f, gy * f * 2.0f + 13.1f,
                                                  gz * f - 27.3f));
    return c1 > params.caveThreshold && c2 > params.caveThreshold;
}

Block WorldGen::topBlock(Biome biome, int height) const {
    switch (biome) {
        case Biome::Ocean:
        case Biome::Beach:
        case Biome::Desert:
            return Block::Sand;
        case Biome::Snow:
            return Block::Snow;
        case Biome::Mountains:
            return height > 42 ? Block::Snow : Block::Grass;
        default:
            return Block::Grass;
    }
}

void WorldGen::generateChunk(Chunk& chunk, const Vec3i& chunkCoord) const {
    int ox = chunkCoord.x * chunkSize;
    int oy = chunkCoord.y * chunkSize;
    int oz = chunkCoord.z * chunkSize;

    for (int lx = 0; lx < chunkSize; ++lx) {
        for (int lz = 0; lz < chunkSize; ++lz) {
            int gx = ox + lx;
            int gz = oz + lz;
            Biome biome = biomeAt(gx, gz);
            int h = surfaceHeight(gx, gz);
            bool sandy = biome == Biome::Beach || biome == Biome::Ocean || biome == Biome::Desert;

            for (int ly = 0; ly < chunkSize; ++ly) {
                int gy = oy + ly;
                Block block = Block::Air;

                if (gy == 0) {
                    block = Block::Stone;
                } else if (gy > h) {
                    block = gy <= params.seaLevel ? Block::Water : Block::Air;
                } else if (gy == h) {
                    block = (h <= params.seaLevel) ? (sandy ? Block::Sand : Block::Dirt)
                                                   : topBlock(biome, h);
                } else if (gy >= h - 3) {
                    block = sandy ? Block::Sand : Block::Dirt;
                } else {
                    block = Block::Stone;
                }

                if (block == Block::Stone && gy < h - 3 && gy >= 1) {
                    float o = oreNoise.noise3D(gx * params.oreFrequency, gy * params.oreFrequency,
                                               gz * params.oreFrequency);
                    if (o > 0.72f) {
                        if (gy < 14) block = Block::GoldOre;
                        else if (gy < 40) block = Block::IronOre;
                        else block = Block::CoalOre;
                    } else if (o < -0.78f) {
                        block = Block::Gravel;
                    }
                }

                if (gy >= 2 && gy < h - 2 && isSolid(block) && isCave(gx, gy, gz))
                    block = Block::Air;

                chunk.set(lx, ly, lz, block);
            }
        }
    }
}

void WorldGen::generateColumn(World& world, int columnX, int columnZ) const {
    for (int cy = 0; cy < stackChunkCount(); ++cy) {
        Chunk& chunk = world.ensureChunk({columnX, cy, columnZ});
        generateChunk(chunk, {columnX, cy, columnZ});
    }

    if (!params.trees) return;
    int ox = columnX * chunkSize;
    int oz = columnZ * chunkSize;
    for (int lx = 2; lx < chunkSize - 2; ++lx) {
        for (int lz = 2; lz < chunkSize - 2; ++lz) {
            int gx = ox + lx;
            int gz = oz + lz;
            Biome biome = biomeAt(gx, gz);
            if (biome != Biome::Forest && biome != Biome::Plains) continue;
            int h = surfaceHeight(gx, gz);
            if (h <= params.seaLevel + 1 || h >= params.maxY - 10) continue;
            if (world.blockAt(gx, h, gz) != Block::Grass) continue;
            float density = biome == Biome::Forest ? 0.55f : 0.68f;
            float t = treeNoise.noise2D(gx * 0.87f + 11.3f, gz * 0.87f - 7.1f);
            if (t > density) placeTree(world, gx, h + 1, gz, params.maxY, biome == Biome::Forest);
        }
    }

    placeStructures(world, columnX, columnZ);
}

void WorldGen::placeStructures(World& world, int columnX, int columnZ) const {
    if (!params.structures) return;

    uint32_t h = structureHash(params.seed ^ 0x51ed270bu, columnX, 0, columnZ);
    int lx = 2 + static_cast<int>(h % 12u);
    int lz = 2 + static_cast<int>((h >> 8) % 12u);
    int gx = columnX * chunkSize + lx;
    int gz = columnZ * chunkSize + lz;

    int surface = surfaceHeight(gx, gz);
    if (surface <= params.seaLevel + 1 || surface >= params.maxY - 14) return;

    Block top = world.blockAt(gx, surface, gz);
    if (top != Block::Grass && top != Block::Snow) return;

    Biome biome = biomeAt(gx, gz);
    uint32_t roll = (h >> 16) % 100u;
    StructureKind kind;
    bool place = false;

    if (biome == Biome::Forest || biome == Biome::Plains) {
        if (roll < 5) {
            kind = StructureKind::Hut;
            place = true;
        } else if (roll < 45) {
            kind = StructureKind::OakTree;
            place = true;
        }
    } else if (biome == Biome::Snow || biome == Biome::Mountains) {
        if (roll < 4) {
            kind = StructureKind::Hut;
            place = true;
        } else if (roll < 35) {
            kind = StructureKind::PineTree;
            place = true;
        }
    }
    if (!place) return;

    uint32_t structSeed = structureHash(params.seed, columnX, 1, columnZ);
    std::vector<StructureEdit> edits = buildStructure(kind, structSeed);
    stampStructure(world, {gx, surface + 1, gz}, edits, params.maxY);
}

}
