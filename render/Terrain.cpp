#include "Terrain.h"

#include <algorithm>

#include "Shaders.h"
#include "vf/LitMesh.h"

namespace vf {

namespace {

constexpr int lightMargin = 2;
constexpr int generateBudget = 2;
constexpr int meshBudget = 6;
constexpr int relightBudget = 1;

const ColumnCoord sideOffsets[4] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};

}

TerrainView::TerrainView(World& world, const WorldGen& generator, const StreamingConfig& streaming)
    : world(world), generator(generator), streamer(streaming),
      stackHeight(generator.stackChunkCount()) {}

bool TerrainView::columnReady(ColumnCoord column) const {
    return world.chunkAt({column.x, 0, column.z}) != nullptr;
}

void TerrainView::forgetLight(ColumnCoord column) {
    lightCache.erase(column);
    if (scaledLevel >= 0 && scaledColumn == column) scaledLevel = -1;
}

const LightField& TerrainView::rawLight(ColumnCoord column) {
    auto it = lightCache.find(column);
    if (it != lightCache.end()) return it->second;
    Vec3i origin{column.x * chunkSize - lightMargin, 0, column.z * chunkSize - lightMargin};
    Vec3i dims{chunkSize + 2 * lightMargin, stackHeight * chunkSize, chunkSize + 2 * lightMargin};
    return lightCache.emplace(column, computeLight(world, origin, dims)).first->second;
}

const LightField& TerrainView::scaledLight(ColumnCoord column) {
    if (scaledLevel == skyLight && scaledColumn == column) return scaledCache;
    scaledCache = rawLight(column);
    for (uint8_t& sun : scaledCache.sun)
        sun = static_cast<uint8_t>((sun * skyLight + maxLight / 2) / maxLight);
    scaledColumn = column;
    scaledLevel = skyLight;
    return scaledCache;
}

void TerrainView::setSkyLevel(int level) {
    if (level == skyLight) return;
    skyLight = level;
    relightQueue.clear();
    std::unordered_set<ColumnCoord, ColumnCoordHash> columns;
    for (const auto& entry : gpuChunks) columns.insert({entry.first.x, entry.first.z});
    std::vector<ColumnCoord> ordered(columns.begin(), columns.end());
    ColumnCoord centre = lastCentre;
    std::sort(ordered.begin(), ordered.end(), [&](ColumnCoord a, ColumnCoord b) {
        int da = (a.x - centre.x) * (a.x - centre.x) + (a.z - centre.z) * (a.z - centre.z);
        int db = (b.x - centre.x) * (b.x - centre.x) + (b.z - centre.z) * (b.z - centre.z);
        return da < db;
    });
    relightQueue.assign(ordered.begin(), ordered.end());
}

void TerrainView::removeChunkGpu(const Vec3i& coord) {
    auto it = gpuChunks.find(coord);
    if (it == gpuChunks.end()) return;
    freeGpuMesh(it->second.opaque);
    freeGpuMesh(it->second.transparent);
    gpuChunks.erase(it);
}

void TerrainView::meshChunk(const Vec3i& coord) {
    ChunkMesh mesh = meshChunkLit(world, coord, scaledLight({coord.x, coord.z}));
    if (mesh.empty()) {
        removeChunkGpu(coord);
        return;
    }
    GpuChunk& gpu = gpuChunks[coord];
    uploadChunkMesh(gpu.opaque, mesh.opaque);
    uploadChunkMesh(gpu.transparent, mesh.transparent);
    float ox = static_cast<float>(coord.x * chunkSize);
    float oy = static_cast<float>(coord.y * chunkSize);
    float oz = static_cast<float>(coord.z * chunkSize);
    gpu.bounds = Aabb{{ox, oy, oz}, {ox + chunkSize, oy + chunkSize, oz + chunkSize}};
    gpu.center = Vec3{ox + chunkSize * 0.5f, oy + chunkSize * 0.5f, oz + chunkSize * 0.5f};
}

void TerrainView::enqueueMesh(const Vec3i& coord) {
    if (queuedMeshes.insert(coord).second) meshQueue.push_back(coord);
}

void TerrainView::enqueueColumn(ColumnCoord column) {
    for (int cy = 0; cy < stackHeight; ++cy) enqueueMesh({column.x, cy, column.z});
}

void TerrainView::generateNow(ColumnCoord column) {
    if (!columnReady(column)) generator.generateColumn(world, column.x, column.z);
    forgetLight(column);
    enqueueColumn(column);
    for (ColumnCoord offset : sideOffsets) {
        ColumnCoord side{column.x + offset.x, column.z + offset.z};
        if (!columnReady(side)) continue;
        forgetLight(side);
        enqueueColumn(side);
    }
}

void TerrainView::unloadColumn(ColumnCoord column) {
    for (int cy = 0; cy < stackHeight; ++cy) {
        Vec3i coord{column.x, cy, column.z};
        removeChunkGpu(coord);
        if (!pinned.count(column)) world.removeChunk(coord);
    }
    forgetLight(column);
}

void TerrainView::recentre(ColumnCoord centre) {
    if (centre == lastCentre) return;
    lastCentre = centre;
    StreamDelta delta = streamer.update(centre);
    for (ColumnCoord column : delta.load) genQueue.push_back(column);
    for (ColumnCoord column : delta.unload) unloadColumn(column);
}

