#pragma once

#include <deque>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "Gpu.h"
#include "Sky.h"
#include "vf/Frustum.h"
#include "vf/Lighting.h"
#include "vf/Physics.h"
#include "vf/Streaming.h"
#include "vf/WorldGen.h"

namespace vf {

class TerrainView {
public:
    TerrainView(World& world, const WorldGen& generator, const StreamingConfig& streaming);

    void generateNow(ColumnCoord column);
    void pin(ColumnCoord column) { pinned.insert(column); }
    void recentre(ColumnCoord centre);
    void update(ColumnCoord centre);

    void editBlock(int gx, int gy, int gz, Block block);
    void markCellChanged(int gx, int gy, int gz);
    void reloadAll();

    void setSkyLevel(int level);
    int skyLevel() const { return skyLight; }

    bool columnReady(ColumnCoord column) const;
    const LightField& rawLight(ColumnCoord column);

    void initGpu();
    void drawOpaque(const SceneLook& look, const Vec3& eye);
    void drawTransparent();

    std::size_t residentColumns() const { return streamer.loadedCount(); }
    std::size_t meshedChunks() const { return gpuChunks.size(); }
    std::size_t pendingMeshes() const {
        return meshQueue.size() + (relightQueue.size() + genQueue.size()) * stackHeight;
    }
    int loadRadius() const { return streamer.settings().loadRadius; }

private:
    struct GpuChunk {
        GpuMesh opaque;
        GpuMesh transparent;
        Aabb bounds;
        Vec3 center;
    };

    void enqueueMesh(const Vec3i& coord);
    void enqueueColumn(ColumnCoord column);
    void meshChunk(const Vec3i& coord);
    void removeChunkGpu(const Vec3i& coord);
    void unloadColumn(ColumnCoord column);
    void forgetLight(ColumnCoord column);
    const LightField& scaledLight(ColumnCoord column);

    World& world;
    const WorldGen& generator;
    ChunkStreamer streamer;
    int stackHeight;

    std::deque<ColumnCoord> genQueue;
    std::deque<Vec3i> meshQueue;
    std::unordered_set<Vec3i, Vec3iHash> queuedMeshes;
    std::deque<ColumnCoord> relightQueue;
    std::unordered_map<Vec3i, GpuChunk, Vec3iHash> gpuChunks;
    std::unordered_set<ColumnCoord, ColumnCoordHash> pinned;

    std::unordered_map<ColumnCoord, LightField, ColumnCoordHash> lightCache;
    LightField scaledCache;
    ColumnCoord scaledColumn{};
    int scaledLevel = -1;
    int skyLight = maxLight;

    ColumnCoord lastCentre{2147483647, 2147483647};
    std::vector<const GpuChunk*> visibleTransparent;

    GLuint program = 0;
    GLuint atlasTexture = 0;
    GLint uViewProj = -1, uAtlas = -1, uSunDir = -1, uSunStrength = -1, uFogColor = -1, uFogRange = -1;
};

}
