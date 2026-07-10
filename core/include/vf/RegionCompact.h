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

// Compaction for a region save container.
//
// A live world rewrites single chunks over and over. A rewrite that still fits
// its sector run is written in place and leaves the unused tail behind; one
// that outgrows it is appended at the end and abandons the old sectors. Both
// are cheap at write time and both waste space, so the file drifts towards a
// layout where most sectors carry nothing.
//
// The compactor measures that drift, rewrites the container back to the tight
// canonical layout, and copies every surviving payload verbatim - the bytes a
// chunk reads back are the bytes it had before, unless repacking was asked for
// explicitly. It refuses to run on a container the scanner calls damaged,
// because moving corrupt records around only spreads the damage.

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

    // Cost in units of work done, not wall clock: one per entry surveyed, one
    // per chunk moved, dropped or repacked, one for the header rewrite.
    std::size_t operations = 0;
};

// Rewrites one chunk that the container already holds, exactly the way a
// running server would: in place while the payload still fits its sector run,
// appended at the end otherwise. Returns false if the chunk is not in the
// table - growing the entry table is a full rewrite, not an update.
bool updateChunkInPlace(ByteBuffer& buffer, const SaveChunkRecord& record, bool compressBody);

class RegionCompactor {
public:
    bool dropEmptyChunks = true;
    bool repackWithStrongerCompression = false;

    std::vector<ChunkFragment> survey(const uint8_t* data, std::size_t size) const;
    std::vector<ChunkFragment> survey(const ByteBuffer& buffer) const;

    // Share of the sectors that a tight rewrite would give back, in percent.
    // Zero for a canonical layout, -1 when the buffer is not a container.
    int fragmentationPercent(const uint8_t* data, std::size_t size) const;
    int fragmentationPercent(const ByteBuffer& buffer) const;

    bool compact(const uint8_t* data, std::size_t size, ByteBuffer& out,
                 CompactionStats& stats) const;
    bool compact(const ByteBuffer& in, ByteBuffer& out, CompactionStats& stats) const;
};

} // namespace vf
