#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "Codec.h"
#include "Math.h"
#include "SaveIntegrity.h"
#include "SaveMigration.h"

namespace vf {

struct ChunkFragment {
    Vec3i coord;
    int entryIndex = 0;
    uint32_t sectorOffset = 0;
    uint32_t sectorCount = 0;
    uint32_t payloadBytes = 0;
    uint32_t neededSectors = 0;
    uint32_t excessSectors = 0;
    uint32_t slackBytes = 0;
    bool empty = false;
    bool reachable = false;
};

struct CompactionStats {
    bool ok = false;
    std::string failure;

    uint32_t sectorBytes = 0;
    uint32_t sectorsBefore = 0;
    uint32_t sectorsAfter = 0;
    uint32_t headerSectors = 0;
    uint32_t referencedSectors = 0;
    uint32_t holeSectors = 0;
    uint32_t excessSectors = 0;
    uint32_t slackBytes = 0;

    int fragmentationBefore = 0;
    int fragmentationAfter = 0;

    std::size_t bytesBefore = 0;
    std::size_t bytesAfter = 0;
    std::size_t bytesReclaimed = 0;

    std::size_t chunksKept = 0;
    std::size_t chunksMoved = 0;
    std::size_t chunksDropped = 0;
    std::size_t chunksRepacked = 0;

    std::size_t operations = 0;
};

bool updateChunkInPlace(ByteBuffer& buffer, const SaveChunkRecord& record, bool compressBody);

class RegionCompactor {
public:
    bool dropEmptyChunks = true;
    bool repackWithStrongerCompression = false;

    std::vector<ChunkFragment> survey(const uint8_t* data, std::size_t size) const;
    std::vector<ChunkFragment> survey(const ByteBuffer& buffer) const;

    int fragmentationPercent(const uint8_t* data, std::size_t size) const;
    int fragmentationPercent(const ByteBuffer& buffer) const;

    bool compact(const uint8_t* data, std::size_t size, ByteBuffer& out,
                 CompactionStats& stats) const;
    bool compact(const ByteBuffer& in, ByteBuffer& out, CompactionStats& stats) const;
};

}
