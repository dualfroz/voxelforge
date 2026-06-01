#include "vf/Terrain.h"

#include <cmath>
#include "vf/Noise.h"

namespace vf {

namespace {

int heightAt(const PerlinNoise& noise, const TerrainParams& params, int gx, int gz) {
    float n = noise.fbm2D(gx * params.frequency, gz * params.frequency, params.octaves, 2.0f, 0.5f);
    float ridged = 1.0f - std::fabs(n);           // sharper ridges near peaks
    float shaped = n * 0.7f + (ridged - 0.5f) * 0.6f;
    int h = params.baseHeight + static_cast<int>(std::round(shaped * params.amplitude));
    if (h < 1) h = 1;
    if (h > params.worldHeight - 1) h = params.worldHeight - 1;
    return h;
}

bool treeAt(const PerlinNoise& noise, int gx, int gz) {
    float t = noise.noise2D(gx * 0.87f + 11.3f, gz * 0.87f - 7.1f);
    return t > 0.82f;
}

void placeTree(World& world, int gx, int baseY, int gz, int worldHeight) {
    int trunk = 4;
    int top = baseY + trunk;
    for (int y = baseY; y <= top && y < worldHeight; ++y) world.setBlock(gx, y, gz, Block::Wood);
    for (int dx = -2; dx <= 2; ++dx) {
        for (int dz = -2; dz <= 2; ++dz) {
            for (int dy = 0; dy <= 2; ++dy) {
                int radius = 2 - dy;
                if (std::abs(dx) + std::abs(dz) > radius + 1) continue;
                int ly = top - 1 + dy;
                if (ly >= worldHeight) continue;
                if (dx == 0 && dz == 0 && ly <= top) continue;
                if (world.blockAt(gx + dx, ly, gz + dz) == Block::Air)
                    world.setBlock(gx + dx, ly, gz + dz, Block::Leaves);
            }
        }
    }
}

} // namespace

int columnHeight(uint32_t seed, const TerrainParams& params, int gx, int gz) {
    PerlinNoise noise(seed);
    return heightAt(noise, params, gx, gz);
}

void generateTerrain(World& world, const TerrainParams& params) {
    PerlinNoise noise(params.seed);
    int blocksX = params.chunksX * chunkSize;
    int blocksZ = params.chunksZ * chunkSize;

    for (int gx = 0; gx < blocksX; ++gx) {
        for (int gz = 0; gz < blocksZ; ++gz) {
            int h = heightAt(noise, params, gx, gz);
            for (int y = 0; y <= h; ++y) {
                Block b;
                if (y == 0) b = Block::Stone;
                else if (y < h - 3) b = Block::Stone;
                else if (y < h) b = Block::Dirt;
                else if (h <= params.seaLevel + 1) b = Block::Sand;
                else if (h > params.worldHeight - 8) b = Block::Snow;
                else b = Block::Grass;
                world.setBlock(gx, y, gz, b);
            }
            for (int y = h + 1; y <= params.seaLevel; ++y)
                world.setBlock(gx, y, gz, Block::Water);

            if (params.trees && h > params.seaLevel + 1 && h < params.worldHeight - 10 &&
                treeAt(noise, gx, gz)) {
                placeTree(world, gx, h + 1, gz, params.worldHeight);
            }
        }
    }
}

} // namespace vf
