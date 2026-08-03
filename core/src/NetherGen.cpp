#include "vf/NetherGen.h"

#include <cmath>
#include <cstdlib>

namespace vf {

namespace {

uint32_t mixCoord(uint32_t seed, int gx, int gy, int gz) {
    uint64_t h = 1469598103934665603ull;
    auto mix = [&h](uint32_t v) {
        h ^= v;
        h *= 1099511628211ull;
    };
    mix(seed);
    mix(static_cast<uint32_t>(gx));
    mix(static_cast<uint32_t>(gy));
    mix(static_cast<uint32_t>(gz));
    h ^= h >> 33;
    return static_cast<uint32_t>(h ^ (h >> 32));
}

bool replaceable(Block block) {
    return block == Block::Air || block == Block::Lava;
}

} // namespace

NetherGenerator::NetherGenerator() : NetherGenerator(Params{}) {}

NetherGenerator::NetherGenerator(const Params& netherParams)
    : params(netherParams),
      floorNoise(netherParams.seed ^ 0x4e455448u),
      ceilingNoise(netherParams.seed ^ 0x524f4f46u),
      caveNoise(netherParams.seed ^ 0x43415645u),
      blobNoise(netherParams.seed ^ 0x424c4f42u),
      oreNoise(netherParams.seed ^ 0x5155415au),
      biomeNoise(netherParams.seed ^ 0x42494f4du) {}

NetherBiome NetherGenerator::biomeAt(int gx, int gz) const {
    float f = params.biomeFrequency;
    float heat = biomeNoise.simplex2D(gx * f, gz * f);
    float damp = biomeNoise.simplex2D(gx * f * 1.7f + 91.3f, gz * f * 1.7f - 44.7f);

    if (damp < -0.30f) return NetherBiome::SoulSandValley;
    if (heat > 0.34f) return NetherBiome::BasaltDeltas;
    if (damp > 0.26f) return NetherBiome::CrimsonForest;
    return NetherBiome::NetherWastes;
}

NetherColumn NetherGenerator::columnAt(int gx, int gz) const {
    NetherColumn column;
    column.biome = biomeAt(gx, gz);
    column.lavaLevel = params.lavaLevel;

    float ff = params.floorFrequency;
    float broad = floorNoise.fbm2D(gx * ff, gz * ff, 4, 2.0f, 0.5f);
    float rough = floorNoise.noise2D(gx * ff * 3.1f + 17.0f, gz * ff * 3.1f - 9.0f);

    int floorCentre = params.lavaLevel + 3;
    float amplitude = 12.0f;
    switch (column.biome) {
        case NetherBiome::SoulSandValley:
            floorCentre -= 8;
            amplitude = 15.0f;
            break;
        case NetherBiome::BasaltDeltas:
            floorCentre -= 3;
            amplitude = 9.0f;
            break;
        case NetherBiome::CrimsonForest:
            floorCentre += 4;
            amplitude = 8.0f;
            break;
        default:
            break;
    }

    float shaped = broad * 0.78f + rough * 0.22f;
    column.floorHeight = floorCentre + static_cast<int>(std::lround(shaped * amplitude));

    float cf = params.ceilingFrequency;
    float roof = ceilingNoise.fbm2D(gx * cf, gz * cf, 3, 2.0f, 0.5f);
    column.ceilingHeight = params.ceilingY - params.roofThickness +
                           static_cast<int>(std::lround(roof * 7.0f));

    int lowest = params.floorY + params.bedrockThickness + 1;
    int highest = params.ceilingY - params.bedrockThickness - 2;
    if (column.floorHeight < lowest) column.floorHeight = lowest;
    if (column.floorHeight > highest - 12) column.floorHeight = highest - 12;
    if (column.ceilingHeight > highest) column.ceilingHeight = highest;
    if (column.ceilingHeight < column.floorHeight + 10) column.ceilingHeight = column.floorHeight + 10;

    column.drowned = column.floorHeight < params.lavaLevel;
    return column;
}

bool NetherGenerator::isCarved(int gx, int gy, int gz) const {
    float f = params.caveFrequency;
    float first = 1.0f - std::fabs(caveNoise.noise3D(gx * f, gy * f * 1.9f, gz * f));
    float second = 1.0f - std::fabs(caveNoise.noise3D(gx * f + 41.3f, gy * f * 1.9f - 17.9f,
                                                      gz * f + 23.1f));
    return first > params.caveThreshold && second > params.caveThreshold;
}

bool NetherGenerator::isBlob(int gx, int gy, int gz) const {
    float broad = blobNoise.noise3D(gx * 0.026f, gy * 0.019f, gz * 0.026f);
    float detail = blobNoise.noise3D(gx * 0.056f + 7.3f, gy * 0.048f - 3.1f, gz * 0.056f + 2.7f);
    return broad + detail * 0.42f > params.blobThreshold;
}

bool NetherGenerator::isBedrock(int gx, int gy, int gz) const {
    if (gy == params.floorY || gy == params.ceilingY - 1) return true;

    int fromFloor = gy - params.floorY;
    int fromCeiling = params.ceilingY - 1 - gy;
    int band = -1;
    if (fromFloor > 0 && fromFloor < params.bedrockThickness) band = fromFloor;
    else if (fromCeiling > 0 && fromCeiling < params.bedrockThickness) band = fromCeiling;
    if (band < 0) return false;

    uint32_t roll = mixCoord(params.seed ^ 0x42454452u, gx, gy, gz);
    return static_cast<int>(roll % static_cast<uint32_t>(params.bedrockThickness)) >= band;
}

Block NetherGenerator::topFor(NetherBiome biome) const {
    switch (biome) {
        case NetherBiome::SoulSandValley: return Block::SoulSand;
        case NetherBiome::BasaltDeltas: return Block::Basalt;
        case NetherBiome::CrimsonForest: return Block::Moss;
        default: return Block::Netherrack;
    }
}

Block NetherGenerator::fillerFor(NetherBiome biome) const {
    switch (biome) {
        case NetherBiome::SoulSandValley: return Block::SoulSoil;
        case NetherBiome::BasaltDeltas: return Block::Blackstone;
        default: return Block::Netherrack;
    }
}

Block NetherGenerator::blockAt(const NetherColumn& column, int gx, int gy, int gz) const {
    if (gy < params.floorY || gy >= params.ceilingY) return Block::Air;
    if (isBedrock(gx, gy, gz)) return Block::Bedrock;

    bool inMass = gy <= column.floorHeight || gy >= column.ceilingHeight;
    bool solid = inMass || isBlob(gx, gy, gz);
    if (solid && isCarved(gx, gy, gz)) solid = false;
    if (!solid) return gy <= params.lavaLevel ? Block::Lava : Block::Air;

    Block material = Block::Netherrack;
    if (gy <= column.floorHeight) {
        int depth = column.floorHeight - gy;
        if (depth == 0) material = topFor(column.biome);
        else if (depth <= 3) material = fillerFor(column.biome);
    }

    float qf = params.quartzFrequency;
    float vein = oreNoise.noise3D(gx * qf, gy * qf, gz * qf);
    if (material == Block::Netherrack) {
        if (vein > params.quartzThreshold) material = Block::NetherQuartzOre;
        else if (vein < -0.78f && column.biome == NetherBiome::BasaltDeltas)
            material = Block::Blackstone;
    }

    if (gy >= column.ceilingHeight && gy <= column.ceilingHeight + 2) {
        float glow = oreNoise.noise3D(gx * 0.09f + 53.1f, gy * 0.09f - 21.7f, gz * 0.09f + 11.9f);
        if (glow > 0.66f) material = Block::Glowstone;
    }

    if (gy >= column.floorHeight - 1 && gy <= column.floorHeight &&
        gy >= params.lavaLevel - 1 && gy <= params.lavaLevel + 2 && vein > 0.30f) {
        material = Block::Magma;
    }

    return material;
}

void NetherGenerator::generateChunk(Chunk& chunk, const Vec3i& chunkCoord) const {
    int ox = chunkCoord.x * chunkSize;
    int oy = chunkCoord.y * chunkSize;
    int oz = chunkCoord.z * chunkSize;

    for (int lx = 0; lx < chunkSize; ++lx) {
        for (int lz = 0; lz < chunkSize; ++lz) {
            NetherColumn column = columnAt(ox + lx, oz + lz);
            for (int ly = 0; ly < chunkSize; ++ly)
                chunk.set(lx, ly, lz, blockAt(column, ox + lx, oy + ly, oz + lz));
        }
    }
}

void NetherGenerator::generateColumn(World& world, int columnX, int columnZ) const {
    for (int cy = 0; cy < stackChunkCount(); ++cy) {
        Chunk& chunk = world.ensureChunk({columnX, cy, columnZ});
        generateChunk(chunk, {columnX, cy, columnZ});
    }
    if (params.decorate) decorateColumn(world, columnX, columnZ);
}

void NetherGenerator::raiseBasaltPillar(World& world, int gx, int gz, const NetherColumn& column,
                                        uint32_t roll) const {
    int base = column.floorHeight + 1;
    if (base <= params.lavaLevel) base = params.lavaLevel + 1;
    int tall = 3 + static_cast<int>((roll >> 4) % 10u);
    int top = base + tall;
    if (top >= column.ceilingHeight) top = column.ceilingHeight - 1;

    for (int y = base; y <= top; ++y) {
        if (!replaceable(world.blockAt(gx, y, gz))) break;
        world.setBlock(gx, y, gz, Block::Basalt);
    }
    if ((roll & 3u) == 0u && replaceable(world.blockAt(gx, base - 1, gz)))
        world.setBlock(gx, base - 1, gz, Block::Magma);
}

void NetherGenerator::pourLavafall(World& world, int gx, int gz, const NetherColumn& column) const {
    int start = column.ceilingHeight - 1;
    for (int y = start; y > params.lavaLevel; --y) {
        if (world.blockAt(gx, y, gz) != Block::Air) break;
        world.setBlock(gx, y, gz, Block::Lava);
    }
    if (world.blockAt(gx, start + 1, gz) == Block::Netherrack)
        world.setBlock(gx, start + 1, gz, Block::Magma);
}

void NetherGenerator::hangGlowstone(World& world, int gx, int gz, const NetherColumn& column,
                                    uint32_t roll) const {
    int drop = 1 + static_cast<int>((roll >> 6) % 3u);
    int spread = static_cast<int>((roll >> 9) % 2u);

    for (int dx = -spread; dx <= spread; ++dx) {
        for (int dz = -spread; dz <= spread; ++dz) {
            int reach = drop - (std::abs(dx) + std::abs(dz));
            for (int step = 0; step < reach; ++step) {
                int y = column.ceilingHeight - 1 - step;
                if (world.blockAt(gx + dx, y, gz + dz) != Block::Air) break;
                world.setBlock(gx + dx, y, gz + dz, Block::Glowstone);
            }
        }
    }
}

void NetherGenerator::plantFungus(World& world, int gx, int gz, const NetherColumn& column,
                                  uint32_t roll) const {
    int base = column.floorHeight + 1;
    if (base <= params.lavaLevel) return;
    if (world.blockAt(gx, base - 1, gz) != Block::Moss) return;

    int stem = 3 + static_cast<int>((roll >> 12) % 5u);
    int top = base + stem - 1;
    if (top + 1 >= column.ceilingHeight) return;

    for (int y = base; y <= top; ++y) {
        if (world.blockAt(gx, y, gz) != Block::Air) return;
        world.setBlock(gx, y, gz, Block::Wood);
    }
    for (int dx = -1; dx <= 1; ++dx) {
        for (int dz = -1; dz <= 1; ++dz) {
            if (std::abs(dx) + std::abs(dz) > 1) continue;
            if (world.blockAt(gx + dx, top + 1, gz + dz) == Block::Air)
                world.setBlock(gx + dx, top + 1, gz + dz, Block::Leaves);
        }
    }
}

void NetherGenerator::decorateColumn(World& world, int columnX, int columnZ) const {
    int ox = columnX * chunkSize;
    int oz = columnZ * chunkSize;

    for (int lx = 3; lx < chunkSize - 3; ++lx) {
        for (int lz = 3; lz < chunkSize - 3; ++lz) {
            int gx = ox + lx;
            int gz = oz + lz;
            NetherColumn column = columnAt(gx, gz);
            uint32_t roll = mixCoord(params.seed ^ 0x4445434fu, gx, 0, gz);

            if (roll % 27u == 0u) hangGlowstone(world, gx, gz, column, roll);

            switch (column.biome) {
                case NetherBiome::BasaltDeltas:
                    if ((roll >> 3) % 23u == 0u) raiseBasaltPillar(world, gx, gz, column, roll);
                    break;
                case NetherBiome::CrimsonForest:
                    if ((roll >> 3) % 17u == 0u) plantFungus(world, gx, gz, column, roll);
                    break;
                default:
                    break;
            }

            if ((roll >> 7) % 97u == 0u) pourLavafall(world, gx, gz, column);
        }
    }
}

} // namespace vf