void TerrainView::update(ColumnCoord centre) {
    recentre(centre);

    int budget = generateBudget;
    while (budget > 0 && !genQueue.empty()) {
        ColumnCoord column = genQueue.front();
        genQueue.pop_front();
        if (!streamer.isLoaded(column)) continue;
        generateNow(column);
        --budget;
    }

    budget = meshBudget;
    while (budget-- > 0 && !meshQueue.empty()) {
        Vec3i coord = meshQueue.front();
        meshQueue.pop_front();
        queuedMeshes.erase(coord);
        if (world.chunkAt(coord)) meshChunk(coord);
    }

    budget = relightBudget;
    while (budget > 0 && !relightQueue.empty() && meshQueue.size() < 4) {
        ColumnCoord column = relightQueue.front();
        relightQueue.pop_front();
        if (!streamer.isLoaded(column) || !columnReady(column)) continue;
        for (int cy = 0; cy < stackHeight; ++cy) {
            Vec3i coord{column.x, cy, column.z};
            if (gpuChunks.count(coord)) meshChunk(coord);
        }
        --budget;
    }
}

void TerrainView::markCellChanged(int gx, int gy, int gz) {
    static const int offsets[7][3] = {{0, 0, 0},  {1, 0, 0},  {-1, 0, 0}, {0, 1, 0},
                                      {0, -1, 0}, {0, 0, 1},  {0, 0, -1}};
    for (const auto& o : offsets) {
        Vec3i coord{floorDiv(gx + o[0], chunkSize), floorDiv(gy + o[1], chunkSize),
                    floorDiv(gz + o[2], chunkSize)};
        if (coord.y < 0 || coord.y >= stackHeight) continue;
        if (!world.chunkAt(coord)) continue;
        forgetLight({coord.x, coord.z});
        enqueueMesh(coord);
    }
    int cx = floorDiv(gx, chunkSize), cz = floorDiv(gz, chunkSize);
    for (ColumnCoord offset : sideOffsets) {
        int lx = gx - (cx + offset.x) * chunkSize;
        int lz = gz - (cz + offset.z) * chunkSize;
        if (lx >= -lightMargin && lx < chunkSize + lightMargin && lz >= -lightMargin &&
            lz < chunkSize + lightMargin)
            forgetLight({cx + offset.x, cz + offset.z});
    }
}

void TerrainView::editBlock(int gx, int gy, int gz, Block block) {
    world.setBlock(gx, gy, gz, block);
    markCellChanged(gx, gy, gz);
}

void TerrainView::reloadAll() {
    for (auto& entry : gpuChunks) {
        freeGpuMesh(entry.second.opaque);
        freeGpuMesh(entry.second.transparent);
    }
    gpuChunks.clear();
    meshQueue.clear();
    queuedMeshes.clear();
    relightQueue.clear();
    lightCache.clear();
    scaledLevel = -1;
    for (const auto& entry : world.allChunks()) enqueueMesh(entry.first);
}

void TerrainView::initGpu() {
    program = buildProgram(terrainVertexShaderSource(), terrainFragmentShaderSource());
    uViewProj = glGetUniformLocation(program, "uViewProj");
    uAtlas = glGetUniformLocation(program, "uAtlas");
    uSunDir = glGetUniformLocation(program, "uSunDir");
    uSunStrength = glGetUniformLocation(program, "uSunStrength");
    uFogColor = glGetUniformLocation(program, "uFogColor");
    uFogRange = glGetUniformLocation(program, "uFogRange");
    atlasTexture = uploadAtlasTexture(16);
}

void TerrainView::drawOpaque(const SceneLook& look, const Vec3& eye) {
    glUseProgram(program);
    glUniformMatrix4fv(uViewProj, 1, GL_FALSE, look.viewProj.m);
    glUniform3f(uSunDir, look.sunDir.x, look.sunDir.y, look.sunDir.z);
    glUniform1f(uSunStrength, look.sunStrength);
    glUniform3f(uFogColor, look.fogColor.x, look.fogColor.y, look.fogColor.z);
    glUniform2f(uFogRange, look.fogStart, look.fogEnd);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D_ARRAY, atlasTexture);
    glUniform1i(uAtlas, 0);

    Frustum frustum = Frustum::fromMatrix(look.viewProj);
    visibleTransparent.clear();
    for (const auto& entry : gpuChunks) {
        const GpuChunk& gpu = entry.second;
        if (!frustum.intersectsAabb(gpu.bounds)) continue;
        drawGpuMesh(gpu.opaque);
        if (gpu.transparent.indexCount > 0) visibleTransparent.push_back(&gpu);
    }
    std::sort(visibleTransparent.begin(), visibleTransparent.end(),
              [&](const GpuChunk* a, const GpuChunk* b) {
                  Vec3 da = a->center - eye;
                  Vec3 db = b->center - eye;
                  return dot(da, da) > dot(db, db);
              });
}

void TerrainView::drawTransparent() {
    glUseProgram(program);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);
    glDisable(GL_CULL_FACE);
    for (const GpuChunk* gpu : visibleTransparent) drawGpuMesh(gpu->transparent);
    glDepthMask(GL_TRUE);
    glEnable(GL_CULL_FACE);
    glDisable(GL_BLEND);
}

}
